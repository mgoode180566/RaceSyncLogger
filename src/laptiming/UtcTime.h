#pragma once
#include <stdint.h>
#include <stddef.h>

namespace RaceSyncLaps {
// UTC date plus signed fractional seconds (UBX nano may be negative).
uint64_t utcMilliseconds(int year, int month, int day, int hour, int minute,
                         int second, double fractionalSeconds);
// Write ISO 8601 UTC with milliseconds. Use a buffer of at least 25 bytes.
bool formatUtcTimestamp(uint64_t milliseconds, char* text, size_t capacity);
}
