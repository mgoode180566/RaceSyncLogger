#pragma once

#include "../config/RaceSyncTypes.h"
#include "RaceSyncRpmSensor.h"

// Lightweight composition layer for the physical sensors used by RaceSync.
// Dependencies are supplied explicitly by reference: no heap allocation,
// service locator, virtual dispatch or hidden ownership is involved.
class RaceSyncSensors
{
public:
    explicit RaceSyncSensors(RaceSyncRpmSensor& rpmSensor)
        : _rpmSensor(rpmSensor)
    {
    }

    bool begin();
    void update(Telemetry& telemetry);

    double rpm() const;
    bool rpmSignalPresent() const;
    uint32_t rpmPulseCount() const;
    uint32_t rpmLastPulseAgeMs() const;

private:
    RaceSyncRpmSensor& _rpmSensor;
};
