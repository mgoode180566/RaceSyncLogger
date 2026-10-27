#include "CircuitSelector.h"
#include <limits>

namespace RaceSyncLaps {
CircuitSelection CircuitSelector::select(const Point& position,
    const std::vector<CircuitLocation>& circuits, int preferredIndex) {
    CircuitSelection result;
    if(!validPoint(position)) return result;
    double first=std::numeric_limits<double>::max(), second=first;
    int best=-1;
    for(size_t i=0;i<circuits.size();++i) {
        if(preferredIndex>=0 && static_cast<int>(i)!=preferredIndex) continue;
        const CircuitLocation& circuit=circuits[i];
        const Point reference{(circuit.point1.latitude+circuit.point2.latitude)/2,
                              (circuit.point1.longitude+circuit.point2.longitude)/2};
        const double distance=distanceMetres(position,reference);
        if(distance>circuit.radiusMetres) continue;
        if(distance<first) {second=first;first=distance;best=static_cast<int>(i);}
        else if(distance<second) second=distance;
    }
    if(best<0) return result;
    if(second-first<100) {result.state=CircuitSelection::State::Ambiguous;return result;}
    result.state=CircuitSelection::State::Selected;
    result.index=best;
    return result;
}
}
