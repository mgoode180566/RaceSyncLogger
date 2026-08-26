#include "RaceSyncSimulator.h"

bool RaceSyncSimulator::begin(
    RaceSyncStorage& storage)
{
    _storage = &storage;

    if (_file)
        _file.close();

    _filename =
        storage.findDemoSource();

    if (_filename.length() == 0)
    {
        _available = false;
        Serial.println("[DEMO] No demo VBO available");
        return false;
    }

    _file =
        storage.openRead(
            _filename
        );

    if (!_file)
    {
        _available = false;
        return false;
    }

    if (!seekData())
    {
        _file.close();
        _available = false;
        return false;
    }

    _available = true;
    _nextSampleMs = millis();

    Serial.print("[DEMO] Source: ");
    Serial.println(_filename);

    return true;
}

bool RaceSyncSimulator::available() const
{
    return _available;
}

const String& RaceSyncSimulator::sourceFilename() const
{
    return _filename;
}

double RaceSyncSimulator::convertRawLatitude(double raw)
{
    return raw / 60.0;
}

double RaceSyncSimulator::convertRawLongitude(double raw)
{
    return -raw / 60.0;
}

bool RaceSyncSimulator::seekData()
{
    _file.seek(0);

    while (_file.available())
    {
        String line =
            _file.readStringUntil('\n');

        line.trim();

        if (
            line.equalsIgnoreCase(
                "[data]"
            )
        )
        {
            return true;
        }
    }

    return false;
}

bool RaceSyncSimulator::parseLine(
    const String& sourceLine,
    Telemetry& telemetry)
{
    char buffer[512];
    sourceLine.toCharArray(
        buffer,
        sizeof(buffer)
    );

    double v[19] = {};
    int count = 0;

    char* context = nullptr;

    char* token =
        strtok_r(
            buffer,
            " \t\r\n",
            &context
        );

    while (
        token &&
        count < 19
    )
    {
        v[count++] =
            atof(token);

        token =
            strtok_r(
                nullptr,
                " \t\r\n",
                &context
            );
    }

    if (count < 12)
        return false;

    telemetry.satellites = (uint8_t)v[0];
    telemetry.rawTime = v[1];
    telemetry.rawLatitude = v[2];
    telemetry.rawLongitude = v[3];

    telemetry.latitude =
        convertRawLatitude(v[2]);

    telemetry.longitude =
        convertRawLongitude(v[3]);

    telemetry.velocityKmh = v[4];
    telemetry.heading = v[5];
    telemetry.height = v[6];
    telemetry.verticalVelocityMs = v[7];
    telemetry.samplePeriod = v[8];

    if (
        telemetry.samplePeriod <= 0 ||
        telemetry.samplePeriod > 1
    )
    {
        telemetry.samplePeriod = 0.040;
    }

    telemetry.solutionType = (uint8_t)v[9];
    telemetry.aviFileIndex = (int)v[10];
    telemetry.aviTime = v[11];

    if (count >= 19)
    {
        telemetry.comboAcc = v[12];
        telemetry.oilPressure = v[13];
        telemetry.oilTemperature = v[14];
        telemetry.waterTemperature = v[15];
        telemetry.revs = v[16];
        telemetry.fuelPressure = v[17];
        telemetry.comboG = v[18];
    }

    telemetry.valid = true;
    telemetry.timeValid = false;
    telemetry.sampleIndex++;
    telemetry.rawVBoxLine = sourceLine;

    return true;
}

bool RaceSyncSimulator::readNext(
    Telemetry& telemetry)
{
    while (
        _file &&
        _file.available()
    )
    {
        String line =
            _file.readStringUntil('\n');

        line.trim();

        if (
            line.length() == 0 ||
            line.startsWith("[")
        )
        {
            continue;
        }

        if (parseLine(line, telemetry))
            return true;
    }

    // Loop demo forever.
    return
        begin(*_storage) &&
        readNext(telemetry);
}

bool RaceSyncSimulator::update(
    Telemetry& telemetry)
{
    if (!_available)
        return false;

    uint32_t now = millis();

    if (
        (int32_t)(
            now -
            _nextSampleMs
        ) < 0
    )
    {
        return false;
    }

    if (!readNext(telemetry))
        return false;

    uint32_t delayMs =
        (uint32_t)(
            telemetry.samplePeriod *
            1000.0
        );

    if (delayMs < 1)
        delayMs = 40;

    _nextSampleMs += delayMs;

    if (
        (int32_t)(
            now -
            _nextSampleMs
        ) > 1000
    )
    {
        _nextSampleMs =
            now +
            delayMs;
    }

    return true;
}
