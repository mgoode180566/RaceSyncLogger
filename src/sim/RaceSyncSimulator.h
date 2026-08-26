#pragma once

#include <Arduino.h>
#include <FS.h>

#include "../config/RaceSyncTypes.h"
#include "../logging/RaceSyncStorage.h"

class RaceSyncSimulator
{
public:
    bool begin(RaceSyncStorage& storage);

    bool available() const;
    bool finished() const;

    const String& sourceFilename() const;

    // Returns true only when a new telemetry sample was produced.
    bool update(Telemetry& telemetry);

private:
    RaceSyncStorage* _storage = nullptr;

    File _file;
    String _filename;

    bool _available = false;
    bool _finished = false;

    uint32_t _nextSampleMs = 0;

    bool seekData();
    bool readNext(Telemetry& telemetry);
    bool parseLine(const String& line, Telemetry& telemetry);

    static double convertRawLatitude(double raw);
    static double convertRawLongitude(double raw);
};
