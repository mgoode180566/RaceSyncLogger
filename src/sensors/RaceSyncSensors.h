#pragma once
#include <Arduino.h>

#include "../config/RaceSyncTypes.h"
#include "RaceSyncRpmSensor.h"
#include "RaceSyncThrottleSensor.h"

// Lightweight composition layer for the physical sensors used by RaceSync.
// Dependencies are supplied explicitly by reference: no heap allocation,
// service locator, virtual dispatch or hidden ownership is involved.
class RaceSyncSensors
{
public:
    RaceSyncSensors(RaceSyncRpmSensor& rpmSensor, RaceSyncThrottleSensor& throttleSensor)
        : _rpmSensor(rpmSensor), _throttleSensor(throttleSensor)
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
    RaceSyncThrottleSensor& _throttleSensor;
};
