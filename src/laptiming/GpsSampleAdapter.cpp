#include "GpsSampleAdapter.h"
#include "UtcTime.h"

RaceSyncLaps::Sample GpsSampleAdapter::convert(const Telemetry& t) {
    const double whole=t.hour*10000.0+t.minute*100.0+t.second;
    RaceSyncLaps::Sample sample;
    sample.position={t.latitude,t.longitude};
    sample.utcMs=t.timeValid ? RaceSyncLaps::utcMilliseconds(
        t.year,t.month,t.day,t.hour,t.minute,t.second,t.rawTime-whole) : 0;
    sample.speedKmh=t.velocityKmh;
    sample.valid=t.valid && t.timeValid && t.satellites>=6 && sample.utcMs!=0;
    return sample;
}
