#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

struct GoProStatus
{
    bool enabled = true;
    bool scanning = false;
    bool discovered = false;
    bool connected = false;
    bool statusValid = false;
    bool busy = false;
    bool recording = false;
    bool overheating = false;
    bool ready = false;
    bool sdCardError = false;
    int batteryPercent = -1;
    int32_t remainingVideoSeconds = -1;
    int rssi = 0;
    uint32_t lastStatusAgeMs = UINT32_MAX;
    uint32_t connectionAttempts = 0;
    uint32_t successfulQueries = 0;
    uint32_t queryErrors = 0;
    char name[32] = "";
    char address[20] = "";
    char state[20] = "STARTING";
    char lastError[64] = "";
};

// Low-priority Open GoPro BLE client. Scanning, connection and GATT work run
// outside the controller loop and are suspended while RaceSync is recording.
class RaceSyncGoPro : private NimBLEClientCallbacks
{
public:
    bool begin();
    void setRacePriorityMode(bool active);
    GoProStatus status() const;

private:
    static constexpr const char* CONTROL_SERVICE_UUID = "0000fea6-0000-1000-8000-00805f9b34fb";
    static constexpr const char* QUERY_REQUEST_UUID = "b5f90076-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr const char* QUERY_RESPONSE_UUID = "b5f90077-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr uint32_t STATUS_INTERVAL_MS = 5000;
    static constexpr uint32_t RETRY_INTERVAL_MS = 15000;
    static constexpr uint8_t QUERY_STATUS_COMMAND = 0x13;

    mutable portMUX_TYPE _statusMux = portMUX_INITIALIZER_UNLOCKED;
    GoProStatus _status;
    volatile bool _racePriorityMode = false;
    TaskHandle_t _taskHandle = nullptr;
    NimBLEClient* _client = nullptr;
    NimBLERemoteCharacteristic* _queryRequest = nullptr;
    uint32_t _lastStatusMs = 0;

    uint8_t _response[128] = {};
    size_t _responseLength = 0;
    size_t _responseExpected = 0;

    static RaceSyncGoPro* _instance;
    static void taskEntry(void* argument);
    static void notificationCallback(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool);

    void taskLoop();
    bool discoverAndConnect();
    bool configureConnection(NimBLEAdvertisedDevice* device);
    void disconnect();
    bool requestStatus();
    void accumulateResponse(const uint8_t* data, size_t length);
    void parseResponse();
    void setState(const char* state, const char* error = nullptr);
    void resetResponse();

    void onConnect(NimBLEClient* client) override;
    void onDisconnect(NimBLEClient* client) override;
};
