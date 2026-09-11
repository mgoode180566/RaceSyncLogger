#include "RaceSyncGoPro.h"

#include <cstring>

RaceSyncGoPro* RaceSyncGoPro::_instance = nullptr;

namespace
{
bool isGoPro(BLEAdvertisedDevice& device)
{
    if (!device.haveName()) return false;
    const std::string name = device.getName();
    return name.rfind("GoPro ", 0) == 0;
}

int32_t readBigEndian32(const uint8_t* value)
{
    return static_cast<int32_t>(
        (static_cast<uint32_t>(value[0]) << 24) |
        (static_cast<uint32_t>(value[1]) << 16) |
        (static_cast<uint32_t>(value[2]) << 8) |
        static_cast<uint32_t>(value[3]));
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

    portENTER_CRITICAL(&_statusMux);
    _status.enabled = true;
    portEXIT_CRITICAL(&_statusMux);
    setState("DISCONNECTED");
    Serial.println("[GOPRO] Framework BLE initialised");
    return true;
}

bool RaceSyncGoPro::connect()
{
    if (!initialiseBluetooth()) return false;
    if (_client != nullptr && _client->isConnected())
    {
        setState("CONNECTED");
        return true;
    }
    return discoverAndConnect();
}

void RaceSyncGoPro::disconnect()
{
    _queryRequest = nullptr;
    if (_client != nullptr && _client->isConnected()) _client->disconnect();

    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.scanning = false;
    _status.statusValid = false;
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
    BLEScanResults results = scan->start(4, false);

    portENTER_CRITICAL(&_statusMux);
    _status.scanning = false;
    portEXIT_CRITICAL(&_statusMux);

    for (int i = 0; i < results.getCount(); ++i)
    {
        BLEAdvertisedDevice candidate = results.getDevice(i);
        if (!isGoPro(candidate)) continue;

        portENTER_CRITICAL(&_statusMux);
        _status.discovered = true;
        _status.rssi = candidate.getRSSI();
        snprintf(_status.name, sizeof(_status.name), "%s", candidate.getName().c_str());
        snprintf(_status.address, sizeof(_status.address), "%s", candidate.getAddress().toString().c_str());
        portEXIT_CRITICAL(&_statusMux);

        const bool connected = configureConnection(&candidate);
        scan->clearResults();
        return connected;
    }

    scan->clearResults();
    setState("NOT_FOUND", "No advertising GoPro found");
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
    if (!_client->connect(device))
    {
        disconnect();
        setState("NOT_CONNECTED", "BLE connection failed");
        return false;
    }

    BLERemoteService* service = _client->getService(BLEUUID(CONTROL_SERVICE_UUID));
    if (service == nullptr)
    {
        disconnect();
        setState("ERROR", "Open GoPro service unavailable");
        return false;
    }

    _queryRequest = service->getCharacteristic(BLEUUID(QUERY_REQUEST_UUID));
    BLERemoteCharacteristic* response = service->getCharacteristic(BLEUUID(QUERY_RESPONSE_UUID));
    if (_queryRequest == nullptr || response == nullptr || !response->canNotify())
    {
        disconnect();
        setState("ERROR", "Open GoPro query service incomplete");
        return false;
    }
    response->registerForNotify(notificationCallback, true);

    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    portEXIT_CRITICAL(&_statusMux);
    setState("CONNECTED");
    Serial.printf("[GOPRO] Manually connected to %s\n", device->getName().c_str());
    requestStatus();
    return true;
}

bool RaceSyncGoPro::requestStatus()
{
    if (_queryRequest == nullptr || _client == nullptr || !_client->isConnected()) return false;
    uint8_t request[] = {0x08, QUERY_STATUS_COMMAND, 6, 8, 10, 35, 70, 82, 112};
    resetResponse();
    _queryRequest->writeValue(request, sizeof(request), true);
    return true;
}

void RaceSyncGoPro::notificationCallback(BLERemoteCharacteristic*, uint8_t* data, size_t length, bool)
{
    if (_instance != nullptr) _instance->accumulateResponse(data, length);
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
    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::onDisconnect(BLEClient*)
{
    _queryRequest = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    snprintf(_status.state, sizeof(_status.state), "%s", "DISCONNECTED");
    portEXIT_CRITICAL(&_statusMux);
}
