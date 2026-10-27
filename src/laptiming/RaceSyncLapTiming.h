#pragma once
#include "CircuitDatabase.h"
#include "LapSessionWriter.h"
#include "GpsSampleAdapter.h"

// Coordinates the modules for the logger. Timing math, circuit configuration,
// circuit selection, telemetry conversion and file persistence live elsewhere.
class RaceSyncLapTiming {
public:
    void begin(RaceSyncStorage& storage);
    void startSession(const String& vboFilename);
    void process(const Telemetry& telemetry);
    void invalidate() { _timer.invalidate(); }
    void stopSession();
    void addStatus(JsonObject status) const;
private:
    CircuitDatabase _database;
    LapSessionWriter _writer;
    RaceSyncLaps::LapTimer _timer;
    int _selected = -1;
    String _selection;
    void selectCircuit(const RaceSyncLaps::Point& position);
};
