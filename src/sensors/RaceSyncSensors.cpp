#include "RaceSyncSensors.h"

#include "../../include/Pins.h"

volatile uint32_t RaceSyncSensors::_rpmLastPulseUs = 0;
volatile uint32_t RaceSyncSensors::_rpmPeriodUs = 0;
volatile uint32_t RaceSyncSensors::_rpmPulseCount = 0;
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
    portEXIT_CRITICAL_ISR(&_rpmMux);
}

bool RaceSyncSensors::begin()
{
    pinMode(Pin::RPM_INPUT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(Pin::RPM_INPUT), handleRpmPulse, FALLING);

    Serial.printf("[RPM] ECU tachometer capture ready on GPIO%d (%.2f pulse/rev)\n",
                  Pin::RPM_INPUT, RPM_PULSES_PER_REVOLUTION);
    return true;
}

void RaceSyncSensors::update(Telemetry& telemetry)
{
    uint32_t lastPulseUs;
    uint32_t periodUs;
    uint32_t pulseCount;

    portENTER_CRITICAL(&_rpmMux);
    lastPulseUs = _rpmLastPulseUs;
    periodUs = _rpmPeriodUs;
    pulseCount = _rpmPulseCount;
    portEXIT_CRITICAL(&_rpmMux);

    const uint32_t nowUs = micros();
    const uint32_t pulseAgeUs = lastPulseUs == 0 ? UINT32_MAX : nowUs - lastPulseUs;

    _rpmSignalPresent = lastPulseUs != 0 && pulseAgeUs <= RPM_SIGNAL_TIMEOUT_US;
    _rpmLastPulseAgeMs = lastPulseUs == 0 ? UINT32_MAX : pulseAgeUs / 1000U;

    double measuredRpm = 0.0;
    if (_rpmSignalPresent && periodUs > 0)
    {
        measuredRpm = 60000000.0 / (static_cast<double>(periodUs) * RPM_PULSES_PER_REVOLUTION);

        // Reject readings beyond the useful CB500 range.
        if (measuredRpm > 15000.0)
        {
            if (pulseCount != _rpmLastEvaluatedPulseCount &&
                _rpmRejectedReadingCount != UINT32_MAX)
            {
                ++_rpmRejectedReadingCount;
            }
            measuredRpm = 0.0;
        }
    }

    if (!_rpmSignalPresent || measuredRpm == 0.0)
    {
        _rpm = 0.0;
    }
    else if (_rpm == 0.0)
    {
        _rpm = measuredRpm;
    }
    else
    {
        // Light smoothing removes single-period jitter without hiding gear changes.
        _rpm += 0.25 * (measuredRpm - _rpm);
    }

    _rpmLastEvaluatedPulseCount = pulseCount;
    telemetry.rpmRejectedReadingCount = _rpmRejectedReadingCount;
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
