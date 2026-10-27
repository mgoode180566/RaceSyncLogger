#include "../src/laptiming/LapTimer.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace RaceSyncLaps;
namespace {
constexpr double DEGREE_METRES=111194.92664455874;
Point metres(double east,double north) {return {north/DEGREE_METRES,east/DEGREE_METRES};}
Settings settings() {
    Settings s;s.point1=metres(-10,0);s.point2=metres(10,0);
    s.minimumLapSeconds=60;s.crossingHeadingDegrees=0;return s;
}
struct Run {
    LapTimer timer;Sample sample;Crossing last;unsigned starts=0,finishes=0;
    Run() {assert(timer.configure(settings()));sample.position=metres(0,-20);sample.utcMs=1000000;sample.speedKmh=150;sample.valid=true;timer.process(sample);}
    void point(Point p,uint64_t t) {
        sample.position=p;sample.utcMs=t;auto c=timer.process(sample);
        if(c.crossed) {last=c;++starts;if(c.completedLap)++finishes;}
    }
    void move(double east,double north,uint32_t duration) {
        Point from=sample.position,to=metres(east,north);uint64_t start=sample.utcMs;
        assert(duration%100==0);
        for(uint32_t t=100;t<=duration;t+=100) {
            const double f=double(t)/duration;
            point({from.latitude+(to.latitude-from.latitude)*f,from.longitude+(to.longitude-from.longitude)*f},start+t);
        }
    }
    void lap() {
        move(0,20,1000);move(40,20,19000);move(40,-20,20000);move(0,-20,20000);
    }
};
}
int main() {
    // First crossing starts timing; successive crossings interpolate to the ms.
    Run run;run.lap();assert(run.starts==1 && run.finishes==0);
    run.move(0,20,1000);assert(run.finishes==1);
    assert(run.last.endUtcMs-run.last.startUtcMs==60000);
    assert(run.last.lap==1 && run.last.startUtcMs==1000500);
    // A reverse crossing after leaving the gate area does not start/finish a lap.
    Run reverse;reverse.move(0,20,1000);reverse.move(0,20,61000);reverse.move(0,-20,1000);
    assert(reverse.starts==1 && reverse.finishes==0);
    // Crossing the infinite extension outside the actual endpoints is ignored.
    Run outside;outside.move(40,-20,1000);outside.move(40,20,1000);assert(outside.starts==0);
    // Exact-on-line sample is counted once and duplicate timestamps are ignored.
    Run exact;exact.move(0,0,1000);assert(exact.starts==1);
    exact.timer.process(exact.sample);exact.move(0,20,1000);assert(exact.starts==1);
    // An early crossing cannot produce an implausibly short lap.
    Run early;early.move(0,20,1000);early.move(40,20,1000);early.move(40,-20,1000);
    early.move(0,-20,1000);early.move(0,20,1000);assert(early.finishes==0);
    // A missing fix or time gap discards an incomplete lap; never bridge a gap.
    Run gap;gap.move(0,20,1000);gap.sample.utcMs+=2000;
    gap.point(metres(0,-20),gap.sample.utcMs);gap.move(0,20,1000);assert(gap.finishes==0);
    Run invalid;invalid.move(0,20,1000);invalid.sample.valid=false;invalid.timer.process(invalid.sample);
    assert(!invalid.timer.lapRunning());
    // Position jump across the line is rejected even with plausible reported speed.
    Run jump;jump.point(metres(0,20),jump.sample.utcMs+40);assert(jump.starts==0);
    // UTC works across midnight, month/year boundaries, leap day and signed nano.
    assert(utcMilliseconds(2026,10,28,0,0,0,0)-utcMilliseconds(2026,10,27,23,59,59,.960)==40);
    assert(utcMilliseconds(2027,1,1,0,0,0,-.04)==utcMilliseconds(2026,12,31,23,59,59,.96));
    assert(utcMilliseconds(2028,3,1,0,0,0,0)-utcMilliseconds(2028,2,29,0,0,0,0)==86400000);
    assert(utcMilliseconds(2026,2,29,0,0,0,0)==0);
    assert(utcMilliseconds(2026,10,27,0,0,0,0)==1793059200000ULL);
    // Bad gates and non-finite inputs cannot configure the timer.
    Settings bad=settings();bad.point2=bad.point1;assert(!run.timer.configure(bad));
    bad=settings();bad.minimumLapSeconds=NAN;assert(!run.timer.configure(bad));
    // Use the user's exact Cadwell endpoints without changing either one.
    Settings cadwell;cadwell.point1={53.310286,-.059360};cadwell.point2={53.310344,-.059583};
    assert(run.timer.configure(cadwell));
    // Direction can alternatively be learned on the first valid crossing.
    Run learned;Settings learn=settings();learn.crossingHeadingDegrees=-1;
    assert(learned.timer.configure(learn));learned.timer.process(learned.sample);
    learned.move(0,20,1000);assert(learned.starts==1);
    std::cout<<"Lap timer tests passed\n";
}
