#include "RaceSyncGoPro.h"

#include <cstring>
#include <esp_arduino_version.h>
#include <BLESecurity.h>

RaceSyncGoPro* RaceSyncGoPro::_instance = nullptr;

namespace
{
constexpr uint32_t AUTO_CONNECT_DELAY_MS = 15000;
constexpr uint32_t AUTO_CONNECT_RETRY_MS = 60000;
constexpr uint32_t KEEP_ALIVE_INTERVAL_MS = 3000;
constexpr uint8_t KEEP_ALIVE_SETTING_ID = 0x5B;
constexpr uint8_t KEEP_ALIVE_VALUE = 0x42;

bool isGoPro(BLEAdvertisedDevice& device)
{
    static BLEUUID goProAdvertisingService("0000fea6-0000-1000-8000-00805f9b34fb");
    if (device.haveServiceUUID() && device.isAdvertisingService(goProAdvertisingService)) return true;

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

bool isLeapYear(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t daysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && isLeapYear(year)) return 29;
    return (month >= 1 && month <= 12) ? days[month - 1] : 31;
}

uint8_t dayOfWeek(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t offsets[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (month < 3) --year;
    return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

bool isBritishSummerTime(const Telemetry& telemetry)
{
    if (telemetry.month < 3 || telemetry.month > 10) return false;
    if (telemetry.month > 3 && telemetry.month < 10) return true;

    const uint8_t lastDay = daysInMonth(telemetry.year, telemetry.month);
    const uint8_t transitionDay = lastDay - dayOfWeek(telemetry.year, telemetry.month, lastDay);
    if (telemetry.month == 3)
    {
        if (telemetry.day != transitionDay) return telemetry.day > transitionDay;
        return telemetry.hour >= 1;
    }
    if (telemetry.day != transitionDay) return telemetry.day < transitionDay;
    return telemetry.hour < 1;
}

void gpsUtcToUkLocal(const Telemetry& telemetry, uint16_t& year, uint8_t& month,
                     uint8_t& day, uint8_t& hour)
{
    year = telemetry.year;
    month = telemetry.month;
    day = telemetry.day;
    hour = telemetry.hour;
    if (!isBritishSummerTime(telemetry)) return;

    if (++hour < 24) return;
    hour = 0;
    if (++day <= daysInMonth(year, month)) return;
    day = 1;
    if (++month <= 12) return;
    month = 1;
    ++year;
}

void configureGoProBleSecurity()
{
    static BLESecurity* security = nullptr;
    if (security != nullptr) return;

    security = new BLESecurity();
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
    security->setAuthenticationMode(true, false, false);
    security->setForceAuthentication(false);
#else
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);
    // Arduino-ESP32 2.x has no BLEClient::secureConnection(). Request
    // encryption automatically as part of the subsequent client connection.
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
#endif

    Serial.println("[GOPRO] BLE security configured: legacy bonded Just Works, deferred authentication");
}
}

void RaceSyncGoPro::beginAutoConnect()
{
    _preferencesReady = _preferences.begin("racesync-gopro", false);
    if (!_preferencesReady)
    {
        Serial.println("[GOPRO] Auto-connect settings unavailable");
        return;
    }

    _savedAddress = _preferences.getString("address", "");
    _savedAddressType = _preferences.getUChar("addressType", BLE_ADDR_TYPE_PUBLIC);
    const bool interruptedAttempt = _preferences.getBool("autoPending", false);
    if (interruptedAttempt)
    {
        // A reset during the previous automatic BLE attempt must not recreate
        // an unattended boot loop. Manual Connect clears this one-boot guard.
        _preferences.putBool("autoPending", false);
        _autoConnectSuppressed = true;
        Serial.println("[GOPRO] Automatic connection suppressed: previous attempt ended in a reset");
    }

    portENTER_CRITICAL(&_statusMux);
    _status.autoConnectConfigured = _savedAddress.length() > 0;
    _status.autoConnectSuppressed = _autoConnectSuppressed;
    snprintf(_status.savedAddress, sizeof(_status.savedAddress), "%s", _savedAddress.c_str());
    portEXIT_CRITICAL(&_statusMux);
    _nextAutoConnectMs = millis() + AUTO_CONNECT_DELAY_MS;

    Serial.printf("[GOPRO] Auto-connect %s; first attempt after %lu seconds while idle\n",
                  _savedAddress.length() ? "configured" : "waiting for first manual pairing",
                  static_cast<unsigned long>(AUTO_CONNECT_DELAY_MS / 1000));
}

void RaceSyncGoPro::updateKeepAlive(bool loggerRecording)
{
    if (loggerRecording || _settingsRequest == nullptr || _client == nullptr ||
        !_client->isConnected() || !status().connected) return;

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - _nextKeepAliveMs) < 0) return;
    _nextKeepAliveMs = now + KEEP_ALIVE_INTERVAL_MS;

    // Open GoPro Keep Alive is a Settings request containing setting 0x5B
    // with the fixed value 0x42. This runs only while the logger is idle, so
    // it cannot contend with session shutter commands or SD writes.
    uint8_t request[] = {0x03, KEEP_ALIVE_SETTING_ID, 0x01, KEEP_ALIVE_VALUE};
    _settingsRequest->writeValue(request, sizeof(request), true);

    portENTER_CRITICAL(&_statusMux);
    _status.keepAliveSent++;
    _status.lastKeepAliveAgeMs = now;
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::updateAutoConnect(const Telemetry& telemetry, bool loggerRecording)
{
    if (!_preferencesReady || _savedAddress.length() == 0 || _autoConnectSuppressed ||
        loggerRecording || telemetry.velocityKmh > 1.0 || _autoConnectTaskHandle != nullptr) return;
    if (status().connected) return;

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - _nextAutoConnectMs) < 0) return;
    _nextAutoConnectMs = now + AUTO_CONNECT_RETRY_MS;
    _autoConnectTelemetry = telemetry;

    if (!_autoCrashGuardComplete) _preferences.putBool("autoPending", true);
    portENTER_CRITICAL(&_statusMux);
    _status.autoConnectAttempting = true;
    _status.autoConnectAttempts++;
    portEXIT_CRITICAL(&_statusMux);

    const BaseType_t created = xTaskCreatePinnedToCore(
        autoConnectTaskEntry, "gopro-auto-connect", 6144, this, 1,
        &_autoConnectTaskHandle, 0);
    if (created == pdPASS)
    {
        Serial.printf("[GOPRO] Automatic direct connection queued for %s\n", _savedAddress.c_str());
        return;
    }

    if (!_autoCrashGuardComplete) _preferences.putBool("autoPending", false);
    _autoConnectTaskHandle = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.autoConnectAttempting = false;
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] Unable to create automatic connection task");
}

void RaceSyncGoPro::autoConnectTaskEntry(void* argument)
{
    RaceSyncGoPro* camera = static_cast<RaceSyncGoPro*>(argument);
    const bool connected = camera->connectSavedCamera(camera->_autoConnectTelemetry);

    if (!camera->_autoCrashGuardComplete && camera->_preferencesReady)
    {
        camera->_preferences.putBool("autoPending", false);
        camera->_autoCrashGuardComplete = true;
    }
    portENTER_CRITICAL(&camera->_statusMux);
    camera->_status.autoConnectAttempting = false;
    if (connected) camera->_status.autoConnectSuccesses++;
    portEXIT_CRITICAL(&camera->_statusMux);

    Serial.printf("[GOPRO] Automatic direct connection %s\n", connected ? "succeeded" : "failed");
    camera->_autoConnectTaskHandle = nullptr;
    vTaskDelete(nullptr);
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

bool RaceSyncGoPro::connect(const Telemetry& telemetry)
{
    Serial.println("[GOPRO] Connect requested from Web API");
    if (_autoConnectTaskHandle != nullptr)
    {
        setState("AUTO_CONNECTING", "Automatic GoPro connection is already in progress");
        return false;
    }
    if (!initialiseBluetooth()) return false;
    if (_client != nullptr && _client->isConnected())
    {
        const GoProStatus current = status();
        if (current.connected && current.statusValid)
        {
            setState("CONNECTED");
            Serial.println("[GOPRO] Existing authenticated GoPro connection is already active");
            setDateTimeFromGps(telemetry);
            return true;
        }

        Serial.println("[GOPRO] Existing BLE link is not fully configured; reconnecting");
        _client->disconnect();
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    return discoverAndConnect(telemetry);
}

void RaceSyncGoPro::disconnect(bool suppressAutoConnect)
{
    _queryRequest = nullptr;
    _commandRequest = nullptr;
    _settingsRequest = nullptr;
    if (_client != nullptr && _client->isConnected()) _client->disconnect();

    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.scanning = false;
    _status.statusValid = false;
    _status.videoStartPending = false;
    _status.videoStopPending = false;
    if (suppressAutoConnect)
    {
        _autoConnectSuppressed = true;
        _status.autoConnectSuppressed = true;
    }
    portEXIT_CRITICAL(&_statusMux);
    resetResponse();
    setState(status().enabled ? "DISCONNECTED" : "DISABLED");
    Serial.printf("[GOPRO] Disconnect complete; auto-connect %s for this boot\n",
                  suppressAutoConnect ? "paused" : "unchanged");
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
        setState("CONNECTED", "GoPro must be connected before video can start");
        return false;
    }
    if (_videoStartTaskHandle != nullptr || _videoStopTaskHandle != nullptr) return false;

    const GoProStatus current = status();
    if (current.statusValid && current.recording)
    {
        Serial.println("[GOPRO] Video already recording; no shutter-on command required");
        return true;
    }

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

bool RaceSyncGoPro::queueVideoStop()
{
    if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.videoStopErrors++;
        portEXIT_CRITICAL(&_statusMux);
        Serial.println("[GOPRO] Video stop not queued: camera is not connected");
        return false;
    }
    if (_videoStartTaskHandle != nullptr || _videoStopTaskHandle != nullptr) return false;

    const GoProStatus current = status();
    if (current.statusValid && !current.recording)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.videoStopConfirmed = true;
        portEXIT_CRITICAL(&_statusMux);
        Serial.println("[GOPRO] Video already stopped; no shutter-off command required");
        return true;
    }

    portENTER_CRITICAL(&_statusMux);
    _status.videoStopPending = true;
    _status.videoStopSent = false;
    _status.videoStopConfirmed = false;
    _status.videoStopRequests++;
    portEXIT_CRITICAL(&_statusMux);

    const BaseType_t created = xTaskCreatePinnedToCore(
        videoStopTaskEntry, "gopro-video-stop", 4096, this, 1,
        &_videoStopTaskHandle, 0);
    if (created == pdPASS) return true;

    _videoStopTaskHandle = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.videoStopPending = false;
    _status.videoStopErrors++;
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] Unable to queue GoPro video stop");
    return false;
}

void RaceSyncGoPro::videoStartTaskEntry(void* argument)
{
    static_cast<RaceSyncGoPro*>(argument)->sendVideoStart();
}

void RaceSyncGoPro::videoStopTaskEntry(void* argument)
{
    static_cast<RaceSyncGoPro*>(argument)->sendVideoStop();
}

void RaceSyncGoPro::sendVideoStart()
{
    if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.videoStartPending = false;
        _status.videoStartErrors++;
        portEXIT_CRITICAL(&_statusMux);
        _videoStartTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

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

void RaceSyncGoPro::sendVideoStop()
{
    if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.videoStopPending = false;
        _status.videoStopErrors++;
        portEXIT_CRITICAL(&_statusMux);
        _videoStopTaskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    uint8_t request[] = {0x03, 0x01, 0x01, 0x00};
    _commandRequest->writeValue(request, sizeof(request), true);

    portENTER_CRITICAL(&_statusMux);
    _status.videoStopSent = true;
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] Video stop command write requested");

    vTaskDelay(pdMS_TO_TICKS(1500));
    portENTER_CRITICAL(&_statusMux);
    if (_status.videoStopPending)
    {
        _status.videoStopPending = false;
        _status.videoStopErrors++;
        snprintf(_status.lastError, sizeof(_status.lastError), "%s", "No GoPro video-stop confirmation");
    }
    portEXIT_CRITICAL(&_statusMux);

    _videoStopTaskHandle = nullptr;
    vTaskDelete(nullptr);
}

GoProStatus RaceSyncGoPro::status() const
{
    portENTER_CRITICAL(&_statusMux);
    GoProStatus copy = _status;
    portEXIT_CRITICAL(&_statusMux);
    if (copy.statusValid && copy.lastStatusAgeMs != UINT32_MAX)
        copy.lastStatusAgeMs = millis() - copy.lastStatusAgeMs;
    if (copy.lastKeepAliveAgeMs != UINT32_MAX)
        copy.lastKeepAliveAgeMs = millis() - copy.lastKeepAliveAgeMs;
    return copy;
}

bool RaceSyncGoPro::discoverAndConnect(const Telemetry& telemetry)
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

        const bool connected = configureConnection(&candidate, telemetry);
        scan->clearResults();
        return connected;
    }

    scan->clearResults();
    setState("NOT_FOUND", "No GoPro advertising; put camera in pairing mode");
    Serial.println("[GOPRO] No GoPro advertisement found. On HERO9 open Connections > Connect Device > GoPro Quik App, then retry.");
    return false;
}

bool RaceSyncGoPro::configureConnection(BLEAdvertisedDevice* device, const Telemetry& telemetry)
{
    setState("CONNECTING");
    if (_client == nullptr) _client = BLEDevice::createClient();
    if (_client == nullptr)
    {
        setState("ERROR", "Unable to create BLE client");
        return false;
    }

    _client->setClientCallbacks(this);
    Serial.println("[GOPRO] Opening BLE link without forcing security");
    if (!_client->connect(device))
    {
        disconnect();
        setState("NOT_CONNECTED", "BLE connection failed");
        Serial.println("[GOPRO] BLE link connection failed");
        return false;
    }

    const String name = device->haveName() ? String(device->getName().c_str()) : String("GoPro");
    const String address(device->getAddress().toString().c_str());
    const bool configured = configureConnectedClient(name.c_str(), address.c_str(), telemetry);
    if (configured)
        rememberCamera(name.c_str(), address.c_str(), static_cast<uint8_t>(device->getAddressType()));
    return configured;
}

bool RaceSyncGoPro::connectSavedCamera(const Telemetry& telemetry)
{
    if (_savedAddress.length() == 0 || !initialiseBluetooth()) return false;
    if (_client != nullptr && _client->isConnected()) return true;

    setState("AUTO_CONNECTING");
    if (_client == nullptr) _client = BLEDevice::createClient();
    if (_client == nullptr) return false;
    _client->setClientCallbacks(this);

    BLEAddress address(_savedAddress.c_str());
    Serial.printf("[GOPRO] Opening saved BLE address %s without scanning\n", _savedAddress.c_str());
    if (!_client->connect(address, static_cast<esp_ble_addr_type_t>(_savedAddressType)))
    {
        _queryRequest = nullptr;
        _commandRequest = nullptr;
        _settingsRequest = nullptr;
        setState("AUTO_RETRY_WAIT", "Saved GoPro did not answer; retrying while idle");
        return false;
    }
    return configureConnectedClient("Saved GoPro", _savedAddress.c_str(), telemetry);
}

bool RaceSyncGoPro::configureConnectedClient(const char* name, const char* address,
                                             const Telemetry& telemetry)
{

    Serial.println("[GOPRO] BLE link connected; discovering Open GoPro service before pairing");
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
    _settingsRequest = service->getCharacteristic(BLEUUID(SETTINGS_REQUEST_UUID));
    BLERemoteCharacteristic* response = service->getCharacteristic(BLEUUID(QUERY_RESPONSE_UUID));
    BLERemoteCharacteristic* commandResponse = service->getCharacteristic(BLEUUID(COMMAND_RESPONSE_UUID));
    if (_queryRequest == nullptr || _commandRequest == nullptr || _settingsRequest == nullptr ||
        response == nullptr || commandResponse == nullptr || !response->canNotify() ||
        !commandResponse->canNotify())
    {
        disconnect();
        setState("ERROR", "Open GoPro query service incomplete");
        Serial.println("[GOPRO] Required Open GoPro command/query characteristics are incomplete");
        return false;
    }

    setState("PAIRING");
    Serial.println("[GOPRO] FEA6 discovered; initiating legacy bonded BLE security");
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    if (!_client->secureConnection())
    {
        Serial.println("[GOPRO] Legacy BLE pairing/bonding failed");
        disconnect();
        setState("PAIRING_REQUIRED", "GoPro rejected BLE pairing; put camera in pairing mode and retry");
        return false;
    }
#else
    // Encryption was requested by BLEDevice before the connection was opened.
    // Allow the asynchronous Arduino-ESP32 2.x GAP authentication to settle.
    vTaskDelay(pdMS_TO_TICKS(250));
#endif
    Serial.println("[GOPRO] BLE pairing/encryption complete");

    Serial.println("[GOPRO] Subscribing to Open GoPro response notifications");
    response->registerForNotify(notificationCallback, true);
    commandResponse->registerForNotify(commandNotificationCallback, true);

    portENTER_CRITICAL(&_statusMux);
    _status.connected = true;
    snprintf(_status.name, sizeof(_status.name), "%s", name);
    snprintf(_status.address, sizeof(_status.address), "%s", address);
    portEXIT_CRITICAL(&_statusMux);
    setState("CONNECTED");
    Serial.printf("[GOPRO] Open GoPro notification registration requested: %s\n", name);

    // Time synchronisation is optional. Failure must not turn a successful
    // camera connection into a failure or interfere with RaceSync logging.
    setDateTimeFromGps(telemetry);

    if (!requestStatus())
    {
        setState("ERROR", "Unable to issue initial GoPro status query");
        return false;
    }
    _nextKeepAliveMs = millis() + KEEP_ALIVE_INTERVAL_MS;
    return true;
}

void RaceSyncGoPro::rememberCamera(const char* name, const char* address, uint8_t addressType)
{
    _savedAddress = address;
    _savedAddressType = addressType;
    _autoConnectSuppressed = false;
    _autoCrashGuardComplete = true;
    if (_preferencesReady)
    {
        _preferences.putString("address", address);
        _preferences.putUChar("addressType", addressType);
        _preferences.putBool("autoPending", false);
    }

    portENTER_CRITICAL(&_statusMux);
    _status.autoConnectConfigured = true;
    _status.autoConnectSuppressed = false;
    snprintf(_status.savedAddress, sizeof(_status.savedAddress), "%s", address);
    snprintf(_status.name, sizeof(_status.name), "%s", name);
    portEXIT_CRITICAL(&_statusMux);
    Serial.printf("[GOPRO] Saved camera for future automatic connection: %s\n", address);
}

bool RaceSyncGoPro::setDateTimeFromGps(const Telemetry& telemetry)
{
    if (!telemetry.timeValid || telemetry.year < 2024 || telemetry.month < 1 ||
        telemetry.month > 12 || telemetry.day < 1 ||
        telemetry.day > daysInMonth(telemetry.year, telemetry.month) ||
        telemetry.hour > 23 || telemetry.minute > 59 || telemetry.second > 59)
    {
        portENTER_CRITICAL(&_statusMux);
        _status.timeSyncSent = false;
        _status.timeSyncConfirmed = false;
        snprintf(_status.timeSyncState, sizeof(_status.timeSyncState), "%s", "GPS_TIME_UNAVAILABLE");
        _status.syncedLocalTime[0] = '\0';
        portEXIT_CRITICAL(&_statusMux);
        Serial.println("[GOPRO] Date/time sync skipped: valid GPS UTC time is unavailable");
        return false;
    }
    if (_client == nullptr || !_client->isConnected() || _commandRequest == nullptr) return false;

    uint16_t year;
    uint8_t month, day, hour;
    gpsUtcToUkLocal(telemetry, year, month, day, hour);

    // Open GoPro Set Date Time: payload length, command, value length, then
    // big-endian year followed by month, day, hour, minute and second.
    uint8_t request[] = {
        0x09, SET_DATE_TIME_COMMAND, 0x07,
        static_cast<uint8_t>(year >> 8), static_cast<uint8_t>(year & 0xFF),
        month, day, hour, telemetry.minute, telemetry.second
    };

    portENTER_CRITICAL(&_statusMux);
    _status.timeSyncSent = true;
    _status.timeSyncConfirmed = false;
    snprintf(_status.timeSyncState, sizeof(_status.timeSyncState), "%s", "PENDING");
    snprintf(_status.syncedLocalTime, sizeof(_status.syncedLocalTime),
             "%04u-%02u-%02u %02u:%02u:%02u", year, month, day, hour,
             telemetry.minute, telemetry.second);
    portEXIT_CRITICAL(&_statusMux);

    _commandRequest->writeValue(request, sizeof(request), true);
    Serial.printf("[GOPRO] GPS time sync requested: %04u-%02u-%02u %02u:%02u:%02u UK local\n",
                  year, month, day, hour, telemetry.minute, telemetry.second);
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
    if (_instance == nullptr || data == nullptr || length < 3) return;

    if (data[1] == SET_DATE_TIME_COMMAND)
    {
        const bool success = data[2] == 0x00;
        portENTER_CRITICAL(&_instance->_statusMux);
        _instance->_status.timeSyncConfirmed = success;
        snprintf(_instance->_status.timeSyncState, sizeof(_instance->_status.timeSyncState),
                 "%s", success ? "SYNCED" : "REJECTED");
        if (!success) _instance->_status.timeSyncErrors++;
        portEXIT_CRITICAL(&_instance->_statusMux);
        Serial.printf("[GOPRO] GPS time sync %s (response code %u)\n",
                      success ? "confirmed" : "rejected", data[2]);
        return;
    }
    if (data[1] != 0x01) return;

    const bool success = data[2] == 0x00;
    portENTER_CRITICAL(&_instance->_statusMux);

    if (_instance->_status.videoStopPending)
    {
        _instance->_status.videoStopPending = false;
        _instance->_status.videoStopConfirmed = success;
        if (success)
        {
            _instance->_status.recording = false;
            _instance->_status.lastError[0] = '\0';
        }
        else
        {
            _instance->_status.videoStopErrors++;
            snprintf(_instance->_status.lastError, sizeof(_instance->_status.lastError),
                     "GoPro rejected video stop (code %u)", data[2]);
        }
    }
    else
    {
        _instance->_status.videoStartPending = false;
        _instance->_status.videoStartConfirmed = success;
        if (success)
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
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    portEXIT_CRITICAL(&_statusMux);
}

void RaceSyncGoPro::onDisconnect(BLEClient*)
{
    _queryRequest = nullptr;
    _commandRequest = nullptr;
    _settingsRequest = nullptr;
    portENTER_CRITICAL(&_statusMux);
    _status.connected = false;
    _status.statusValid = false;
    _status.videoStartPending = false;
    _status.videoStopPending = false;
    snprintf(_status.state, sizeof(_status.state), "%s", "DISCONNECTED");
    portEXIT_CRITICAL(&_statusMux);
    Serial.println("[GOPRO] BLE disconnected");
}
