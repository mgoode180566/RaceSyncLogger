#include "RaceSyncLogger.h"
#include "../config/RaceSyncConfig.h"

bool RaceSyncLogger::begin(
    RaceSyncStorage& storage)
{
    _storage = &storage;
    return storage.ready();
}

bool RaceSyncLogger::recording() const
{
    return _recording;
}

const String& RaceSyncLogger::currentFilename() const
{
    return _filename;
}

uint32_t RaceSyncLogger::sampleCount() const
{
    return _sampleCount;
}

uint32_t RaceSyncLogger::storageWriteErrors() const
{
    return _writeErrors;
}

uint32_t RaceSyncLogger::lastWriteAgeMs() const
{
    return _lastWriteMs == 0
        ? UINT32_MAX
        : millis() - _lastWriteMs;
}

uint32_t RaceSyncLogger::recordingSeconds() const
{
    return (
        _recording &&
        _startedMs != 0
    )
        ? (
            millis() -
            _startedMs
        ) / 1000
        : 0;
}

String RaceSyncLogger::createFilename(
    const Telemetry& telemetry,
    DataMode mode) const
{
    char buffer[64];

    if (
        telemetry.timeValid &&
        telemetry.year >= 2024
    )
    {
        snprintf(
            buffer,
            sizeof(buffer),

            "RS_%04u%02u%02u_%02u%02u%02u.vbo",

            telemetry.year,
            telemetry.month,
            telemetry.day,
            telemetry.hour,
            telemetry.minute,
            telemetry.second
        );
    }
    else
    {
        snprintf(
            buffer,
            sizeof(buffer),

            "RS_%s_%010lu.vbo",

            mode == DataMode::DEMO
                ? "DEMO"
                : "LIVE",

            (unsigned long)millis()
        );
    }

    return String(buffer);
}

void RaceSyncLogger::writeHeader(
    File& f,
    DataMode mode)
{
    f.println("[header]");
    f.println("satellites");
    f.println("time");
    f.println("latitude");
    f.println("longitude");
    f.println("velocity kmh");
    f.println("heading");
    f.println("height");
    f.println("vertical velocity m/s");
    f.println("sampleperiod");
    f.println("solution type");
    f.println("avifileindex");
    f.println("avisynctime");
    f.println("ComboAcc");
    f.println("ADC3 Oil Pressure");
    f.println("ADC2 Oil Temp");
    f.println("ADC1 Water Temp");
    f.println("Revs");
    f.println("ADC4 Fuel Pressure");
    f.println("Combo_G");

    f.println();
    f.println("[comments]");
    f.println("RaceSync ESP32 Logger");
    f.print("Source: ");
    f.println(dataModeName(mode));

    f.println();
    f.println("[column names]");

    f.println(
        "sats time lat long velocity heading height "
        "vert-vel Tsample solution_type "
        "avifileindex avitime "
        "ComboAcc ADC3_Oil_Pressure "
        "ADC2_Oil_Temp ADC1_Water_Temp "
        "Revs ADC4_Fuel_Pressure Combo_G"
    );

    f.println();
    f.println("[data]");
}

String RaceSyncLogger::createVBoxLine(
    const Telemetry& telemetry) const
{
    char line[512];

    snprintf(
        line,
        sizeof(line),

        "%03u "
        "%010.3f "
        "%+014.8f "
        "%+014.8f "
        "%07.3f "
        "%07.3f "
        "%+09.2f "
        "%+08.2f "
        "%.3f "
        "%02u "
        "%04d "
        "%09.0f "
        "%+.6E "
        "%+.6E "
        "%+.6E "
        "%+.6E "
        "%+.6E "
        "%+.6E "
        "%+.6E",

        telemetry.satellites,
        telemetry.rawTime,
        telemetry.rawLatitude,
        telemetry.rawLongitude,
        telemetry.velocityKmh,
        telemetry.heading,
        telemetry.height,
        telemetry.verticalVelocityMs,
        telemetry.samplePeriod,
        telemetry.solutionType,
        telemetry.aviFileIndex,
        telemetry.aviTime,
        telemetry.comboAcc,
        telemetry.oilPressure,
        telemetry.oilTemperature,
        telemetry.waterTemperature,
        telemetry.revs,
        telemetry.fuelPressure,
        telemetry.comboG
    );

    return String(line);
}

bool RaceSyncLogger::start(
    const Telemetry& telemetry,
    DataMode mode)
{
    if (_recording)
        return true;

    _filename =
        createFilename(
            telemetry,
            mode
        );

    _file =
        _storage->openWrite(
            _filename
        );

    if (!_file)
    {
        _writeErrors++;
        Serial.println(
            "[LOGGER] Unable to create session"
        );
        return false;
    }

    writeHeader(
        _file,
        mode
    );

    _file.flush();

    _sampleCount = 0;
    _belowSpeedSince = 0;
    _lastFlush = millis();
    _lastWriteMs = 0;
    _startedMs = millis();

    _recording = true;

    Serial.print("[LOGGER] Started: ");
    Serial.println(_filename);

    return true;
}

void RaceSyncLogger::stop()
{
    if (!_recording)
        return;

    _file.flush();
    _file.close();

    _recording = false;
    _belowSpeedSince = 0;

    Serial.print("[LOGGER] Closed: ");
    Serial.print(_filename);
    Serial.print(" samples=");
    Serial.println(_sampleCount);
}

void RaceSyncLogger::writeSample(
    const Telemetry& telemetry)
{
    if (!_recording)
        return;

    size_t written =
        _file.println(
            createVBoxLine(
                telemetry
            )
        );

    if (written == 0)
    {
        _writeErrors++;
        Serial.println("[LOGGER] Write failed");
        return;
    }

    _sampleCount++;
    _lastWriteMs = millis();

    uint32_t now = millis();

    if (
        now -
        _lastFlush >=
        RaceSyncConfig::LOG_FLUSH_INTERVAL_MS
    )
    {
        _file.flush();
        _lastFlush = now;
    }
}

void RaceSyncLogger::processSample(
    const Telemetry& telemetry,
    DataMode mode)
{
    if (!telemetry.valid)
        return;

    if (
        !_recording &&
        telemetry.velocityKmh >=
            RaceSyncConfig::LOG_START_SPEED_KMH
    )
    {
        start(
            telemetry,
            mode
        );
    }

    if (!_recording)
        return;

    writeSample(telemetry);

    if (
        telemetry.velocityKmh <=
        RaceSyncConfig::LOG_STOP_SPEED_KMH
    )
    {
        if (_belowSpeedSince == 0)
            _belowSpeedSince = millis();

        if (
            millis() -
            _belowSpeedSince >=
            RaceSyncConfig::LOG_STOP_DELAY_MS
        )
        {
            stop();
        }
    }
    else
    {
        _belowSpeedSince = 0;
    }
}
