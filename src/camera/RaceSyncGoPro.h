#pragma once

#include <Arduino.h>
#include <BLEDevice.h>

struct GoProStatus
{
    bool enabled = false;
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
    bool videoStartPending = false;
    bool videoStartSent = false;
    bool videoStartConfirmed = false;
    uint32_t videoStartRequests = 0;
    uint32_t videoStartErrors = 0;
    bool videoStopPending = false;
    bool videoStopSent = false;
    bool videoStopConfirmed = false;
    uint32_t videoStopRequests = 0;
    uint32_t videoStopErrors = 0;
    char name[32] = "";
    char address[20] = "";
    char state[24] = "DISABLED";
    char lastError[64] = "";
};

// Open GoPro BLE client. Bluetooth connection/discovery is manually initiated
// while RaceSync is idle. Once connected, logger transitions may queue shutter
// commands on a low-priority task; those commands never block the logging path.
class RaceSyncGoPro : private BLEClientCallbacks
{
public:
    bool connect();
    void disconnect();
    bool refreshStatus();
    bool queueVideoStart();
    bool queueVideoStop();
    GoProStatus status() const;

private:
    static constexpr const char* CONTROL_SERVICE_UUID = "0000fea6-0000-1000-8000-00805f9b34fb";
    static constexpr const char* QUERY_REQUEST_UUID = "b5f90076-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr const char* QUERY_RESPONSE_UUID = "b5f90077-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr const char* COMMAND_REQUEST_UUID = "b5f90072-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr const char* COMMAND_RESPONSE_UUID = "b5f90073-aa8d-11e3-9046-0002a5d5c51b";
    static constexpr uint8_t QUERY_STATUS_COMMAND = 0x13;

    mutable portMUX_TYPE _statusMux = portMUX_INITIALIZER_UNLOCKED;
    GoProStatus _status;
    BLEClient* _client = nullptr;
    BLERemoteCharacteristic* _queryRequest = nullptr;
    BLERemoteCharacteristic* _commandRequest = nullptr;
    TaskHandle_t _videoCommandTaskHandle = nullptr;
    bool _videoCommandStart = true;
    uint8_t _response[128] = {};
    size_t _responseLength = 0;
    size_t _responseExpected = 0;

    static RaceSyncGoPro* _instance;
    static void notificationCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
    static void commandNotificationCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
    static void videoCommandTaskEntry(void* argument);
    bool initialiseBluetooth();
    bool discoverAndConnect();
    bool configureConnection(BLEAdvertisedDevice* device);
    bool requestStatus();
    bool queueVideoCommand(bool start);
    void sendVideoCommand();
    void accumulateResponse(const uint8_t* data, size_t length);
    void parseResponse();
    void setState(const char* state, const char* error = nullptr);
    void resetResponse();

    void onConnect(BLEClient* client) override;
    void onDisconnect(BLEClient* client) override;
};
