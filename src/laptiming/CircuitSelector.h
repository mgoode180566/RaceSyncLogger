#pragma once
#include <vector>
#include <stddef.h>
#include "LapTimer.h"

namespace RaceSyncLaps {
struct CircuitLocation {
    Point point1, point2;
    double radiusMetres;
    CircuitLocation(Point a, Point b, double radius) : point1(a), point2(b), radiusMetres(radius) {}
};
struct CircuitSelection {
    enum class State { Waiting, Ambiguous, Selected };
    State state = State::Waiting;
    int index = -1;
};
// Stateless selection policy: no JSON, SD card, Arduino or session dependencies.
// The coordinator locks a successful selection for the rest of the session.
class CircuitSelector {
public:
    static CircuitSelection select(const Point& position,
        const std::vector<CircuitLocation>& circuits, int preferredIndex = -1);
};
}
