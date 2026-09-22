#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include "../config/RaceSyncTypes.h"

// Best-effort RaceChrono DIY BLE GPS publisher. The logger gives this class a
// copy of a completed GPS fix only after processing the sample. A one-element
// overwrite queue means a slow or disconnected phone can never apply back-
// pressure to GPS parsing or VBO/SD writes.
class RaceSyncRaceChronoBle
{
public:
    bool begin();
    void submitLatest(const Telemetry& telemetry);

private:
    struct GpsSnapshot
    {
        bool valid;
        bool timeValid;
        uint8_t satellites;
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint16_t milliseconds;
        double latitude;
        double longitude;
        double altitudeM;
        double speedKmh;
        double bearingDeg;
    };

    static constexpr const char* SERVICE_UUID = "00001ff8-0000-1000-8000-00805f9b34fb";
    static constexpr const char* GPS_UUID = "00000003-0000-1000-8000-00805f9b34fb";
    static constexpr const char* TIME_UUID = "00000004-0000-1000-8000-00805f9b34fb";

    QueueHandle_t _queue = nullptr;
    TaskHandle_t _task = nullptr;
    BLECharacteristic* _gpsCharacteristic = nullptr;
    BLECharacteristic* _timeCharacteristic = nullptr;
    uint32_t _previousDateAndHour = UINT32_MAX;
    uint8_t _syncBits = 0;

    static void taskEntry(void* argument);
    void taskLoop();
    void publish(const GpsSnapshot& snapshot);
    static uint16_t encodeAltitude(double metres);
    static uint16_t encodeSpeed(double kmh);
    static void writeU24(uint8_t* output, uint32_t value);
    static void writeI32(uint8_t* output, int32_t value);
};
