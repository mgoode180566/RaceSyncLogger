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
// commands on low-priority tasks; those commands never block the logging path.
class RaceSyncGoPro : private BLEClientCallbacks
{
public:
    bool connect();
    void disconnect();
    bool refreshStatus();
    bool queueVideoStart();

    bool queueVideoStop()
    {
        const GoProStatus current = status();
        if (current.connected && current.statusValid && !current.recording)
        {
            portENTER_CRITICAL(&_statusMux);
            _status.videoStopPending = false;
            _status.videoStopSent = false;
            _status.videoStopConfirmed = true;
            portEXIT_CRITICAL(&_statusMux);
            Serial.println("[GOPRO] Video already stopped; no shutter-off command required");
            return true;
        }

        if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr)
        {
            portENTER_CRITICAL(&_statusMux);
            _status.videoStopErrors++;
            portEXIT_CRITICAL(&_statusMux);
            Serial.println("[GOPRO] Video stop not queued: camera is not connected");
            return false;
        }
        if (_videoStopTaskHandle != nullptr) return false;

        portENTER_CRITICAL(&_statusMux);
        _status.videoStopPending = true;
        _status.videoStopSent = false;
        _status.videoStopConfirmed = false;
        _status.videoStopRequests++;
        portEXIT_CRITICAL(&_statusMux);

        const BaseType_t created = xTaskCreatePinnedToCore(
            [](void* argument)
            {
                RaceSyncGoPro* self = static_cast<RaceSyncGoPro*>(argument);
                if (self->_client == nullptr || !self->_client->isConnected() || self->_commandRequest == nullptr)
                {
                    portENTER_CRITICAL(&self->_statusMux);
                    self->_status.videoStopPending = false;
                    self->_status.videoStopErrors++;
                    portEXIT_CRITICAL(&self->_statusMux);
                    self->_videoStopTaskHandle = nullptr;
                    vTaskDelete(nullptr);
                    return;
                }

                // Official Open GoPro Set Shutter command with enable=0.
                uint8_t request[] = {0x03, 0x01, 0x01, 0x00};
                self->_commandRequest->writeValue(request, sizeof(request), true);
                portENTER_CRITICAL(&self->_statusMux);
                self->_status.videoStopSent = true;
                portEXIT_CRITICAL(&self->_statusMux);
                Serial.println("[GOPRO] Video stop command write requested");

                // The existing command callback was written for shutter-on and
                // cannot distinguish the requested parameter. Finalise the stop
                // state here after the camera has had time to acknowledge it.
                vTaskDelay(pdMS_TO_TICKS(1500));
                if (self->_client != nullptr && self->_client->isConnected())
                {
                    portENTER_CRITICAL(&self->_statusMux);
                    self->_status.videoStopPending = false;
                    self->_status.videoStopConfirmed = true;
                    self->_status.recording = false;
                    self->_status.lastError[0] = '\0';
                    portEXIT_CRITICAL(&self->_statusMux);
                    Serial.println("[GOPRO] Video stop command completed");
                }
                else
                {
                    portENTER_CRITICAL(&self->_statusMux);
                    self->_status.videoStopPending = false;
                    self->_status.videoStopErrors++;
                    portEXIT_CRITICAL(&self->_statusMux);
                    Serial.println("[GOPRO] Video stop could not be confirmed: BLE disconnected");
                }

                self->_videoStopTaskHandle = nullptr;
                vTaskDelete(nullptr);
            },
            "gopro-video-stop", 4096, this, 1, &_videoStopTaskHandle, 0);

        if (created == pdPASS) return true;

        _videoStopTaskHandle = nullptr;
        portENTER_CRITICAL(&_statusMux);
        _status.videoStopPending = false;
        _status.videoStopErrors++;
        portEXIT_CRITICAL(&_statusMux);
        Serial.println("[GOPRO] Unable to queue GoPro video stop");
        return false;
    }

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
    TaskHandle_t _videoStartTaskHandle = nullptr;
    TaskHandle_t _videoStopTaskHandle = nullptr;
    uint8_t _response[128] = {};
    size_t _responseLength = 0;
    size_t _responseExpected = 0;

    static RaceSyncGoPro* _instance;
    static void notificationCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
    static void commandNotificationCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
    static void videoStartTaskEntry(void* argument);
    bool initialiseBluetooth();
    bool discoverAndConnect();
    bool configureConnection(BLEAdvertisedDevice* device);
    bool requestStatus();
    void sendVideoStart();
    void accumulateResponse(const uint8_t* data, size_t length);
    void parseResponse();
    void setState(const char* state, const char* error = nullptr);
    void resetResponse();

    void onConnect(BLEClient* client) override;
    void onDisconnect(BLEClient* client) override;
};
