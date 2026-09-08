#include "RaceSyncSensors.h"

bool RaceSyncSensors::begin()
{
    return _rpmSensor.begin();
}

void RaceSyncSensors::update(Telemetry& telemetry)
{
    _rpmSensor.update(telemetry);
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
