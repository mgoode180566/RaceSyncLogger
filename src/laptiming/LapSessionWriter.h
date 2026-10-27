#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "LapTimer.h"
#include "../logging/RaceSyncStorage.h"

// Owns only the CSV/JSON companion files and their write failures.
// Receives completed laps; never detects crossings or selects a circuit.
class LapSessionWriter {
public:
    void begin(RaceSyncStorage& storage) { _storage=&storage; }
    bool startSession(const String& vboFilename, JsonVariantConst database,
                      bool configurationValid, const String& configurationError);
    bool append(const String& circuitId, const RaceSyncLaps::Crossing& lap);
    bool close();
    bool active() const { return static_cast<bool>(_csv); }
    const String& filename() const { return _csvFilename; }
    const String& error() const { return _error; }
    uint32_t writeErrors() const { return _writeErrors; }
private:
    RaceSyncStorage* _storage = nullptr;
    File _csv;
    String _csvFilename, _error;
    uint32_t _writeErrors = 0;
    bool failWrite();
};
