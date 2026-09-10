#include "RaceSyncGoPro.h"

#include <cstring>

RaceSyncGoPro* RaceSyncGoPro::_instance = nullptr;

namespace
{
bool isGoPro(const NimBLEAdvertisedDevice& device)
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

bool RaceSyncGoPro::begin()
{
    if (_taskHandle != nullptr) return true;
    _instance = this;
    BaseType_t created = xTaskCreatePinnedToCore(taskEntry, "gopro-status", 6144, this, 1, &_taskHandle, 0);
    if (created != pdPASS)
    {
        setState("ERROR", "Unable to create BLE task");
        return false;
    }
    return true;
}

void RaceSyncGoPro::setRacePriorityMode(bool active)
{
    _racePriorityMode = active;
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

void RaceSyncGoPro::taskEntry(void* argument)
{
    static_cast<RaceSyncGoPro*>(argument)->taskLoop();
}

void RaceSyncGoPro::taskLoop()
{
    NimBLEDevice::init("RaceSync");
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    setState("SEARCHING");

    for (;;)
    {
        if (_racePriorityMode)
        {
            disconnect();
            setState("SUSPENDED");
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        if (_client == nullptr || !_client->isConnected())
        {
            if (!discoverAndConnect())
            {
                vTaskDelay(pdMS_TO_TICKS(RETRY_INTERVAL_MS));
                continue;
            }
            _lastStatusMs = 0;
        }

        const uint32_t now = millis();
        if (_lastStatusMs == 0 || now - _lastStatusMs >= STATUS_INTERVAL_MS)
        {
            if (!requestStatus())
            {
                portENTER_CRITICAL(&_statusMux);
                _status.queryErrors++;
                portEXIT_CRITICAL(&_statusMux);
                setState("CONNECTED", "Status query failed");
            }
            _lastStatusMs = now;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

bool RaceSyncGoPro::discoverAndConnect()
{
    portENTER_CRITICAL(&_statusMux);
    _status.scanning = true;
    _status.connectionAttempts++;
    portEXIT_CRITICAL(&_statusMux);
    setState("SEARCHING");

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(80);
    scan->setWindow(40);
    NimBLEScanResults results = scan->start(4, false);

    portENTER_CRITICAL(&_statusMux);
    _status.scanning = false;
    portEXIT_CRITICAL(&_statusMux);

    if (_racePriorityMode)
    {
        scan->clearResults();
        return false;
    }

    NimBLEAdvertisedDevice* camera = nullptr;
    for (int i = 0; i < results.getCount(); ++i)
    {
        NimBLEAdvertisedDevice* candidate = results.getDevice(i);
        if (candidate != nullptr && isGoPro(*candidate))
        {
            camera = candidate;
            break;
        }
    }

    if (camera == nullptr)
    {
        setState("NOT_FOUND", "No advertising GoPro found");
        scan->clearResults();
        return false;
    }

    portENTER_CRITICAL(&_statusMux);
    _status.discovered = true;
    _status.rssi = camera->getRSSI();
    snprintf(_status.name, sizeof(_status.name), "%s", camera->getName().c_str());
    snprintf(_status.address, sizeof(_status.address), "%s", camera->getAddress().toString().c_str());
    portEXIT_CRITICAL(&_statusMux);

    const bool connected = configureConnection(camera);
    scan->clearResults();
    return connected;
}

bool RaceSyncGoPro::configureConnection(NimBLEAdvertisedDevice* device)
{
    setState("CONNECTING");
    _client = NimBLEDevice::createClient();
    if (_client == nullptr)
    {
        setState("ERROR", "Unable to create BLE client");
        return false;
    }

    _client->setClientCallbacks(this, false);
    _client->setConnectionParams(24, 48, 0, 60);
    _client->setConnectTimeout(8);
    if (!_client->connect(device))
    {
        setState("NOT_CONNECTED", "BLE connection failed");
        disconnect();
        return false;
    }

    NimBLERemoteService* service = _client->getService(CONTROL_SERVICE_UUID);
    if (service == nullptr)
    {
        setState("ERROR", "Open GoPro service unavailable");
        disconnect();
        return false;
    }

    _queryRequest = service->getCharacteristic(QUERY_REQUEST_UUID);
    NimBLERemoteCharacteristic* response = service->getCharacteristic(QUERY_RESPONSE_UUID);
    if (_queryRequest == nullptr || response == nullptr || !response->canNotify())
    {
        setState("ERROR", "Open GoPro query service incomplete");
        disconnect();
        return false;
    }
    if (!response->subscribe(true, notificationCallback))
    {
        setState("ERROR", "Unable to subscribe to GoPro status");
        disconnect();
        return false;
    }

    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    portEXIT_CRITICAL(&_statusMux);
    setState("CONNECTED");
    Serial.print("[GOPRO] Connected to ");
    Serial.println(device->getName().c_str());
    return true;
}

void RaceSyncGoPro::disconnect()
{
    _queryRequest = nullptr;
    if (_client != nullptr)
    {
        if (_client->isConnected()) _client->disconnect();
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    portEXIT_CRITICAL(&_statusMux);
    resetResponse();
}

bool RaceSyncGoPro::requestStatus()
{
    if (_queryRequest == nullptr || _client == nullptr || !_client->isConnected()) return false;
    // Open GoPro Query Status Value (0x13): overheating, busy, encoding,
    // remaining video time, battery percentage, ready and SD errors.
    const uint8_t request[] = {0x08, QUERY_STATUS_COMMAND, 6, 8, 10, 35, 70, 82, 112};
    resetResponse();
    return _queryRequest->writeValue(request, sizeof(request), true);
}

void RaceSyncGoPro::notificationCallback(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool)
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
    updated.lastStatusAgeMs = millis(); // Timestamp internally; status() converts it to age.
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

void RaceSyncGoPro::onConnect(NimBLEClient*)
{
    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::onDisconnect(NimBLEClient*)
{
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    portEXIT_CRITICAL(&_statusMux);
}
