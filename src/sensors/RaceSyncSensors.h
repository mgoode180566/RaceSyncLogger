#pragma once
#include <Arduino.h>

#include "../config/RaceSyncTypes.h"

class RaceSyncSensors
{
public:
    bool begin();
    void update(Telemetry& telemetry);

    double rpm() const;
    bool rpmSignalPresent() const;
    uint32_t rpmPulseCount() const;
    uint32_t rpmLastPulseAgeMs() const;

private:
    static void ARDUINO_ISR_ATTR handleRpmPulse();

    // Set this to match the isolated ECU tach output. The default assumes
    // one falling edge per crankshaft revolution.
    static constexpr float RPM_PULSES_PER_REVOLUTION = 1.0f;
    static constexpr uint32_t RPM_MIN_PULSE_INTERVAL_US = 1500;
    static constexpr uint32_t RPM_SIGNAL_TIMEOUT_US = 500000;

    static volatile uint32_t _rpmLastPulseUs;
    static volatile uint32_t _rpmPeriodUs;
    static volatile uint32_t _rpmPulseCount;
    static portMUX_TYPE _rpmMux;

    double _rpm = 0.0;
    uint32_t _rpmLastPulseAgeMs = UINT32_MAX;
    bool _rpmSignalPresent = false;
};
