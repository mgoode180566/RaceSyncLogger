#pragma once
#include <Arduino.h>

enum class DataMode
{
    STARTING,
    LIVE
};

inline const char* dataModeName(DataMode mode)
{
    return mode == DataMode::LIVE ? "LIVE" : "STARTING";
}

struct Telemetry
{
    bool valid = false;
    bool timeValid = false;

    uint8_t satellites = 0;
    uint8_t solutionType = 0;

    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;
    uint8_t second = 0;

    double rawTime = 0.0;
    double rawLatitude = 0.0;
    double rawLongitude = 0.0;

    double latitude = 0.0;
    double longitude = 0.0;

    double velocityKmh = 0.0;
    double heading = 0.0;
    double height = 0.0;
    double verticalVelocityMs = 0.0;

    double samplePeriod = 0.040;

    int aviFileIndex = 0;
    double aviTime = 0.0;

    double comboAcc = 0.0;
    double oilPressure = 0.0;
    double oilTemperature = 0.0;
    double waterTemperature = 0.0;
    double revs = 0.0;
    double fuelPressure = 0.0;
    double comboG = 0.0;

    uint32_t sampleIndex = 0;
    String rawVBoxLine;
};
