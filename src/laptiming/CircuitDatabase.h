#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "CircuitSelector.h"
#include "../logging/RaceSyncStorage.h"

struct CircuitDefinition {
    String id, name, layout;
    RaceSyncLaps::Settings settings;
    double radius = 1500;
};

// Owns the SD database, JSON parsing/validation, and the immutable session copy.
// It does not choose a circuit or manage any lap/session output files.
class CircuitDatabase {
public:
    void begin(RaceSyncStorage& storage);
    bool load();
    const CircuitDefinition& circuit(size_t index) const { return _circuits[index]; }
    const std::vector<RaceSyncLaps::CircuitLocation>& locations() const { return _locations; }
    int preferredIndex() const;
    JsonVariantConst snapshot() const { return _database.as<JsonVariantConst>(); }
    const String& error() const { return _error; }
private:
    RaceSyncStorage* _storage = nullptr;
    std::vector<CircuitDefinition> _circuits;
    std::vector<RaceSyncLaps::CircuitLocation> _locations;
    JsonDocument _database;
    String _selectedId, _error;
    bool ensureDatabase();
};
