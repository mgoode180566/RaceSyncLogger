#include "RaceSyncSensors.h"

#include <Preferences.h>
#include "../../include/Pins.h"

volatile uint32_t RaceSyncSensors::_rpmLastPulseUs = 0;
volatile uint32_t RaceSyncSensors::_rpmPeriodUs = 0;
volatile uint32_t RaceSyncSensors::_rpmPulseCount = 0;
volatile uint32_t RaceSyncSensors::_rpmRejectedPulseCount = 0;
portMUX_TYPE RaceSyncSensors::_rpmMux = portMUX_INITIALIZER_UNLOCKED;

void ARDUINO_ISR_ATTR RaceSyncSensors::handleRpmPulse()
{
    const uint32_t nowUs = micros();

    portENTER_CRITICAL_ISR(&_rpmMux);
    const uint32_t intervalUs = nowUs - _rpmLastPulseUs;

    // Reject optocoupler chatter and electrical spikes. Always accept the
    // first edge so that signal presence can be reported immediately.
    if (_rpmLastPulseUs == 0 || intervalUs >= RPM_MIN_PULSE_INTERVAL_US)
    {
        if (_rpmLastPulseUs != 0)
        {
            _rpmPeriodUs = intervalUs;
        }

        _rpmLastPulseUs = nowUs;
        ++_rpmPulseCount;
    }
    else if (_rpmRejectedPulseCount != UINT32_MAX)
    {
        ++_rpmRejectedPulseCount;
    }
    portEXIT_CRITICAL_ISR(&_rpmMux);
}

double RaceSyncSensors::medianOfThree(double a, double b, double c)
{
    if (a > b)
    {
        const double t = a;
        a = b;
        b = t;
    }
    if (b > c)
    {
        const double t = b;
        b = c;
        c = t;
    }
    if (a > b)
    {
        const double t = a;
        a = b;
        b = t;
    }
    return b;
}

bool RaceSyncSensors::begin()
{
    Preferences preferences;
    if (preferences.begin("racesync", true))
    {
        const double savedLimit = preferences.getDouble("rpmMaxValid", RPM_DEFAULT_MAX_VALID);
        preferences.end();
        if (savedLimit >= RPM_MIN_CONFIGURABLE_LIMIT && savedLimit <= RPM_MAX_CONFIGURABLE_LIMIT)
            _rpmMaxValid = savedLimit;
    }

    pinMode(Pin::RPM_INPUT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Pin::RPM_INPUT), handleRpmPulse, FALLING);

    Serial.printf("[RPM] ECU tachometer capture ready on GPIO%d (%.2f pulse/rev, max %.0f rpm)\n",
                  Pin::RPM_INPUT, RPM_PULSES_PER_REVOLUTION, _rpmMaxValid);
    return true;
}

void RaceSyncSensors::update(Telemetry& telemetry)
{
    if (telemetry.rpmMaxValid >= RPM_MIN_CONFIGURABLE_LIMIT &&
        telemetry.rpmMaxValid <= RPM_MAX_CONFIGURABLE_LIMIT &&
        telemetry.rpmMaxValid != _rpmMaxValid)
    {
        _rpmMaxValid = telemetry.rpmMaxValid;
    }
    telemetry.rpmMaxValid = _rpmMaxValid;

    uint32_t lastPulseUs;
    uint32_t periodUs;
    uint32_t pulseCount;
    uint32_t rejectedPulseCount;

    portENTER_CRITICAL(&_rpmMux);
    lastPulseUs = _rpmLastPulseUs;
    periodUs = _rpmPeriodUs;
    pulseCount = _rpmPulseCount;
    rejectedPulseCount = _rpmRejectedPulseCount;
    portEXIT_CRITICAL(&_rpmMux);

    const uint32_t nowUs = micros();
    const uint32_t pulseAgeUs = lastPulseUs == 0 ? UINT32_MAX : nowUs - lastPulseUs;

    _rpmSignalPresent = lastPulseUs != 0 && pulseAgeUs <= RPM_SIGNAL_TIMEOUT_US;
    _rpmLastPulseAgeMs = lastPulseUs == 0 ? UINT32_MAX : pulseAgeUs / 1000U;

    double measuredRpm = 0.0;
    if (_rpmSignalPresent && periodUs > 0)
    {
        measuredRpm = 60000000.0 / (static_cast<double>(periodUs) * RPM_PULSES_PER_REVOLUTION);
    }
    _rpmRawMeasured = measuredRpm;

    const bool newAcceptedPulse = pulseCount != _rpmLastEvaluatedPulseCount;
    const double previousFilteredRpm = _rpm;

    if (!_rpmSignalPresent)
    {
        // Only a genuine loss of tach pulses is allowed to drive the published
        // RPM to zero. Rejected/noisy readings hold the previous good value.
        _rpm = 0.0;
        _rpmHistoryCount = 0;
        _rpmHistoryIndex = 0;
    }
    else if (newAcceptedPulse && measuredRpm > 0.0)
    {
        bool rejectReading = false;

        // First reject an absolute over-range measurement. Do not convert it
        // to zero; keep the previous valid RPM until a good pulse arrives.
        if (measuredRpm > _rpmMaxValid)
        {
            rejectReading = true;
            if (_rpmRejectedReadingCount != UINT32_MAX)
                ++_rpmRejectedReadingCount;
        }

        // Flag and reject physically implausible one-period excursions once
        // the engine is already known to be running. These thresholds existed
        // as diagnostics previously; they now protect the logged RPM trace.
        if (!rejectReading && _rpm > RPM_DEBUG_MIN_ENGINE_RPM)
        {
            if (measuredRpm < _rpm * RPM_LOW_SPIKE_RATIO)
            {
                rejectReading = true;
                if (_rpmLowSpikeCount != UINT32_MAX) ++_rpmLowSpikeCount;
                if (_rpmRejectedReadingCount != UINT32_MAX) ++_rpmRejectedReadingCount;
            }
            else if (measuredRpm > _rpm * RPM_HIGH_SPIKE_RATIO)
            {
                rejectReading = true;
                if (_rpmHighSpikeCount != UINT32_MAX) ++_rpmHighSpikeCount;
                if (_rpmRejectedReadingCount != UINT32_MAX) ++_rpmRejectedReadingCount;
            }
        }

        if (!rejectReading)
        {
            _rpmHistory[_rpmHistoryIndex] = measuredRpm;
            _rpmHistoryIndex = (_rpmHistoryIndex + 1U) % 3U;
            if (_rpmHistoryCount < 3U) ++_rpmHistoryCount;

            double filteredInput = measuredRpm;
            if (_rpmHistoryCount == 3U)
            {
                filteredInput = medianOfThree(_rpmHistory[0], _rpmHistory[1], _rpmHistory[2]);
            }

            if (_rpmMinAccepted == 0.0 || filteredInput < _rpmMinAccepted) _rpmMinAccepted = filteredInput;
            if (filteredInput > _rpmMaxAccepted) _rpmMaxAccepted = filteredInput;

            if (_rpm == 0.0)
            {
                _rpm = filteredInput;
            }
            else
            {
                // Update once per accepted pulse, not once per main-loop pass.
                // This keeps filter behaviour independent of loop frequency.
                _rpm += RPM_FILTER_ALPHA * (filteredInput - _rpm);
            }
        }
    }

    // Count only a genuine output transition to zero caused by signal timeout.
    if (previousFilteredRpm > RPM_DEBUG_MIN_ENGINE_RPM && _rpm == 0.0)
    {
        if (_rpmZeroDropCount != UINT32_MAX) ++_rpmZeroDropCount;
    }

    _rpmPreviouslySignalPresent = _rpmSignalPresent;
    _rpmLastEvaluatedPulseCount = pulseCount;

    telemetry.rpmRejectedReadingCount = _rpmRejectedReadingCount;
    telemetry.rpmRejectedPulseCount = rejectedPulseCount;
    telemetry.rpmLowSpikeCount = _rpmLowSpikeCount;
    telemetry.rpmHighSpikeCount = _rpmHighSpikeCount;
    telemetry.rpmZeroDropCount = _rpmZeroDropCount;
    telemetry.rpmRawMeasured = _rpmRawMeasured;
    telemetry.rpmMinAccepted = _rpmMinAccepted;
    telemetry.rpmMaxAccepted = _rpmMaxAccepted;
    telemetry.rpmLastPeriodUs = periodUs;
    telemetry.revs = _rpm;
    telemetry.rpmSignalPresent = _rpmSignalPresent;
    telemetry.rpmPulseCount = pulseCount;
    telemetry.rpmLastPulseAgeMs = _rpmLastPulseAgeMs;
    telemetry.rpmInputLevel = digitalRead(Pin::RPM_INPUT) == HIGH ? HIGH : LOW;
}

double RaceSyncSensors::rpm() const
{
    return _rpm;
}

bool RaceSyncSensors::rpmSignalPresent() const
{
    return _rpmSignalPresent;
}

uint32_t RaceSyncSensors::rpmPulseCount() const
{
    uint32_t count;
    portENTER_CRITICAL(&_rpmMux);
    count = _rpmPulseCount;
    portEXIT_CRITICAL(&_rpmMux);
    return count;
}

uint32_t RaceSyncSensors::rpmLastPulseAgeMs() const
{
    return _rpmLastPulseAgeMs;
}
