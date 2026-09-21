#include "RaceSyncThrottleSensor.h"
#include <Preferences.h>
#include "../../include/Pins.h"

bool RaceSyncThrottleSensor::begin()
{
    pinMode(Pin::TPS_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(Pin::TPS_ADC, ADC_11db);
    Preferences p;
    if (p.begin("racesync", true))
    {
        _closedRaw = p.getUShort("tpsClosed", 0);
        _openRaw = p.getUShort("tpsOpen", 0);
        p.end();
    }
    _raw = analogRead(Pin::TPS_ADC);
    _filteredRaw = _raw;
    Serial.printf("[TPS] GPIO%d raw=%u calibration=%s (%u..%u)\n", Pin::TPS_ADC, _raw,
                  calibrated() ? "ready" : "required", _closedRaw, _openRaw);
    return true;
}

bool RaceSyncThrottleSensor::calibrated() const
{
    return abs(static_cast<int>(_openRaw) - static_cast<int>(_closedRaw)) >= MIN_CALIBRATION_SPAN;
}

void RaceSyncThrottleSensor::update(Telemetry& t)
{
    const uint32_t now = micros();
    if (now - _lastSampleUs >= SAMPLE_INTERVAL_US)
    {
        _lastSampleUs = now;
        _raw = analogRead(Pin::TPS_ADC);
        _filteredRaw += FILTER_ALPHA * (static_cast<float>(_raw) - _filteredRaw);
        // A healthy 3.3 V potentiometric sensor should not remain pinned at a rail.
        _connected = _raw > 8 && _raw < 4087;
        if (calibrated())
        {
            const float span = static_cast<float>(static_cast<int>(_openRaw) - static_cast<int>(_closedRaw));
            _percent = 100.0f * (_filteredRaw - _closedRaw) / span;
            _percent = constrain(_percent, 0.0f, 100.0f);
        }
        else _percent = 0.0f;
    }
    t.throttleRaw = _raw;
    t.throttleFilteredRaw = filteredRaw();
    t.throttleClosedRaw = _closedRaw;
    t.throttleOpenRaw = _openRaw;
    t.throttleCalibrated = calibrated();
    t.throttleConnected = _connected;
    t.throttlePercent = _percent;
}

bool RaceSyncThrottleSensor::saveCalibration()
{
    Preferences p;
    if (!p.begin("racesync", false)) return false;
    const bool ok = p.putUShort("tpsClosed", _closedRaw) != 0 && p.putUShort("tpsOpen", _openRaw) != 0;
    p.end();
    return ok;
}

bool RaceSyncThrottleSensor::calibrateClosed()
{
    _closedRaw = filteredRaw();
    return saveCalibration();
}

bool RaceSyncThrottleSensor::calibrateOpen()
{
    _openRaw = filteredRaw();
    if (!calibrated()) return false;
    return saveCalibration();
}

bool RaceSyncThrottleSensor::clearCalibration()
{
    _closedRaw = _openRaw = 0;
    _percent = 0.0f;
    Preferences p;
    if (!p.begin("racesync", false)) return false;
    p.remove("tpsClosed");
    p.remove("tpsOpen");
    p.end();
    return true;
}
