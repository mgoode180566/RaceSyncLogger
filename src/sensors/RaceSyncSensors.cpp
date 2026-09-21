#include "RaceSyncSensors.h"

bool RaceSyncSensors::begin()
{
    const bool rpmReady = _rpmSensor.begin();
    const bool throttleReady = _throttleSensor.begin();
    return rpmReady && throttleReady;
}

void RaceSyncSensors::update(Telemetry& telemetry)
{
    _rpmSensor.update(telemetry);
    _throttleSensor.update(telemetry);
}

double RaceSyncSensors::rpm() const
{
    return _rpmSensor.rpm();
}

bool RaceSyncSensors::rpmSignalPresent() const
{
    return _rpmSensor.rpmSignalPresent();
}

uint32_t RaceSyncSensors::rpmPulseCount() const
{
    return _rpmSensor.rpmPulseCount();
}

uint32_t RaceSyncSensors::rpmLastPulseAgeMs() const
{
    return _rpmSensor.rpmLastPulseAgeMs();
}
