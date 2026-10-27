#include "LapTimer.h"
#include <cmath>
#include <algorithm>

namespace RaceSyncLaps {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double METRES_PER_DEGREE = 6371000.0 * PI / 180.0;
struct XY { double x, y; };
XY subtract(XY a, XY b) { return {a.x-b.x, a.y-b.y}; }
double cross(XY a, XY b) { return a.x*b.y-a.y*b.x; }
XY project(const Point& p, const Point& origin) {
    return {(p.longitude-origin.longitude)*METRES_PER_DEGREE*std::cos(origin.latitude*PI/180),
            (p.latitude-origin.latitude)*METRES_PER_DEGREE};
}
}
bool validPoint(const Point& p) {
    return std::isfinite(p.latitude) && std::isfinite(p.longitude) &&
           std::abs(p.latitude)<=90 && std::abs(p.longitude)<=180;
}
double distanceMetres(const Point& a, const Point& b) {
    XY d=project(a,b); return std::hypot(d.x,d.y);
}
bool LapTimer::configure(const Settings& s) {
    *this=LapTimer();
    const double length=distanceMetres(s.point1,s.point2);
    if(!validPoint(s.point1)||!validPoint(s.point2)||!std::isfinite(length)||length<2||length>200||
       !std::isfinite(s.minimumLapSeconds)||s.minimumLapSeconds<5||s.minimumLapSeconds>3600||
       !std::isfinite(s.minimumSpeedKmh)||s.minimumSpeedKmh<1||
       !std::isfinite(s.maximumSpeedKmh)||s.maximumSpeedKmh<s.minimumSpeedKmh||s.maximumSpeedKmh>500||
       !std::isfinite(s.rearmDistanceMetres)||s.rearmDistanceMetres<2||s.rearmDistanceMetres>100||
       !std::isfinite(s.crossingHeadingDegrees)||
       (s.crossingHeadingDegrees!=-1 && (s.crossingHeadingDegrees<0||s.crossingHeadingDegrees>=360))||
       s.maximumGapMs<40||s.maximumGapMs>2000) return false;
    _settings=s;_configured=true;return true;
}
void LapTimer::invalidate() {
    _havePrevious=false;_lapStarted=false;_armed=false;
    // Preserve completed lap numbering and learned direction for this session.
}
Crossing LapTimer::process(const Sample& sample) {
    Crossing result;
    if(!_configured) return result;
    if(!sample.valid || !validPoint(sample.position) || !sample.utcMs ||
       !std::isfinite(sample.speedKmh) || sample.speedKmh<0 || sample.speedKmh>_settings.maximumSpeedKmh) {
        invalidate();return result;
    }
    if(_havePrevious && sample.utcMs==_previous.utcMs) return result;
    if(_havePrevious && (sample.utcMs<_previous.utcMs || sample.utcMs-_previous.utcMs>_settings.maximumGapMs))
        invalidate();
    const XY gate=project(_settings.point2,_settings.point1);
    const double length=std::hypot(gate.x,gate.y);
    const XY current=project(sample.position,_settings.point1);
    const double side=cross(gate,current);
    if(!_havePrevious) {
        _previous=sample;_havePrevious=true;
        _armed=std::abs(side)/length>=_settings.rearmDistanceMetres;
        return result;
    }
    const XY previous=project(_previous.position,_settings.point1);
    const XY movement=subtract(current,previous);
    const uint64_t dt=sample.utcMs-_previous.utcMs;
    // Position jumps must also pass a speed check, independent of receiver velocity.
    if(std::hypot(movement.x,movement.y)>_settings.maximumSpeedKmh/3.6*dt/1000.0+3) {
        invalidate();return result;
    }
    const double oldSide=cross(gate,previous);
    const bool changed=(oldSide<0 && side>=0)||(oldSide>0 && side<=0);
    const double denominator=cross(movement,gate);
    if(_armed && changed && std::abs(denominator)>1e-8 && sample.speedKmh>=_settings.minimumSpeedKmh) {
        const XY toGate={-previous.x,-previous.y};
        const double alongMovement=cross(toGate,gate)/denominator;
        const double alongGate=cross(toGate,movement)/denominator;
        const int direction=side>oldSide?1:-1;
        bool allowed=_direction==0 || _direction==direction;
        if(_settings.crossingHeadingDegrees>=0) {
            const double heading=_settings.crossingHeadingDegrees*PI/180;
            allowed=allowed && movement.x*std::sin(heading)+movement.y*std::cos(heading)>0;
        }
        if(allowed && alongMovement>=0 && alongMovement<=1 && alongGate>=0 && alongGate<=1) {
            const uint64_t crossing=_previous.utcMs+static_cast<uint64_t>(std::llround(alongMovement*dt));
            if(!_lapStarted || crossing-_lapStart>=_settings.minimumLapSeconds*1000) {
                result.crossed=true;result.endUtcMs=crossing;
                if(_lapStarted) {
                    result.completedLap=true;result.lap=++_completed;result.startUtcMs=_lapStart;
                }
                _lapStart=crossing;_lapStarted=true;_direction=direction;
            }
            _armed=false;
        }
    }
    // Rearm only after leaving the gate area, never on the same crossing sample.
    else if(std::abs(side)/length>=_settings.rearmDistanceMetres) _armed=true;
    _previous=sample;
    return result;
}
}
