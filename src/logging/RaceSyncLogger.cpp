#include "RaceSyncLogger.h"
#include "../config/RaceSyncConfig.h"

bool RaceSyncLogger::begin(RaceSyncStorage& storage) { _storage = &storage; return storage.ready(); }
bool RaceSyncLogger::recording() const { return _recording; }
bool RaceSyncLogger::manualSession() const { return _recording && _manualSession; }
const String& RaceSyncLogger::currentFilename() const { return _filename; }
uint32_t RaceSyncLogger::sampleCount() const { return _sampleCount; }
uint32_t RaceSyncLogger::storageWriteErrors() const { return _writeErrors; }
uint32_t RaceSyncLogger::lastWriteAgeMs() const { return _lastWriteMs == 0 ? UINT32_MAX : millis() - _lastWriteMs; }
uint32_t RaceSyncLogger::recordingSeconds() const { return (_recording && _startedMs) ? (millis() - _startedMs) / 1000 : 0; }

String RaceSyncLogger::createFilename(const Telemetry& t, DataMode mode) const
{
    char b[64];
    if (t.timeValid && t.year >= 2024)
        snprintf(b, sizeof(b), "RS_%04u-%02u-%02u_%02u-%02u-%02u.vbo", t.year,t.month,t.day,t.hour,t.minute,t.second);
    else
        snprintf(b, sizeof(b), "RS_LIVE_%010lu.vbo", (unsigned long)millis());
    return String(b);
}

void RaceSyncLogger::writeHeader(File& f, DataMode mode)
{
    f.println("[header]");
    f.println("satellites\ntime\nlatitude\nlongitude\nvelocity kmh\nheading\nheight\nvertical velocity m/s\nsampleperiod\nsolution type\navifileindex\navisynctime\nComboAcc\nADC3 Oil Pressure\nADC2 Oil Temp\nADC1 Water Temp\nRevs\nADC4 Fuel Pressure\nCombo_G");
    f.println(); f.println("[comments]"); f.println("RaceSync ESP32 Logger"); f.print("Source: "); f.println(dataModeName(mode));
    f.println(); f.println("[column names]");
    f.println("sats time lat long velocity heading height vert-vel Tsample solution_type avifileindex avitime ComboAcc ADC3_Oil_Pressure ADC2_Oil_Temp ADC1_Water_Temp Revs ADC4_Fuel_Pressure Combo_G");
    f.println(); f.println("[data]");
}

void RaceSyncLogger::writeKmlHeader(File& f, const String& name)
{
    f.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    f.println("<kml xmlns=\"http://www.opengis.net/kml/2.2\">");
    f.println("<Document>");
    f.print("<name>"); f.print(name); f.println("</name>");
    f.println("<Style id=\"RaceSyncTrack\"><LineStyle><width>4</width></LineStyle></Style>");
    f.println("<Placemark>");
    f.println("<name>RaceSync GPS Track</name>");
    f.println("<styleUrl>#RaceSyncTrack</styleUrl>");
    f.println("<LineString><tessellate>1</tessellate><altitudeMode>absolute</altitudeMode><coordinates>");
}

void RaceSyncLogger::writeKmlFooter(File& f)
{
    f.println("</coordinates></LineString>");
    f.println("</Placemark>");
    f.println("</Document>");
    f.println("</kml>");
}

String RaceSyncLogger::createVBoxLine(const Telemetry& t) const
{
    char line[512];
    snprintf(line,sizeof(line),"%03u %010.3f %+014.8f %+014.8f %07.3f %07.3f %+09.2f %+08.2f %.3f %02u %04d %09.0f %+.6E %+.6E %+.6E %+.6E %+.6E %+.6E %+.6E",
        t.satellites,t.rawTime,t.rawLatitude,t.rawLongitude,t.velocityKmh,t.heading,t.height,t.verticalVelocityMs,t.samplePeriod,t.solutionType,t.aviFileIndex,t.aviTime,t.comboAcc,t.oilPressure,t.oilTemperature,t.waterTemperature,t.revs,t.fuelPressure,t.comboG);
    return String(line);
}

bool RaceSyncLogger::storageHasSafeFreeSpace()
{
    if (!_storage || !_storage->ready()) return false;
    uint32_t now=millis(); if (_lastStorageCheckMs && now-_lastStorageCheckMs<1000) return true;
    _lastStorageCheckMs=now; uint64_t freeBytes=_storage->freeBytes();
    if (freeBytes < RaceSyncConfig::MIN_FREE_STORAGE_BYTES) {
        Serial.printf("[LOGGER] Low storage - %lu KB free. Closing session.\n",(unsigned long)(freeBytes/1024)); return false;
    }
    return true;
}

bool RaceSyncLogger::start(const Telemetry& t, DataMode mode, bool manual)
{
    if (_recording) {
        if (manual) {
            _manualSession = true;
            _autoStartInhibit = false;
            Serial.println("[LOGGER] Existing recording switched to manual control");
        }
        return true;
    }
    if (!_storage || !_storage->ready()) { Serial.println("[LOGGER] Storage not ready"); return false; }
    if (_storage->freeBytes() < RaceSyncConfig::MIN_FREE_STORAGE_BYTES) { Serial.println("[LOGGER] Insufficient free storage - recording not started"); return false; }

    _filename=createFilename(t,mode);
    _kmlFilename=_filename; _kmlFilename.replace(".vbo",".kml");
    _file=_storage->openWrite(_filename);
    if (!_file) { _writeErrors++; Serial.println("[LOGGER] Unable to create VBO session"); return false; }
    _kmlFile=_storage->openFileWrite(_kmlFilename);
    if (!_kmlFile) { _writeErrors++; _file.close(); Serial.println("[LOGGER] Unable to create companion KML"); return false; }

    writeHeader(_file,mode); writeKmlHeader(_kmlFile,_kmlFilename); _file.flush(); _kmlFile.flush();
    _sampleCount=0; _belowSpeedSince=0; _lastFlush=millis(); _lastWriteMs=0; _startedMs=millis(); _lastStorageCheckMs=0; _recording=true; _manualSession=manual; _autoStartInhibit=false;
    Serial.printf("[LOGGER] Started: %s (%s)\n",_filename.c_str(), manual ? "MANUAL" : "AUTO");
    Serial.printf("[LOGGER] KML: %s\n",_kmlFilename.c_str());
    return true;
}

void RaceSyncLogger::stop()
{
    if (!_recording) return;
    if (_file) { _file.flush(); _file.close(); }
    if (_kmlFile) { writeKmlFooter(_kmlFile); _kmlFile.flush(); _kmlFile.close(); }
    _recording=false; _manualSession=false; _belowSpeedSince=0;
    Serial.printf("[LOGGER] Closed: %s samples=%u\n",_filename.c_str(),_sampleCount);
    Serial.printf("[LOGGER] Closed KML: %s\n",_kmlFilename.c_str());
}

bool RaceSyncLogger::manualStart(const Telemetry& telemetry, DataMode mode)
{
    if (!telemetry.valid) {
        Serial.println("[LOGGER] Manual start rejected - GPS fix not valid");
        return false;
    }
    return start(telemetry, mode, true);
}

bool RaceSyncLogger::manualStop()
{
    if (!_recording) return false;
    _autoStartInhibit = true;
    stop();
    _autoStartInhibit = true;
    Serial.println("[LOGGER] Manual stop - automatic restart inhibited until speed drops below start threshold");
    return true;
}

void RaceSyncLogger::forceStop() { stop(); }

void RaceSyncLogger::writeKmlSample(const Telemetry& t)
{
    _kmlFile.print(t.longitude, 7); _kmlFile.print(',');
    _kmlFile.print(t.latitude, 7); _kmlFile.print(',');
    _kmlFile.println(t.height, 2);
}

void RaceSyncLogger::writeSample(const Telemetry& t)
{
    if (!_recording) return;
    if (!storageHasSafeFreeSpace()) { stop(); return; }
    size_t vboWritten=_file.println(createVBoxLine(t));
    if (vboWritten==0) { _writeErrors++; Serial.println("[LOGGER] VBO write failed - closing session"); stop(); return; }
    writeKmlSample(t);
    if (!_kmlFile) { _writeErrors++; Serial.println("[LOGGER] KML write failed - closing session"); stop(); return; }
    _sampleCount++; _lastWriteMs=millis(); uint32_t now=millis();
    if (now-_lastFlush >= RaceSyncConfig::LOG_FLUSH_INTERVAL_MS) { _file.flush(); _kmlFile.flush(); _lastFlush=now; }
}

void RaceSyncLogger::processSample(const Telemetry& t, DataMode mode)
{
    if (!t.valid) return;

    if (_recording && _manualSession) {
        writeSample(t);
        return;
    }

    if (_autoStartInhibit) {
        if (t.velocityKmh < RaceSyncConfig::LOG_START_SPEED_KMH) {
            _autoStartInhibit = false;
            Serial.println("[LOGGER] Automatic start re-enabled");
        } else {
            return;
        }
    }

    if (!_recording && t.velocityKmh >= RaceSyncConfig::LOG_START_SPEED_KMH) start(t,mode,false);
    if (!_recording) return;
    writeSample(t); if (!_recording) return;
    if (t.velocityKmh <= RaceSyncConfig::LOG_STOP_SPEED_KMH) {
        if (_belowSpeedSince==0) _belowSpeedSince=millis();
        if (millis()-_belowSpeedSince >= RaceSyncConfig::LOG_STOP_DELAY_MS) stop();
    } else _belowSpeedSince=0;
}
