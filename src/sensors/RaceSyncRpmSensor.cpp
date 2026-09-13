#include "RaceSyncRpmSensor.h"

#include <Preferences.h>
#include "../../include/Pins.h"

volatile uint32_t RaceSyncRpmSensor::_rpmLastPulseUs = 0;
volatile uint32_t RaceSyncRpmSensor::_rpmPeriodUs = 0;
volatile uint32_t RaceSyncRpmSensor::_rpmPulseCount = 0;
volatile uint32_t RaceSyncRpmSensor::_rpmRejectedPulseCount = 0;
portMUX_TYPE RaceSyncRpmSensor::_rpmMux = portMUX_INITIALIZER_UNLOCKED;

void ARDUINO_ISR_ATTR RaceSyncRpmSensor::handleRpmPulse()
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
        _rpmPulseCount = _rpmPulseCount + 1;
    }
    else if (_rpmRejectedPulseCount != UINT32_MAX)
    {
        _rpmRejectedPulseCount = _rpmRejectedPulseCount + 1;
    }
    portEXIT_CRITICAL_ISR(&_rpmMux);
}

double RaceSyncRpmSensor::medianOfThree(double a, double b, double c)
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

bool RaceSyncRpmSensor::begin()
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

void RaceSyncRpmSensor::update(Telemetry& telemetry)
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
        // RPM to zero. Also discard all filter/candidate history so restart is clean.
        _rpm = 0.0;
        _rpmHistoryCount = 0;
        _rpmHistoryIndex = 0;
        _rpmStepCandidate = 0.0;
        _rpmStepCandidateCount = 0;
    }
    else if (newAcceptedPulse && measuredRpm > 0.0)
    {
        bool acceptReading = true;
        bool reseedFilter = false;

        // Absolute over-range measurements are never accepted or used as a
        // confirmation candidate. Hold the previous good RPM until a valid pulse arrives.
        if (measuredRpm > _rpmMaxValid)
        {
            acceptReading = false;
            _rpmStepCandidate = 0.0;
            _rpmStepCandidateCount = 0;
            if (_rpmRejectedReadingCount != UINT32_MAX)
                ++_rpmRejectedReadingCount;
        }
        else if (_rpm > RPM_DEBUG_MIN_ENGINE_RPM)
        {
            const bool largeLowStep = measuredRpm < _rpm * RPM_LOW_SPIKE_RATIO;
            const bool largeHighStep = measuredRpm > _rpm * RPM_HIGH_SPIKE_RATIO;

            if (largeLowStep || largeHighStep)
            {
                // Do not permanently reject a large step. The first pulse is held
                // as a candidate. A second pulse that agrees with that candidate
                // confirms a genuine RPM change and immediately re-seeds the filter.
                if (largeLowStep)
                {
                    if (_rpmLowSpikeCount != UINT32_MAX) ++_rpmLowSpikeCount;
                }
                else
                {
                    if (_rpmHighSpikeCount != UINT32_MAX) ++_rpmHighSpikeCount;
                }

                bool agreesWithCandidate = false;
                if (_rpmStepCandidateCount > 0 && _rpmStepCandidate > 0.0)
                {
                    const double difference = measuredRpm > _rpmStepCandidate
                                                ? measuredRpm - _rpmStepCandidate
                                                : _rpmStepCandidate - measuredRpm;
                    const double tolerance = _rpmStepCandidate * RPM_STEP_CONFIRM_TOLERANCE;
                    agreesWithCandidate = difference <= tolerance;
                }

                if (agreesWithCandidate)
                {
                    if (_rpmStepCandidateCount < UINT8_MAX) ++_rpmStepCandidateCount;
                }
                else
                {
                    _rpmStepCandidate = measuredRpm;
                    _rpmStepCandidateCount = 1;
                }

                if (_rpmStepCandidateCount >= RPM_STEP_CONFIRM_PULSES)
                {
                    // Confirmed real step: accept the latest measurement and reset
                    // the median/EMA history so the old RPM cannot latch the output.
                    acceptReading = true;
                    reseedFilter = true;
                    _rpmStepCandidate = 0.0;
                    _rpmStepCandidateCount = 0;
                }
                else
                {
                    acceptReading = false;
                    if (_rpmRejectedReadingCount != UINT32_MAX)
                        ++_rpmRejectedReadingCount;
                }
            }
            else
            {
                // A normal reading invalidates any unconfirmed one-pulse excursion.
                _rpmStepCandidate = 0.0;
                _rpmStepCandidateCount = 0;
            }
        }
        else
        {
            _rpmStepCandidate = 0.0;
            _rpmStepCandidateCount = 0;
        }

        if (acceptReading)
        {
            double filteredInput = measuredRpm;

            if (reseedFilter)
            {
                _rpmHistory[0] = measuredRpm;
                _rpmHistory[1] = measuredRpm;
                _rpmHistory[2] = measuredRpm;
                _rpmHistoryCount = 1;
                _rpmHistoryIndex = 1;
                _rpm = measuredRpm;
            }
            else
            {
                _rpmHistory[_rpmHistoryIndex] = measuredRpm;
                _rpmHistoryIndex = (_rpmHistoryIndex + 1U) % 3U;
                if (_rpmHistoryCount < 3U) ++_rpmHistoryCount;

                if (_rpmHistoryCount == 3U)
                {
                    filteredInput = medianOfThree(_rpmHistory[0], _rpmHistory[1], _rpmHistory[2]);
                }

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

            if (_rpmMinAccepted == 0.0 || filteredInput < _rpmMinAccepted) _rpmMinAccepted = filteredInput;
            if (filteredInput > _rpmMaxAccepted) _rpmMaxAccepted = filteredInput;
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

double RaceSyncRpmSensor::rpm() const
{
    return _rpm;
}

bool RaceSyncRpmSensor::rpmSignalPresent() const
{
    return _rpmSignalPresent;
}

uint32_t RaceSyncRpmSensor::rpmPulseCount() const
{
    uint32_t count;
    portENTER_CRITICAL(&_rpmMux);
    count = _rpmPulseCount;
    portEXIT_CRITICAL(&_rpmMux);
    return count;
}

uint32_t RaceSyncRpmSensor::rpmLastPulseAgeMs() const
{
    return _rpmLastPulseAgeMs;
}
