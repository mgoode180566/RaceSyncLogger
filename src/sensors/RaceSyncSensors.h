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
    static double medianOfThree(double a, double b, double c);

    // Set this to match the isolated ECU tach output. The default assumes
    // two falling edges per crankshaft revolution.
    static constexpr float RPM_PULSES_PER_REVOLUTION = 2.0f;
    static constexpr uint32_t RPM_MIN_PULSE_INTERVAL_US = 1500;
    static constexpr uint32_t RPM_SIGNAL_TIMEOUT_US = 250000;
    static constexpr double RPM_DEFAULT_MAX_VALID = 11000.0;
    static constexpr double RPM_MIN_CONFIGURABLE_LIMIT = 1000.0;
    static constexpr double RPM_MAX_CONFIGURABLE_LIMIT = 30000.0;
    static constexpr double RPM_DEBUG_MIN_ENGINE_RPM = 1000.0;
    static constexpr double RPM_LOW_SPIKE_RATIO = 0.50;
    static constexpr double RPM_HIGH_SPIKE_RATIO = 1.50;
    static constexpr double RPM_FILTER_ALPHA = 0.25;

    static volatile uint32_t _rpmLastPulseUs;
    static volatile uint32_t _rpmPeriodUs;
    static volatile uint32_t _rpmPulseCount;
    static volatile uint32_t _rpmRejectedPulseCount;
    static portMUX_TYPE _rpmMux;

    // Count evaluated intervals once, not once per main-loop pass.
    uint32_t _rpmLastEvaluatedPulseCount = 0;
    uint32_t _rpmRejectedReadingCount = 0;
    uint32_t _rpmLowSpikeCount = 0;
    uint32_t _rpmHighSpikeCount = 0;
    uint32_t _rpmZeroDropCount = 0;
    double _rpm = 0.0;
    double _rpmRawMeasured = 0.0;
    double _rpmMinAccepted = 0.0;
    double _rpmMaxAccepted = 0.0;
    double _rpmMaxValid = RPM_DEFAULT_MAX_VALID;
    double _rpmHistory[3] = {0.0, 0.0, 0.0};
    uint8_t _rpmHistoryCount = 0;
    uint8_t _rpmHistoryIndex = 0;
    uint32_t _rpmLastPulseAgeMs = UINT32_MAX;
    bool _rpmSignalPresent = false;
    bool _rpmPreviouslySignalPresent = false;
};
