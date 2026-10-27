#pragma once
#include "LapTimer.h"
#include "../config/RaceSyncTypes.h"

// The only input boundary that knows RaceSync's telemetry representation.
class GpsSampleAdapter {
public:
    static RaceSyncLaps::Sample convert(const Telemetry& telemetry);
};
