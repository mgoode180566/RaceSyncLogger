#include "RaceSyncGoPro.h"

#include <cstring>
#include <esp_arduino_version.h>
#include <BLESecurity.h>

RaceSyncGoPro* RaceSyncGoPro::_instance = nullptr;

namespace
{
bool isGoPro(BLEAdvertisedDevice& device)
{
    // Open GoPro specifies 0xFEA6 as the BLE advertising service used for
    // discovery. The local name is scan-response data and may not always be
    // available when the advertisement is first seen, so it must not be the
    // only discovery criterion.
    static BLEUUID goProAdvertisingService("0000fea6-0000-1000-8000-00805f9b34fb");
    if (device.haveServiceUUID() && device.isAdvertisingService(goProAdvertisingService)) return true;

    // Keep the name check as a fallback for BLE stacks that do not expose the
    // advertised service UUID from the scan result.
    if (!device.haveName()) return false;
    const String name(device.getName().c_str());
    return name.startsWith("GoPro ");
}

int32_t readBigEndian32(const uint8_t* value)
{
    return static_cast<int32_t>(
        (static_cast<uint32_t>(value[0]) << 24) |
        (static_cast<uint32_t>(value[1]) << 16) |
        (static_cast<uint32_t>(value[2]) << 8) |
        static_cast<uint32_t>(value[3]));
}

void configureGoProBleSecurity()
{
    // Open GoPro requires the BLE client to pair before subscribing to or
    // writing the Control & Query characteristics. HERO9 uses a Just Works
    // style bond, so no display or passkey capability is required here.
    static BLESecurity* security = nullptr;
    if (security != nullptr) return;

    security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    security->setAuthenticationMode(true, false, true);
#if defined(CONFIG_BLUEDROID_ENABLED)
    BLESecurity::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
#endif
#else
    security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
#endif

    Serial.println("[GOPRO] BLE security configured: bonded encrypted connection requested");
}
}

bool RaceSyncGoPro::initialiseBluetooth()
{
    if (status().enabled) return true;

    Serial.println("[GOPRO] Manual request: initialising framework BLE");
    setState("INITIALISING");
    _instance = this;
    BLEDevice::init("RaceSync");
    BLEDevice::setPower(ESP_PWR_LVL_P3);
    configureGoProBleSecurity();

    portENTER_CRITICAL(&_statusMux);
    _status.enabled = true;
    portEXIT_CRITICAL(&_statusMux);
    setState("DISCONNECTED");
    Serial.println("[GOPRO] Framework BLE initialised");
    return true;
}

bool RaceSyncGoPro::connect()
{
    Serial.println("[GOPRO] Connect requested from Web API");
    if (!initialiseBluetooth()) return false;
    if (_client != nullptr && _client->isConnected())
    {
        const GoProStatus current = status();
        if (current.connected && current.statusValid)
        {
            setState("CONNECTED");
            Serial.println("[GOPRO] Existing authenticated GoPro connection is already active");
            return true;
        }

        Serial.println("[GOPRO] Existing BLE link is not fully configured; reconnecting");
        _client->disconnect();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    return discoverAndConnect();
}

void RaceSyncGoPro::disconnect()
{
    _queryRequest = nullptr;
    _commandRequest = nullptr;
    if (_client != nullptr && _client->isConnected()) _client->disconnect();

    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.scanning = false;
    _status.statusValid = false;
    _status.videoStartPending = false;
    portEXIT_CRITICAL(&_statusMux);
    resetResponse();
    setState(status().enabled ? "DISCONNECTED" : "DISABLED");
    Serial.println("[GOPRO] Manual disconnect complete");
}

bool RaceSyncGoPro::refreshStatus()
{
    if (!requestStatus())
    {
        portENTER_CRITICAL(&_statusMux);
        _status.queryErrors++;
        portEXIT_CRITICAL(&_statusMux);
        setState("DISCONNECTED", "Connect the GoPro before refreshing status");
        return false;
    }
    return true;
}

bool RaceSyncGoPro::queueVideoStart()
{
    if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.videoStartErrors++;
        portEXIT_CRITICAL(&_statusMux);
        setState("CONNECTED", "GoPro must be connected before starting manual logging");
        return false;
    }
    if (_videoStartTaskHandle != nullptr) return false;

    portENTER_CRITICAL(&_statusMux);
    _status.videoStartPending = true;
    _status.videoStartSent = false;
    _status.videoStartConfirmed = false;
    _status.videoStartRequests++;
    portEXIT_CRITICAL(&_statusMux);

    const BaseType_t created = xTaskCreatePinnedToCore(
        videoStartTaskEntry, "gopro-video-start", 4096, this, 1,
        &_videoStartTaskHandle, 0);
    if (created == pdPASS) return true;

    _videoStartTaskHandle = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.videoStartPending = false;
    _status.videoStartErrors++;
    portEXIT_CRITICAL(&_statusMux);
    setState("CONNECTED", "Unable to queue GoPro video start");
    return false;
}

void RaceSyncGoPro::videoStartTaskEntry(void* argument)
{
    static_cast<RaceSyncGoPro*>(argument)->sendVideoStart();
}

void RaceSyncGoPro::sendVideoStart()
{
    // Official Open GoPro Set Shutter command: length, command, parameter,
    // enable. This task is low priority and never blocks the logging caller.
    uint8_t request[] = {0x03, 0x01, 0x01, 0x01};
    _commandRequest->writeValue(request, sizeof(request), true);

    portENTER_CRITICAL(&_statusMux);
    _status.videoStartSent = true;
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] Video start command write requested");

    vTaskDelay(pdMS_TO_TICKS(1500));
    portENTER_CRITICAL(&_statusMux);
    if (_status.videoStartPending)
    {
        _status.videoStartPending = false;
        _status.videoStartErrors++;
        snprintf(_status.lastError, sizeof(_status.lastError), "%s", "No GoPro video-start confirmation");
    }
    portEXIT_CRITICAL(&_statusMux);

    _videoStartTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

GoProStatus RaceSyncGoPro::status() const
{
    portENTER_CRITICAL(&_statusMux);
    GoProStatus copy = _status;
    portEXIT_CRITICAL(&_statusMux);
    if (copy.statusValid && copy.lastStatusAgeMs != UINT32_MAX)
    {
        copy.lastStatusAgeMs = millis() - copy.lastStatusAgeMs;
    }
    return copy;
}

bool RaceSyncGoPro::discoverAndConnect()
{
    portENTER_CRITICAL(&_statusMux);
    _status.scanning = true;
    _status.discovered = false;
    _status.connectionAttempts++;
    portEXIT_CRITICAL(&_statusMux);
    setState("SCANNING");

    BLEScan* scan = BLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(40);

    Serial.println("[GOPRO] BLE scan started (10 seconds). Camera must be advertising/pairable.");
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    BLEScanResults* results = scan->start(10, false);
#else
    BLEScanResults resultStorage = scan->start(10, false);
    BLEScanResults* results = &resultStorage;
#endif

    portENTER_CRITICAL(&_statusMux);
    _status.scanning = false;
    portEXIT_CRITICAL(&_statusMux);

    const int resultCount = results != nullptr ? results->getCount() : 0;
    Serial.printf("[GOPRO] BLE scan complete: %d device(s) found\n", resultCount);

    for (int i = 0; i < resultCount; ++i)
    {
        BLEAdvertisedDevice candidate = results->getDevice(i);
        const bool goPro = isGoPro(candidate);
        const String candidateName = candidate.haveName() ? String(candidate.getName().c_str()) : String("<unnamed>");
        const String candidateAddress(candidate.getAddress().toString().c_str());

        Serial.printf("[GOPRO] BLE device %d: name='%s' address=%s RSSI=%d%s\n",
                      i + 1,
                      candidateName.c_str(),
                      candidateAddress.c_str(),
                      candidate.getRSSI(),
                      goPro ? " [GoPro FEA6/name match]" : "");

        if (!goPro) continue;

        portENTER_CRITICAL(&_statusMux);
        _status.discovered = true;
        _status.rssi = candidate.getRSSI();
        snprintf(_status.name, sizeof(_status.name), "%s", candidateName.c_str());
        snprintf(_status.address, sizeof(_status.address), "%s", candidateAddress.c_str());
        portEXIT_CRITICAL(&_statusMux);

        Serial.printf("[GOPRO] GoPro advertisement found; connecting to %s (%s)\n",
                      candidateName.c_str(), candidateAddress.c_str());

        const bool connected = configureConnection(&candidate);
        scan->clearResults();
        return connected;
    }

    scan->clearResults();
    setState("NOT_FOUND", "No GoPro advertising; put camera in pairing mode");
    Serial.println("[GOPRO] No GoPro advertisement found. On HERO9 open Connections > Connect Device > GoPro Quik App, then retry.");
    return false;
}

bool RaceSyncGoPro::configureConnection(BLEAdvertisedDevice* device)
{
    setState("CONNECTING");
    if (_client == nullptr) _client = BLEDevice::createClient();
    if (_client == nullptr)
    {
        setState("ERROR", "Unable to create BLE client");
        return false;
    }

    _client->setClientCallbacks(this);
    Serial.println("[GOPRO] Opening BLE link; pairing/bonding will be requested if this is a new client");
    if (!_client->connect(device))
    {
        disconnect();
        setState("NOT_CONNECTED", "BLE connection failed");
        Serial.println("[GOPRO] BLE link connection failed");
        return false;
    }

    // The BLE security configuration requests encryption from the connection
    // event. Give SMP enough time to complete before touching protected CCCDs.
    setState("PAIRING");
    Serial.println("[GOPRO] BLE link connected; waiting for pairing/encryption");
    vTaskDelay(pdMS_TO_TICKS(1200));

    Serial.println("[GOPRO] Discovering Open GoPro service");
    BLERemoteService* service = _client->getService(BLEUUID(CONTROL_SERVICE_UUID));
    if (service == nullptr)
    {
        disconnect();
        setState("ERROR", "Open GoPro service unavailable");
        Serial.println("[GOPRO] Open GoPro FEA6 service not available after connection");
        return false;
    }

    _queryRequest = service->getCharacteristic(BLEUUID(QUERY_REQUEST_UUID));
    _commandRequest = service->getCharacteristic(BLEUUID(COMMAND_REQUEST_UUID));
    BLERemoteCharacteristic* response = service->getCharacteristic(BLEUUID(QUERY_RESPONSE_UUID));
    BLERemoteCharacteristic* commandResponse = service->getCharacteristic(BLEUUID(COMMAND_RESPONSE_UUID));
    if (_queryRequest == nullptr || _commandRequest == nullptr || response == nullptr ||
        commandResponse == nullptr || !response->canNotify() || !commandResponse->canNotify())
    {
        disconnect();
        setState("ERROR", "Open GoPro query service incomplete");
        Serial.println("[GOPRO] Required Open GoPro command/query characteristics are incomplete");
        return false;
    }

    // Arduino-ESP32 BLE 3.3.11 exposes registerForNotify() as void. A failed
    // CCCD write is reported by the BLE stack itself, so usability is verified
    // by the initial status query and the notification callback that follows.
    Serial.println("[GOPRO] Subscribing to Open GoPro response notifications");
    response->registerForNotify(notificationCallback, true);
    commandResponse->registerForNotify(commandNotificationCallback, true);

    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    portEXIT_CRITICAL(&_statusMux);
    setState("CONNECTED");
    Serial.printf("[GOPRO] Open GoPro notification registration requested: %s\n",
                  device->haveName() ? device->getName().c_str() : "GoPro");

    if (!requestStatus())
    {
        setState("ERROR", "Unable to issue initial GoPro status query");
        return false;
    }
    return true;
}

bool RaceSyncGoPro::requestStatus()
{
    if (_queryRequest == nullptr || _client == nullptr || !_client->isConnected()) return false;
    uint8_t request[] = {0x08, QUERY_STATUS_COMMAND, 6, 8, 10, 35, 70, 82, 112};
    resetResponse();
    _queryRequest->writeValue(request, sizeof(request), true);
    Serial.println("[GOPRO] Status query write requested; awaiting notification");
    return true;
}

void RaceSyncGoPro::notificationCallback(BLERemoteCharacteristic*, uint8_t* data, size_t length, bool)
{
    if (_instance != nullptr) _instance->accumulateResponse(data, length);
}

void RaceSyncGoPro::commandNotificationCallback(BLERemoteCharacteristic*, uint8_t* data, size_t length, bool)
{
    if (_instance == nullptr || data == nullptr || length < 3 || data[1] != 0x01) return;

    portENTER_CRITICAL(&_instance->_statusMux);
    _instance->_status.videoStartPending = false;
    _instance->_status.videoStartConfirmed = data[2] == 0x00;
    if (data[2] == 0x00)
    {
        _instance->_status.recording = true;
        _instance->_status.lastError[0] = '\0';
    }
    else
    {
        _instance->_status.videoStartErrors++;
        snprintf(_instance->_status.lastError, sizeof(_instance->_status.lastError),
                 "GoPro rejected video start (code %u)", data[2]);
    }
    portEXIT_CRITICAL(&_instance->_statusMux);
}

void RaceSyncGoPro::accumulateResponse(const uint8_t* data, size_t length)
{
    if (data == nullptr || length == 0) return;
    size_t offset = 0;
    if ((data[0] & 0x80U) != 0) offset = 1;
    else
    {
        resetResponse();
        const uint8_t header = (data[0] >> 5) & 0x03U;
        if (header == 0 && length >= 1) { _responseExpected = data[0] & 0x1FU; offset = 1; }
        else if (header == 1 && length >= 2) { _responseExpected = ((data[0] & 0x1FU) << 8) | data[1]; offset = 2; }
        else if (header == 2 && length >= 3) { _responseExpected = (static_cast<size_t>(data[1]) << 8) | data[2]; offset = 3; }
        else return;
    }

    const size_t available = length - offset;
    const size_t room = sizeof(_response) - _responseLength;
    const size_t copyLength = available < room ? available : room;
    memcpy(_response + _responseLength, data + offset, copyLength);
    _responseLength += copyLength;

    if (_responseExpected > sizeof(_response))
    {
        setState("CONNECTED", "GoPro response too large");
        resetResponse();
        return;
    }
    if (_responseExpected > 0 && _responseLength >= _responseExpected) parseResponse();
}

void RaceSyncGoPro::parseResponse()
{
    if (_responseLength < 2 || _response[0] != QUERY_STATUS_COMMAND || _response[1] != 0)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.queryErrors++;
        portEXIT_CRITICAL(&_statusMux);
        setState("CONNECTED", "Invalid GoPro status response");
        resetResponse();
        return;
    }

    GoProStatus updated = status();
    size_t cursor = 2;
    while (cursor + 2 <= _responseLength)
    {
        const uint8_t id = _response[cursor++];
        const uint8_t valueLength = _response[cursor++];
        if (cursor + valueLength > _responseLength) break;
        const uint8_t* value = _response + cursor;
        if (valueLength >= 1)
        {
            switch (id)
            {
                case 6: updated.overheating = value[0] != 0; break;
                case 8: updated.busy = value[0] != 0; break;
                case 10: updated.recording = value[0] != 0; break;
                case 35: if (valueLength >= 4) updated.remainingVideoSeconds = readBigEndian32(value); break;
                case 70: updated.batteryPercent = value[0]; break;
                case 82: updated.ready = value[0] != 0; break;
                case 112: updated.sdCardError = value[0] != 0; break;
                default: break;
            }
        }
        cursor += valueLength;
    }

    updated.connected = true;
    updated.statusValid = true;
    updated.lastStatusAgeMs = millis();
    updated.successfulQueries++;
    snprintf(updated.state, sizeof(updated.state), "%s", "CONNECTED");
    updated.lastError[0] = '\0';
    portENTER_CRITICAL(&_statusMux);
    _status = updated;
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] Status response received and parsed");
    resetResponse();
}

void RaceSyncGoPro::setState(const char* state, const char* error)
{
    portENTER_CRITICAL(&_statusMux);
    snprintf(_status.state, sizeof(_status.state), "%s", state);
    if (error != nullptr) snprintf(_status.lastError, sizeof(_status.lastError), "%s", error);
    else _status.lastError[0] = '\0';
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::resetResponse()
{
    _responseLength = 0;
    _responseExpected = 0;
}

void RaceSyncGoPro::onConnect(BLEClient*)
{
    // A raw BLE link is not enough for Open GoPro. The connection is only
    // reported as usable after pairing plus notification subscriptions succeed.
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::onDisconnect(BLEClient*)
{
    _queryRequest = nullptr;
    _commandRequest = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    snprintf(_status.state, sizeof(_status.state), "%s", "DISCONNECTED");
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] BLE disconnected");
}
