#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "LapTimer.h"
#include "../config/RaceSyncTypes.h"
#include "../logging/RaceSyncStorage.h"

class RaceSyncLapTiming {
public:
    void begin(RaceSyncStorage& storage);
    void startSession(const String& vboFilename);
    void process(const Telemetry& telemetry);
    void invalidate() { _timer.invalidate(); }
    void stopSession();
    void addStatus(JsonObject status) const;
private:
    struct Circuit {
        String id, name, layout;
        RaceSyncLaps::Settings settings;
        double radius = 1500;
    };
    RaceSyncStorage* _storage = nullptr;
    std::vector<Circuit> _circuits;
    JsonDocument _database;
    RaceSyncLaps::LapTimer _timer;
    File _csv;
    String _csvFilename, _metadataFilename, _selectedId, _error, _selection;
    int _selected = -1;
    uint32_t _writeErrors = 0;
    bool ensureDatabase();
    bool loadDatabase();
    void selectCircuit(const RaceSyncLaps::Point& position);
    void failWrite();
    static String utcTimestamp(uint64_t milliseconds);
};
