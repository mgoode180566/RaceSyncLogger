#include "RaceSyncLogger.h"
#include "../config/RaceSyncConfig.h"

namespace
{
    constexpr uint32_t GPS_STALE_THRESHOLD_MS = 1000;
}

void RaceSyncLogger::loadAutomaticSettings()
{
    _settingsPreferences.begin("racesync", false);
    _startSpeedKmh = _settingsPreferences.getDouble("logStartKmh", RaceSyncConfig::LOG_START_SPEED_KMH);
    _stopSpeedKmh = RaceSyncConfig::LOG_STOP_SPEED_KMH;
    _stopDelayMs = _settingsPreferences.getUInt("logStopDelay", RaceSyncConfig::LOG_STOP_DELAY_MS);

    if (_startSpeedKmh < 1.0 || _startSpeedKmh > 100.0) _startSpeedKmh = RaceSyncConfig::LOG_START_SPEED_KMH;
    if (_stopDelayMs < 1000 || _stopDelayMs > 600000) _stopDelayMs = RaceSyncConfig::LOG_STOP_DELAY_MS;

    Serial.printf("[LOGGER] Auto settings: start %.1f km/h, stop <= %.1f km/h for %lu s\n",
                  _startSpeedKmh, _stopSpeedKmh, (unsigned long)(_stopDelayMs / 1000));
}

bool RaceSyncLogger::begin(RaceSyncStorage& storage)
{
    _storage = &storage;
    loadAutomaticSettings();
    return storage.ready() && storage.writable();
}

bool RaceSyncLogger::recording() const { return _recording; }
bool RaceSyncLogger::manualSession() const { return _recording && _manualSession; }
const String& RaceSyncLogger::currentFilename() const { return _filename; }
const String& RaceSyncLogger::currentLogFilename() const { return _logFilename; }
uint32_t RaceSyncLogger::sampleCount() const { return _sampleCount; }
uint32_t RaceSyncLogger::storageWriteErrors() const { return _writeErrors; }
uint32_t RaceSyncLogger::lastWriteAgeMs() const { return _lastWriteMs == 0 ? UINT32_MAX : millis() - _lastWriteMs; }
uint32_t RaceSyncLogger::recordingSeconds() const { return (_recording && _startedMs) ? (millis() - _startedMs) / 1000 : 0; }
double RaceSyncLogger::startSpeedKmh() const { return _startSpeedKmh; }
double RaceSyncLogger::stopSpeedKmh() const { return _stopSpeedKmh; }
uint32_t RaceSyncLogger::stopDelaySeconds() const { return _stopDelayMs / 1000; }
const String& RaceSyncLogger::lastStopReason() const { return _lastStopReason; }
const String& RaceSyncLogger::lastSessionFilename() const { return _lastSessionFilename; }
const String& RaceSyncLogger::lastSessionLogFilename() const { return _lastSessionLogFilename; }
uint64_t RaceSyncLogger::lastSessionFileSizeBytes() const { return _lastSessionFileSizeBytes; }
uint32_t RaceSyncLogger::lastSessionSamples() const { return _lastSessionSamples; }
uint32_t RaceSyncLogger::lastSessionDurationSeconds() const { return _lastSessionDurationSeconds; }
double RaceSyncLogger::lastStopSpeedKmh() const { return _lastStopSpeedKmh; }
uint32_t RaceSyncLogger::lastStopGpsAgeMs() const { return _lastStopGpsAgeMs; }
bool RaceSyncLogger::lastStopGpsFixValid() const { return _lastStopGpsFixValid; }
uint8_t RaceSyncLogger::lastStopSatellites() const { return _lastStopSatellites; }
uint32_t RaceSyncLogger::gpsDropouts() const { return _recording ? _sessionGpsDropouts : _lastGpsDropouts; }
uint32_t RaceSyncLogger::maxGpsPacketAgeMs() const { return _recording ? _sessionMaxGpsPacketAgeMs : _lastMaxGpsPacketAgeMs; }
uint32_t RaceSyncLogger::invalidGpsSamples() const { return _recording ? _sessionInvalidGpsSamples : _lastInvalidGpsSamples; }

bool RaceSyncLogger::updateAutomaticSettings(double startSpeedKmh, uint32_t stopDelaySeconds)
{
    if (_recording) return false;
    if (startSpeedKmh < 1.0 || startSpeedKmh > 100.0) return false;
    if (stopDelaySeconds < 1 || stopDelaySeconds > 600) return false;

    _startSpeedKmh = startSpeedKmh;
    _stopDelayMs = stopDelaySeconds * 1000UL;
    _settingsPreferences.putDouble("logStartKmh", _startSpeedKmh);
    _settingsPreferences.putUInt("logStopDelay", _stopDelayMs);

    Serial.printf("[LOGGER] Settings saved: start %.1f km/h, stop delay %lu s\n",
                  _startSpeedKmh, (unsigned long)stopDelaySeconds);
    return true;
}

String RaceSyncLogger::createFilename(const Telemetry& t, DataMode mode) const
{
    char b[64];
    if (t.timeValid && t.year >= 2024)
        snprintf(b, sizeof(b), "RS_%04u-%02u-%02u_%02u-%02u-%02u.vbo", t.year,t.month,t.day,t.hour,t.minute,t.second);
    else
        snprintf(b, sizeof(b), "RS_LIVE_%010lu.vbo", (unsigned long)millis());
    return String(b);
}

String RaceSyncLogger::telemetryTimestamp(const Telemetry& t) const
{
    char b[40];
    if (t.timeValid && t.year >= 2024)
    {
        snprintf(b, sizeof(b), "%04u-%02u-%02uT%02u:%02u:%02uZ", t.year, t.month, t.day, t.hour, t.minute, t.second);
        return String(b);
    }
    snprintf(b, sizeof(b), "uptime:%lu", (unsigned long)(millis() / 1000));
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

String RaceSyncLogger::createVBoxLine(const Telemetry& t) const
{
    char line[512];
    snprintf(line,sizeof(line),"%03u %010.3f %+014.8f %+014.8f %07.3f %07.3f %+09.2f %+08.2f %.3f %02u %04d %09.0f %+.6E %+.6E %+.6E %+.6E %+.6E %+.6E %+.6E",
        t.satellites,t.rawTime,t.rawLatitude,t.rawLongitude,t.velocityKmh,t.heading,t.height,t.verticalVelocityMs,t.samplePeriod,t.solutionType,t.aviFileIndex,t.aviTime,t.comboAcc,t.oilPressure,t.oilTemperature,t.waterTemperature,t.revs,t.fuelPressure,t.comboG);
    return String(line);
}

void RaceSyncLogger::writeDiagnosticEvent(const char* event, const Telemetry* telemetry)
{
    if (!_logFile) return;
    _logFile.print("event="); _logFile.print(event);
    _logFile.print(" uptimeMs="); _logFile.print(millis());
    if (telemetry)
    {
        _logFile.print(" utc="); _logFile.print(telemetryTimestamp(*telemetry));
        _logFile.print(" speedKmh="); _logFile.print(telemetry->velocityKmh, 3);
        _logFile.print(" fixValid="); _logFile.print(telemetry->valid ? "true" : "false");
        _logFile.print(" sats="); _logFile.print(telemetry->satellites);
        _logFile.print(" solutionType="); _logFile.print(telemetry->solutionType);
    }
    _logFile.print(" gpsPacketAgeMs=");
    if (_currentGpsAgeMs == UINT32_MAX) _logFile.print(-1); else _logFile.print(_currentGpsAgeMs);
    _logFile.println();
    _logFile.flush();
}

void RaceSyncLogger::writeDiagnosticSummary(bool finalized)
{
    if (!_logFile) return;
    _logFile.println("[summary]");
    _logFile.print("endTime="); _logFile.println(_haveLastTelemetry ? telemetryTimestamp(_lastTelemetry) : String("unknown"));
    _logFile.print("stopReason="); _logFile.println(_lastStopReason);
    _logFile.print("durationSeconds="); _logFile.println(_lastSessionDurationSeconds);
    _logFile.print("samplesWritten="); _logFile.println(_lastSessionSamples);
    _logFile.print("vboFinalized="); _logFile.println(finalized ? "true" : "false");
    _logFile.print("vboFileSizeBytes="); _logFile.println((unsigned long long)_lastSessionFileSizeBytes);
    _logFile.print("stopSpeedKmh="); _logFile.println(_lastStopSpeedKmh, 3);
    _logFile.print("stopGpsPacketAgeMs=");
    if (_lastStopGpsAgeMs == UINT32_MAX) _logFile.println(-1); else _logFile.println(_lastStopGpsAgeMs);
    _logFile.print("stopGpsFixValid="); _logFile.println(_lastStopGpsFixValid ? "true" : "false");
    _logFile.print("stopSatellites="); _logFile.println(_lastStopSatellites);
    _logFile.print("gpsDropouts="); _logFile.println(_lastGpsDropouts);
    _logFile.print("maxGpsPacketAgeMs="); _logFile.println(_lastMaxGpsPacketAgeMs);
    _logFile.print("invalidGpsSamples="); _logFile.println(_lastInvalidGpsSamples);
    _logFile.print("sdWriteErrorsTotal="); _logFile.println(_writeErrors);
    _logFile.print("storageLastError="); _logFile.println(_storage ? _storage->lastError() : String("storage unavailable"));
    _logFile.flush();
}

bool RaceSyncLogger::storageHasSafeFreeSpace()
{
    if (!_storage || !_storage->ready() || !_storage->writable()) return false;
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
    if (!_storage->writable()) { Serial.print("[LOGGER] Storage not writable: "); Serial.println(_storage->lastError()); return false; }
    if (_storage->freeBytes() < RaceSyncConfig::MIN_FREE_STORAGE_BYTES) { Serial.println("[LOGGER] Insufficient free storage - recording not started"); return false; }

    _filename=createFilename(t,mode);
    _partFilename=_filename;
    _partFilename.replace(".vbo", ".part");
    _logFilename=_filename;
    _logFilename.replace(".vbo", ".log");

    _file=_storage->openPartWrite(_partFilename);
    if (!_file) { _writeErrors++; Serial.println("[LOGGER] Unable to create incomplete .part session"); return false; }

    _logFile=_storage->openFileWrite(_logFilename);
    if (!_logFile)
    {
        _writeErrors++;
        Serial.println("[LOGGER] Warning: unable to create diagnostic log; VBO recording will continue");
    }

    writeHeader(_file,mode); _file.flush();
    _sampleCount=0; _belowSpeedSince=0; _lastFlush=millis(); _lastWriteMs=0; _startedMs=millis(); _lastStorageCheckMs=0; _recording=true; _manualSession=manual; _autoStartInhibit=false;
    _lastTelemetry=t; _haveLastTelemetry=true; _sessionGpsDropouts=0; _sessionMaxGpsPacketAgeMs=0; _sessionInvalidGpsSamples=0; _gpsStale=false; _stationaryCandidateLogged=false;

    if (_logFile)
    {
        _logFile.println("RaceSync session diagnostic log");
        _logFile.print("vboFile="); _logFile.println(_filename);
        _logFile.print("startTime="); _logFile.println(telemetryTimestamp(t));
        _logFile.print("startMode="); _logFile.println(manual ? "MANUAL" : "AUTO");
        _logFile.print("startSpeedKmh="); _logFile.println(_startSpeedKmh, 2);
        _logFile.print("stopSpeedKmh="); _logFile.println(_stopSpeedKmh, 2);
        _logFile.print("stopDelaySeconds="); _logFile.println(_stopDelayMs / 1000);
        writeDiagnosticEvent("SESSION_START", &t);
    }

    Serial.printf("[LOGGER] Started: %s -> %s (%s), diagnostics=%s\n",
                  _partFilename.c_str(), _filename.c_str(), manual ? "MANUAL" : "AUTO", _logFilename.c_str());
    return true;
}

void RaceSyncLogger::stop(bool finalize, const char* reason)
{
    if (!_recording) return;

    _lastStopReason = reason ? reason : "UNKNOWN";
    _lastSessionDurationSeconds = _startedMs ? (millis() - _startedMs) / 1000 : 0;
    _lastSessionSamples = _sampleCount;
    _lastSessionFilename = _filename;
    _lastSessionLogFilename = _logFilename;
    _lastStopSpeedKmh = _haveLastTelemetry ? _lastTelemetry.velocityKmh : 0.0;
    _lastStopGpsAgeMs = _currentGpsAgeMs;
    _lastStopGpsFixValid = _haveLastTelemetry && _lastTelemetry.valid;
    _lastStopSatellites = _haveLastTelemetry ? _lastTelemetry.satellites : 0;
    _lastGpsDropouts = _sessionGpsDropouts;
    _lastMaxGpsPacketAgeMs = _sessionMaxGpsPacketAgeMs;
    _lastInvalidGpsSamples = _sessionInvalidGpsSamples;

    writeDiagnosticEvent("SESSION_STOP_REQUESTED", _haveLastTelemetry ? &_lastTelemetry : nullptr);

    if (_file)
    {
        _file.flush();
        _file.close();
    }

    bool finalized = false;
    if (finalize && _storage)
    {
        finalized = _storage->finalizePartFile(_partFilename, _filename);
        if (!finalized)
        {
            ++_writeErrors;
            Serial.printf("[LOGGER] Session close failed; recoverable file retained as %s\n", _partFilename.c_str());
        }
    }
    else
    {
        Serial.printf("[LOGGER] Incomplete session retained for recovery: %s\n", _partFilename.c_str());
    }

    _lastSessionFileSizeBytes = 0;
    if (finalized && _storage)
    {
        File finalFile = _storage->openRead(_filename);
        if (finalFile)
        {
            _lastSessionFileSizeBytes = finalFile.size();
            finalFile.close();
        }
    }

    writeDiagnosticSummary(finalized);
    if (_logFile) _logFile.close();

    _recording=false;
    _manualSession=false;
    _belowSpeedSince=0;
    _stationaryCandidateLogged=false;

    if (finalized)
    {
        Serial.printf("[LOGGER] Finalized: %s samples=%u size=%llu stopReason=%s\n",
            _filename.c_str(), _sampleCount, (unsigned long long)_lastSessionFileSizeBytes, _lastStopReason.c_str());
    }
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
    stop(true, "MANUAL");
    _autoStartInhibit = true;
    Serial.println("[LOGGER] Manual stop - automatic restart inhibited until speed drops below start threshold");
    return true;
}

void RaceSyncLogger::forceStop() { stop(true, "FORCED"); }

void RaceSyncLogger::observeGpsHealth(bool connected, bool fixValid, uint32_t packetAgeMs)
{
    _currentGpsAgeMs = packetAgeMs;
    if (!_recording) return;

    if (packetAgeMs != UINT32_MAX && packetAgeMs > _sessionMaxGpsPacketAgeMs)
        _sessionMaxGpsPacketAgeMs = packetAgeMs;

    const bool stale = !connected || packetAgeMs == UINT32_MAX || packetAgeMs > GPS_STALE_THRESHOLD_MS;
    if (stale && !_gpsStale)
    {
        _gpsStale = true;
        ++_sessionGpsDropouts;
        _belowSpeedSince = 0;
        _stationaryCandidateLogged = false;
        writeDiagnosticEvent("GPS_STALE", _haveLastTelemetry ? &_lastTelemetry : nullptr);
    }
    else if (!stale && _gpsStale)
    {
        _gpsStale = false;
        writeDiagnosticEvent("GPS_RECOVERED", _haveLastTelemetry ? &_lastTelemetry : nullptr);
    }

    if (!fixValid)
    {
        // An invalid fix is unknown motion state, never evidence that the bike is stationary.
        _belowSpeedSince = 0;
        _stationaryCandidateLogged = false;
    }
}

void RaceSyncLogger::writeSample(const Telemetry& t)
{
    if (!_recording) return;
    if (!storageHasSafeFreeSpace()) { stop(true, "LOW_STORAGE"); return; }
    size_t vboWritten=_file.println(createVBoxLine(t));
    if (vboWritten==0) { _writeErrors++; Serial.println("[LOGGER] VBO write failed - retaining .part for recovery"); stop(false, "SD_WRITE_ERROR"); return; }
    _sampleCount++; _lastWriteMs=millis(); uint32_t now=millis();
    if (now-_lastFlush >= RaceSyncConfig::LOG_FLUSH_INTERVAL_MS) { _file.flush(); _lastFlush=now; }
}

void RaceSyncLogger::processSample(const Telemetry& t, DataMode mode)
{
    _lastTelemetry = t;
    _haveLastTelemetry = true;

    if (!t.valid)
    {
        if (_recording)
        {
            ++_sessionInvalidGpsSamples;
            _belowSpeedSince = 0;
            _stationaryCandidateLogged = false;
        }
        return;
    }

    if (_recording && _manualSession) {
        writeSample(t);
        return;
    }

    if (_autoStartInhibit) {
        if (t.velocityKmh < _startSpeedKmh) {
            _autoStartInhibit = false;
            Serial.println("[LOGGER] Automatic start re-enabled");
        } else {
            return;
        }
    }

    if (!_recording && t.velocityKmh >= _startSpeedKmh) start(t,mode,false);
    if (!_recording) return;
    writeSample(t); if (!_recording) return;

    // A stale GPS state is unknown motion, not stationary. Do not advance stop timing.
    if (_gpsStale)
    {
        _belowSpeedSince = 0;
        _stationaryCandidateLogged = false;
        return;
    }

    if (t.velocityKmh <= _stopSpeedKmh) {
        if (_belowSpeedSince==0)
        {
            _belowSpeedSince=millis();
            if (!_stationaryCandidateLogged)
            {
                writeDiagnosticEvent("STATIONARY_CANDIDATE", &t);
                _stationaryCandidateLogged = true;
            }
        }
        if (millis()-_belowSpeedSince >= _stopDelayMs) stop(true, "STATIONARY_TIMEOUT");
    } else {
        if (_belowSpeedSince != 0) writeDiagnosticEvent("MOVEMENT_RESUMED", &t);
        _belowSpeedSince=0;
        _stationaryCandidateLogged=false;
    }
}
