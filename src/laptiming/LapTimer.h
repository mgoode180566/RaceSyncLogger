#pragma once
#include <stdint.h>

namespace RaceSyncLaps {
struct Point {
    double latitude, longitude;
    Point(double lat = 0, double lon = 0) : latitude(lat), longitude(lon) {}
};
struct Settings {
    Point point1, point2;
    double minimumLapSeconds = 60;
    double minimumSpeedKmh = 10;
    double maximumSpeedKmh = 300;
    double rearmDistanceMetres = 10;
    // -1 learns direction from the first crossing; otherwise compass degrees.
    double crossingHeadingDegrees = -1;
    uint32_t maximumGapMs = 1000;
};
struct Sample {
    Point position;
    uint64_t utcMs = 0;
    double speedKmh = 0;
    bool valid = false;
};
struct Crossing {
    bool crossed = false;
    bool completedLap = false;
    uint32_t lap = 0;
    uint64_t startUtcMs = 0;
    uint64_t endUtcMs = 0;
};
bool validPoint(const Point& point);
double distanceMetres(const Point& a, const Point& b);
// UTC date plus signed fractional seconds (UBX nano may be negative).
uint64_t utcMilliseconds(int year, int month, int day, int hour, int minute,
                         int second, double fractionalSeconds);
class LapTimer {
public:
    bool configure(const Settings& settings);
    void invalidate(); // A GPS discontinuity discards the current incomplete lap.
    Crossing process(const Sample& sample);
    uint32_t completedLaps() const { return _completed; }
    bool lapRunning() const { return _lapStarted; }
private:
    Settings _settings;
    Sample _previous;
    bool _configured = false, _havePrevious = false, _armed = false, _lapStarted = false;
    int _direction = 0;
    uint32_t _completed = 0;
    uint64_t _lapStart = 0;
};
}
