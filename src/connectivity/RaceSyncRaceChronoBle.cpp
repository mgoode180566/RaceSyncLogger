#include "RaceSyncRaceChronoBle.h"

#ifndef CONFIG_NIMBLE_ENABLED
#include <BLE2902.h>
#endif
#include <cmath>

namespace
{
class RaceChronoServerCallbacks : public BLEServerCallbacks
{
    void onDisconnect(BLEServer* server) override
    {
        // Restore discoverability after the phone leaves range or closes the
        // app. This callback does not touch the logger or wait for advertising.
        server->startAdvertising();
    }
};
}

bool RaceSyncRaceChronoBle::begin()
{
    _queue = xQueueCreate(1, sizeof(GpsSnapshot));
    if (_queue == nullptr)
    {
        Serial.println("[RACECHRONO] Disabled: snapshot queue allocation failed");
        return false;
    }

    if (!BLEDevice::getInitialized()) BLEDevice::init("RaceSync GPS");
    BLEDevice::setPower(ESP_PWR_LVL_P3);

    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new RaceChronoServerCallbacks());
    const BLEUUID serviceUuid(SERVICE_UUID);
    BLEService* service = server->createService(serviceUuid);
    _gpsCharacteristic = service->createCharacteristic(
        BLEUUID(GPS_UUID), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    _timeCharacteristic = service->createCharacteristic(
        BLEUUID(TIME_UUID), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#ifndef CONFIG_NIMBLE_ENABLED
    // Bluedroid requires an explicit CCCD. NimBLE creates it automatically
    // for NOTIFY characteristics and deprecates manual BLE2902 descriptors.
    _gpsCharacteristic->addDescriptor(new BLE2902());
    _timeCharacteristic->addDescriptor(new BLE2902());
#endif

    uint8_t invalidGps[20] = {0, 0, 0, 0, 0x7F, 0xFF, 0xFF, 0xFF,
                              0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                              0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t invalidTime[3] = {0, 0, 0};
    _gpsCharacteristic->setValue(invalidGps, sizeof(invalidGps));
    _timeCharacteristic->setValue(invalidTime, sizeof(invalidTime));
    service->start();

    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->setMinInterval(32);
    advertising->setMaxInterval(160);
    advertising->addServiceUUID(serviceUuid);
    advertising->setScanResponse(false);
    advertising->start();

    // Core 0, priority 1: deliberately below the Arduino loop and storage path.
    if (xTaskCreatePinnedToCore(taskEntry, "racechrono-ble", 4096, this, 1, &_task, 0) != pdPASS)
    {
        Serial.println("[RACECHRONO] Disabled: publisher task allocation failed");
        vQueueDelete(_queue);
        _queue = nullptr;
        return false;
    }

    Serial.println("[RACECHRONO] GPS-only BLE service advertising as RaceSync GPS");
    return true;
}

void RaceSyncRaceChronoBle::submitLatest(const Telemetry& telemetry)
{
    if (_queue == nullptr) return;

    const double seconds = telemetry.rawTime - floor(telemetry.rawTime);
    GpsSnapshot snapshot = {
        telemetry.valid,
        telemetry.timeValid,
        telemetry.satellites,
        telemetry.year,
        telemetry.month,
        telemetry.day,
        telemetry.hour,
        telemetry.minute,
        telemetry.second,
        static_cast<uint16_t>(constrain(lround(seconds * 1000.0), 0L, 999L)),
        telemetry.latitude,
        telemetry.longitude,
        telemetry.height,
        telemetry.velocityKmh,
        telemetry.heading
    };

    // Never wait. If BLE has not consumed the previous fix, replace it.
    xQueueOverwrite(_queue, &snapshot);
}

void RaceSyncRaceChronoBle::taskEntry(void* argument)
{
    static_cast<RaceSyncRaceChronoBle*>(argument)->taskLoop();
}

void RaceSyncRaceChronoBle::taskLoop()
{
    GpsSnapshot snapshot;
    for (;;)
    {
        if (xQueueReceive(_queue, &snapshot, portMAX_DELAY) == pdTRUE) publish(snapshot);
    }
}

void RaceSyncRaceChronoBle::publish(const GpsSnapshot& s)
{
    if (_gpsCharacteristic == nullptr || _timeCharacteristic == nullptr) return;

    uint32_t dateAndHour = 0;
    if (s.timeValid && s.year >= 2000)
    {
        dateAndHour = (static_cast<uint32_t>(s.year - 2000) * 8928U) +
                      (static_cast<uint32_t>(s.month - 1) * 744U) +
                      (static_cast<uint32_t>(s.day - 1) * 24U) + s.hour;
        if (dateAndHour != _previousDateAndHour)
        {
            _previousDateAndHour = dateAndHour;
            _syncBits = (_syncBits + 1) & 0x07;
        }
    }

    const uint32_t timeFromHour = (static_cast<uint32_t>(s.minute) * 30000U) +
                                  (static_cast<uint32_t>(s.second) * 500U) +
                                  (s.milliseconds / 2U);
    uint8_t gps[20];
    gps[0] = ((_syncBits & 0x07) << 5) | ((timeFromHour >> 16) & 0x1F);
    gps[1] = (timeFromHour >> 8) & 0xFF;
    gps[2] = timeFromHour & 0xFF;
    gps[3] = ((s.valid ? 1U : 0U) << 6) | min<uint8_t>(s.satellites, 0x3E);

    if (s.valid)
    {
        writeI32(&gps[4], static_cast<int32_t>(lround(s.latitude * 10000000.0)));
        writeI32(&gps[8], static_cast<int32_t>(lround(s.longitude * 10000000.0)));
        const uint16_t altitude = encodeAltitude(s.altitudeM);
        const uint16_t speed = encodeSpeed(s.speedKmh);
        gps[12] = altitude >> 8; gps[13] = altitude;
        gps[14] = speed >> 8; gps[15] = speed;
        const uint16_t bearing = static_cast<uint16_t>(constrain(lround(s.bearingDeg * 100.0), 0L, 35999L));
        gps[16] = bearing >> 8; gps[17] = bearing;
    }
    else
    {
        writeI32(&gps[4], INT32_MAX);
        writeI32(&gps[8], INT32_MAX);
        memset(&gps[12], 0xFF, 6);
    }
    gps[18] = 0xFF; // HDOP not currently retained by RaceSync's UBX parser.
    gps[19] = 0xFF; // VDOP not currently retained by RaceSync's UBX parser.

    _gpsCharacteristic->setValue(gps, sizeof(gps));
    _gpsCharacteristic->notify();

    if (s.timeValid)
    {
        uint8_t time[3];
        writeU24(time, ((_syncBits & 0x07) << 21) | (dateAndHour & 0x1FFFFF));
        _timeCharacteristic->setValue(time, sizeof(time));
        _timeCharacteristic->notify();
    }
}

uint16_t RaceSyncRaceChronoBle::encodeAltitude(double metres)
{
    const double shifted = max(0.0, metres + 500.0);
    if (metres <= 2776.7) return static_cast<uint16_t>(lround(shifted * 10.0)) & 0x7FFF;
    return (static_cast<uint16_t>(lround(shifted)) & 0x7FFF) | 0x8000;
}

uint16_t RaceSyncRaceChronoBle::encodeSpeed(double kmh)
{
    kmh = max(0.0, kmh);
    if (kmh <= 327.67) return static_cast<uint16_t>(lround(kmh * 100.0)) & 0x7FFF;
    return (static_cast<uint16_t>(lround(kmh * 10.0)) & 0x7FFF) | 0x8000;
}

void RaceSyncRaceChronoBle::writeU24(uint8_t* output, uint32_t value)
{
    output[0] = (value >> 16) & 0xFF;
    output[1] = (value >> 8) & 0xFF;
    output[2] = value & 0xFF;
}

void RaceSyncRaceChronoBle::writeI32(uint8_t* output, int32_t value)
{
    const uint32_t encoded = static_cast<uint32_t>(value);
    output[0] = (encoded >> 24) & 0xFF;
    output[1] = (encoded >> 16) & 0xFF;
    output[2] = (encoded >> 8) & 0xFF;
    output[3] = encoded & 0xFF;
}
