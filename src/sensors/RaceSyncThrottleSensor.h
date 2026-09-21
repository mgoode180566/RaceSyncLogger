#pragma once
#include <Arduino.h>
#include "../config/RaceSyncTypes.h"

class RaceSyncThrottleSensor
{
public:
    bool begin();
    void update(Telemetry& telemetry);
    bool calibrateClosed();
    bool calibrateOpen();
    bool clearCalibration();
    uint16_t raw() const { return _raw; }
    uint16_t filteredRaw() const { return static_cast<uint16_t>(_filteredRaw + 0.5f); }
    uint16_t closedRaw() const { return _closedRaw; }
    uint16_t openRaw() const { return _openRaw; }
    float percent() const { return _percent; }
    bool calibrated() const;
    bool connected() const { return _connected; }

private:
    static constexpr uint32_t SAMPLE_INTERVAL_US = 5000; // 200 Hz
    static constexpr uint16_t MIN_CALIBRATION_SPAN = 400;
    static constexpr float FILTER_ALPHA = 0.18f;
    uint32_t _lastSampleUs = 0;
    uint16_t _raw = 0;
    float _filteredRaw = 0.0f;
    uint16_t _closedRaw = 0;
    uint16_t _openRaw = 0;
    float _percent = 0.0f;
    bool _connected = false;
    bool saveCalibration();
};
