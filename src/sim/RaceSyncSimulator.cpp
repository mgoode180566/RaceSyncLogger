#include "RaceSyncSimulator.h"

bool RaceSyncSimulator::begin(
    RaceSyncStorage& storage)
{
    _storage = &storage;

    if (_file)
    {
        _file.close();
    }

    _filename =
        storage.findDemoSource();

    _finished = false;

    if (_filename.length() == 0)
    {
        _available = false;

        Serial.println(
            "[DEMO] No demo VBO available"
        );

        return false;
    }

    _file =
        storage.openRead(
            _filename
        );

    if (!_file)
    {
        _available = false;

        Serial.println(
            "[DEMO] Unable to open demo VBO"
        );

        return false;
    }

    if (!seekData())
    {
        _file.close();

        _available = false;

        Serial.println(
            "[DEMO] [data] section not found"
        );

        return false;
    }

    _available = true;
    _finished = false;

    _nextSampleMs =
        millis();

    Serial.print(
        "[DEMO] Source: "
    );

    Serial.println(
        _filename
    );

    return true;
}


bool RaceSyncSimulator::available() const
{
    return _available;
}


bool RaceSyncSimulator::finished() const
{
    return _finished;
}


const String& RaceSyncSimulator::sourceFilename() const
{
    return _filename;
}


double RaceSyncSimulator::convertRawLatitude(
    double raw)
{
    return raw / 60.0;
}


double RaceSyncSimulator::convertRawLongitude(
    double raw)
{
    return -raw / 60.0;
}


bool RaceSyncSimulator::seekData()
{
    _file.seek(0);

    while (_file.available())
    {
        String line =
            _file.readStringUntil(
                '\n'
            );

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

    double values[19] =
        {};

    int count =
        0;

    char* context =
        nullptr;

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
        values[count++] =
            atof(token);

        token =
            strtok_r(
                nullptr,
                " \t\r\n",
                &context
            );
    }

    if (count < 12)
    {
        return false;
    }

    telemetry.satellites =
        (uint8_t)values[0];

    telemetry.rawTime =
        values[1];

    telemetry.rawLatitude =
        values[2];

    telemetry.rawLongitude =
        values[3];

    telemetry.latitude =
        convertRawLatitude(
            values[2]
        );

    telemetry.longitude =
        convertRawLongitude(
            values[3]
        );

    telemetry.velocityKmh =
        values[4];

    telemetry.heading =
        values[5];

    telemetry.height =
        values[6];

    telemetry.verticalVelocityMs =
        values[7];

    telemetry.samplePeriod =
        values[8];

    if (
        telemetry.samplePeriod <= 0 ||
        telemetry.samplePeriod > 1
    )
    {
        telemetry.samplePeriod =
            0.040;
    }

    telemetry.solutionType =
        (uint8_t)values[9];

    telemetry.aviFileIndex =
        (int)values[10];

    telemetry.aviTime =
        values[11];

    if (count >= 19)
    {
        telemetry.comboAcc =
            values[12];

        telemetry.oilPressure =
            values[13];

        telemetry.oilTemperature =
            values[14];

        telemetry.waterTemperature =
            values[15];

        telemetry.revs =
            values[16];

        telemetry.fuelPressure =
            values[17];

        telemetry.comboG =
            values[18];
    }

    telemetry.valid =
        true;

    // Demo rows contain time-of-day but not a full date.
    telemetry.timeValid =
        false;

    telemetry.sampleIndex++;

    telemetry.rawVBoxLine =
        sourceLine;

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
            _file.readStringUntil(
                '\n'
            );

        line.trim();

        if (
            line.length() == 0 ||
            line.startsWith("[")
        )
        {
            continue;
        }

        if (
            parseLine(
                line,
                telemetry
            )
        )
        {
            return true;
        }
    }

    // --------------------------------------------------------
    // IMPORTANT:
    //
    // The old demo implementation reopened the source file here
    // and replayed it forever. That meant the logger also stayed
    // open forever and eventually filled LittleFS.
    //
    // Demo mode now plays exactly once.
    // --------------------------------------------------------

    if (_file)
    {
        _file.close();
    }

    _available =
        false;

    _finished =
        true;

    Serial.println(
        "[DEMO] End of demo session"
    );

    return false;
}


bool RaceSyncSimulator::update(
    Telemetry& telemetry)
{
    if (
        !_available ||
        _finished
    )
    {
        return false;
    }

    uint32_t now =
        millis();

    if (
        (int32_t)(
            now -
            _nextSampleMs
        ) < 0
    )
    {
        return false;
    }

    if (
        !readNext(
            telemetry
        )
    )
    {
        return false;
    }

    uint32_t delayMs =
        (uint32_t)(
            telemetry.samplePeriod *
            1000.0
        );

    if (delayMs < 1)
    {
        delayMs =
            40;
    }

    _nextSampleMs +=
        delayMs;

    // If the loop has fallen a long way behind, resynchronise
    // instead of trying to emit a large burst of samples.
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
