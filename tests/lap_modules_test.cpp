#include "../src/laptiming/CircuitSelector.h"
#include "../src/laptiming/UtcTime.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace RaceSyncLaps;

int main() {
    const Point cadwell(53.310286,-0.059360);
    const Point other(52.0,-1.0);
    std::vector<CircuitLocation> locations;
    assert(CircuitSelector::select(cadwell,locations).state==CircuitSelection::State::Waiting);
    locations.push_back(CircuitLocation(cadwell,Point(53.310344,-0.059583),1500));
    auto selection=CircuitSelector::select(cadwell,locations);
    assert(selection.state==CircuitSelection::State::Selected && selection.index==0);
    assert(CircuitSelector::select(other,locations).state==CircuitSelection::State::Waiting);
    // Nearby layouts need explicit selection rather than choosing arbitrarily.
    locations.push_back(CircuitLocation(cadwell,Point(53.310344,-0.059583),1500));
    assert(CircuitSelector::select(cadwell,locations).state==CircuitSelection::State::Ambiguous);
    selection=CircuitSelector::select(cadwell,locations,1);
    assert(selection.state==CircuitSelection::State::Selected && selection.index==1);
    assert(CircuitSelector::select(other,locations,1).state==CircuitSelection::State::Waiting);
    // An invalid preference does not silently select another circuit.
    assert(CircuitSelector::select(cadwell,locations,99).state==CircuitSelection::State::Waiting);
    locations.push_back(CircuitLocation(other,Point(52.0001,-1.0),1500));
    assert(CircuitSelector::select(other,locations).index==2);

    char text[32];
    const uint64_t midnight=utcMilliseconds(2026,10,28,0,0,0,0);
    assert(formatUtcTimestamp(midnight,text,sizeof(text)));
    assert(std::strcmp(text,"2026-10-28T00:00:00.000Z")==0);
    assert(formatUtcTimestamp(midnight-40,text,sizeof(text)));
    assert(std::strcmp(text,"2026-10-27T23:59:59.960Z")==0);
    assert(formatUtcTimestamp(utcMilliseconds(2028,2,29,12,34,56,.789),text,sizeof(text)));
    assert(std::strcmp(text,"2028-02-29T12:34:56.789Z")==0);
    assert(formatUtcTimestamp(utcMilliseconds(2099,12,31,23,59,59,.999),text,sizeof(text)));
    assert(std::strcmp(text,"2099-12-31T23:59:59.999Z")==0);
    char small[8];
    assert(!formatUtcTimestamp(midnight,small,sizeof(small)));
    std::cout<<"Circuit selection and UTC module tests passed\n";
}
