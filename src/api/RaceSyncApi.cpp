#include "RaceSyncApi.h"

#include <ArduinoJson.h>
#include "../../include/Pins.h"
#include "../config/RaceSyncConfig.h"
#include "esp_system.h"

RaceSyncApi::RaceSyncApi(
    RaceSyncStorage& storage,
    RaceSyncLogger& logger,
    RaceSyncGps& gps,
    RaceSyncWifi& wifi,
    Telemetry& telemetry,
    DataMode& mode,
    uint32_t& bootCount)
    : _storage(storage), _logger(logger), _gps(gps), _wifi(wifi),
      _telemetry(telemetry), _mode(mode), _bootCount(bootCount)
{
}

void RaceSyncApi::addCorsHeaders()
{
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
    _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void RaceSyncApi::sendJson(int status, const String& body)
{
    addCorsHeaders();
    _server.send(status, "application/json", body);
}

String RaceSyncApi::formatUptime()
{
    uint32_t seconds = millis() / 1000;
    uint32_t days = seconds / 86400; seconds %= 86400;
    uint32_t hours = seconds / 3600; seconds %= 3600;
    uint32_t minutes = seconds / 60; seconds %= 60;
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%ud %02u:%02u:%02u", days, hours, minutes, seconds);
    return String(buffer);
}

String RaceSyncApi::formatVBoxTime(double rawTime)
{
    int hour = (int)(rawTime / 10000.0);
    int minute = ((int)rawTime % 10000) / 100;
    double seconds = rawTime - hour * 10000.0 - minute * 100.0;
    int whole = (int)seconds;
    int milliseconds = (int)((seconds - whole) * 1000.0 + 0.5);
    if (milliseconds >= 1000) { milliseconds = 0; whole++; }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", hour, minute, whole, milliseconds);
    return String(buffer);
}

const char* RaceSyncApi::resetReasonName()
{
    switch (esp_reset_reason())
    {
        case ESP_RST_POWERON: return "POWER_ON";
        case ESP_RST_EXT: return "EXTERNAL_RESET";
        case ESP_RST_SW: return "SOFTWARE_RESET";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INTERRUPT_WATCHDOG";
        case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
        case ESP_RST_WDT: return "WATCHDOG";
        case ESP_RST_DEEPSLEEP: return "DEEP_SLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

void RaceSyncApi::handleStatus()
{
    JsonDocument doc;

    JsonObject system = doc["system"].to<JsonObject>();
    system["product"] = RaceSyncConfig::PRODUCT;
    system["firmware"] = RaceSyncConfig::FIRMWARE;
    system["mode"] = dataModeName(_mode);
    system["uptimeSeconds"] = millis() / 1000;
    system["uptime"] = formatUptime();
    system["bootCount"] = _bootCount;
    system["resetReason"] = resetReasonName();

    JsonObject board = doc["board"].to<JsonObject>();
    board["model"] = "ESP32-S3 DevKitC-1";
    board["chipModel"] = ESP.getChipModel();
    board["chipRevision"] = ESP.getChipRevision();
    board["cores"] = ESP.getChipCores();
    board["cpuMHz"] = ESP.getCpuFreqMHz();
    board["sdkVersion"] = ESP.getSdkVersion();
    board["flashSizeBytes"] = ESP.getFlashChipSize();
    board["flashSizeMB"] = ESP.getFlashChipSize() / (1024 * 1024);
    board["heapSizeBytes"] = ESP.getHeapSize();
    board["freeHeapBytes"] = ESP.getFreeHeap();
    board["minFreeHeapBytes"] = ESP.getMinFreeHeap();
    board["maxAllocHeapBytes"] = ESP.getMaxAllocHeap();
    board["psramSizeBytes"] = ESP.getPsramSize();
    board["freePsramBytes"] = ESP.getFreePsram();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["status"] = "OK";
    wifi["mode"] = "ACCESS_POINT";
    wifi["ssid"] = RaceSyncConfig::WIFI_SSID;
    wifi["password"] = RaceSyncConfig::WIFI_PASSWORD;
    wifi["ip"] = _wifi.ip();
    wifi["connectedClients"] = _wifi.connectedClients();
    wifi["uptimeSeconds"] = _wifi.uptimeSeconds();

    JsonObject gps = doc["gps"].to<JsonObject>();
    gps["connected"] = _gps.connected();
    gps["source"] = "MG-902";
    gps["validFix"] = _telemetry.valid;
    gps["fixType"] = _telemetry.solutionType;
    gps["satellites"] = _telemetry.satellites;
    gps["latitude"] = _telemetry.latitude;
    gps["longitude"] = _telemetry.longitude;
    gps["speedKmh"] = _telemetry.velocityKmh;
    gps["heading"] = _telemetry.heading;
    gps["heightM"] = _telemetry.height;
    gps["sampleRateHz"] = _telemetry.samplePeriod > 0 ? 1.0 / _telemetry.samplePeriod : 0.0;
    uint32_t packetAge = _gps.lastPacketAgeMs();
    gps["lastPacketAgeMs"] = packetAge == UINT32_MAX ? -1 : (int64_t)packetAge;
    gps["bytesReceived"] = _gps.bytesReceived();
    gps["packetCount"] = _gps.packetCount();
    gps["checksumErrors"] = _gps.checksumErrors();

    uint64_t totalStorage = _storage.totalBytes();
    uint64_t usedStorage = _storage.usedBytes();
    uint64_t freeStorage = _storage.freeBytes();
    double usedPercent = totalStorage > 0 ? (usedStorage * 100.0) / totalStorage : 0.0;

    JsonObject storage = doc["storage"].to<JsonObject>();
    storage["type"] = _storage.storageType();
    storage["filesystem"] = _storage.filesystemName();
    storage["ready"] = _storage.ready();
    storage["readable"] = _storage.readable();
    storage["writable"] = _storage.writable();
    storage["lastError"] = _storage.lastError();

    JsonObject selfTest = storage["selfTest"].to<JsonObject>();
    selfTest["filename"] = _storage.selfTestFilename();
    selfTest["openOk"] = _storage.selfTestOpenOk();
    selfTest["bytesWritten"] = _storage.selfTestBytesWritten();
    selfTest["existsAfterClose"] = _storage.selfTestExistsAfterClose();
    selfTest["readBackOpenOk"] = _storage.selfTestReadBackOpenOk();
    selfTest["readBackBytes"] = _storage.selfTestReadBackBytes();
    selfTest["deleteOk"] = _storage.selfTestDeleteOk();

    JsonObject recovery = storage["recovery"].to<JsonObject>();
    recovery["recoveredPartFiles"] = _storage.recoveredPartFiles();
    recovery["failedPartRecoveries"] = _storage.failedPartRecoveries();
    recovery["lastRecoveredFilename"] = _storage.lastRecoveredFilename();

    storage["totalBytes"] = totalStorage;
    storage["usedBytes"] = usedStorage;
    storage["freeBytes"] = freeStorage;
    storage["usedPercent"] = usedPercent;
    storage["sessionCount"] = _storage.sessionCount();
    storage["writeErrors"] = _logger.storageWriteErrors();
    storage["minimumFreeReserveBytes"] = RaceSyncConfig::MIN_FREE_STORAGE_BYTES;

    JsonObject logger = doc["logger"].to<JsonObject>();
    logger["state"] = _logger.recording() ? "RECORDING" : "IDLE";
    logger["recording"] = _logger.recording();
    logger["currentFile"] = _logger.currentFilename();
    logger["samplesWritten"] = _logger.sampleCount();
    logger["startSpeedKmh"] = _logger.startSpeedKmh();
    logger["stopSpeedKmh"] = _logger.stopSpeedKmh();
    logger["stopDelaySeconds"] = _logger.stopDelaySeconds();
    logger["recordingSeconds"] = _logger.recordingSeconds();
    uint32_t writeAge = _logger.lastWriteAgeMs();
    logger["lastWriteAgeMs"] = writeAge == UINT32_MAX ? -1 : (int64_t)writeAge;

    JsonObject power = doc["power"].to<JsonObject>();
    power["source"] = "EXTERNAL";
    power["voltageMonitoring"] = false;
    power["batteryPercentageAvailable"] = false;

    JsonObject health = doc["health"].to<JsonObject>();
    health["system"] = "OK";
    if (!_gps.connected()) health["gps"] = "NO_DEVICE";
    else if (!_telemetry.valid) health["gps"] = "NO_FIX";
    else health["gps"] = "OK";

    if (!_storage.ready() || !_storage.readable() || !_storage.writable()) health["storage"] = "ERROR";
    else if (usedPercent >= RaceSyncConfig::STORAGE_FULL_PERCENT) health["storage"] = "FULL";
    else if (usedPercent >= RaceSyncConfig::STORAGE_WARNING_PERCENT) health["storage"] = "WARNING";
    else health["storage"] = "OK";

    health["logger"] = _logger.recording() ? "RECORDING" : "IDLE";
    health["wifi"] = "OK";
    bool systemHealthy = _storage.ready() && _storage.readable() && _storage.writable() && usedPercent < RaceSyncConfig::STORAGE_WARNING_PERCENT && ESP.getFreeHeap() > RaceSyncConfig::MIN_HEALTHY_HEAP_BYTES;
    health["overall"] = systemHealthy ? "OK" : "WARNING";

    String response; serializeJson(doc, response); sendJson(200, response);
}

void RaceSyncApi::handleLocation()
{
    JsonDocument doc;
    doc["valid"] = _telemetry.valid;
    doc["latitude"] = _telemetry.latitude;
    doc["longitude"] = _telemetry.longitude;
    doc["speedKmh"] = _telemetry.velocityKmh;
    doc["heading"] = _telemetry.heading;
    doc["height"] = _telemetry.height;
    doc["satellites"] = _telemetry.satellites;
    String response; serializeJson(doc, response); sendJson(200, response);
}

void RaceSyncApi::handleTelemetry()
{
    JsonDocument doc;
    doc["valid"] = _telemetry.valid;
    doc["mode"] = dataModeName(_mode);
    doc["sampleIndex"] = _telemetry.sampleIndex;
    doc["samplePeriod"] = _telemetry.samplePeriod;
    doc["time"] = formatVBoxTime(_telemetry.rawTime);

    JsonObject gps = doc["gps"].to<JsonObject>();
    gps["satellites"] = _telemetry.satellites;
    gps["latitude"] = _telemetry.latitude;
    gps["longitude"] = _telemetry.longitude;
    gps["speedKmh"] = _telemetry.velocityKmh;
    gps["heading"] = _telemetry.heading;

    JsonObject channels = doc["channels"].to<JsonObject>();
    channels["ComboAcc"] = _telemetry.comboAcc;
    channels["OilPressure"] = _telemetry.oilPressure;
    channels["OilTemperature"] = _telemetry.oilTemperature;
    channels["WaterTemperature"] = _telemetry.waterTemperature;
    channels["Revs"] = _telemetry.revs;
    channels["FuelPressure"] = _telemetry.fuelPressure;
    channels["ComboG"] = _telemetry.comboG;

    JsonObject rpm = doc["rpm"].to<JsonObject>();
    rpm["value"] = _telemetry.revs;
    rpm["ledEnabled"] = _telemetry.rpmLedEnabled;
    rpm["signalPresent"] = _telemetry.rpmSignalPresent;
    rpm["pulseCount"] = _telemetry.rpmPulseCount;
    rpm["rejectedReadingCount"] = _telemetry.rpmRejectedReadingCount;
    rpm["lastPulseAgeMs"] = _telemetry.rpmLastPulseAgeMs == UINT32_MAX
        ? -1
        : static_cast<int64_t>(_telemetry.rpmLastPulseAgeMs);
    rpm["inputPin"] = Pin::RPM_INPUT;
    rpm["inputLevel"] = _telemetry.rpmInputLevel;

    String response; serializeJson(doc, response); sendJson(200, response);
}

void RaceSyncApi::handleSessions()
{
    JsonDocument doc;
    doc["device"] = RaceSyncConfig::PRODUCT;
    JsonArray sessions = doc["sessions"].to<JsonArray>();
    String activeFilename = _logger.recording() ? _logger.currentFilename() : "";
    _storage.addSessionsToJson(sessions, activeFilename);
    doc["count"] = sessions.size();
    String response; serializeJson(doc, response); sendJson(200, response);
}

bool RaceSyncApi::parseSessionIdFromUri(uint32_t& sessionId) const
{
    sessionId = 0;
    const String prefix = "/api/sessions/";
    String value = _server.uri().substring(prefix.length());
    if (value.length() == 0) return false;
    for (size_t i = 0; i < value.length(); i++) if (value[i] < '0' || value[i] > '9') return false;
    unsigned long parsed = strtoul(value.c_str(), nullptr, 10);
    if (parsed == 0) return false;
    sessionId = (uint32_t)parsed;
    return true;
}

void RaceSyncApi::handleSessionDownloadById(uint32_t sessionId)
{
    String filename;
    if (!_storage.findSessionById(sessionId, filename)) { sendJson(404, "{\"error\":\"Session not found\"}"); return; }
    File file = _storage.openRead(filename);
    if (!file) { sendJson(404, "{\"error\":\"Session file not found\"}"); return; }
    addCorsHeaders();
    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    _server.sendHeader("X-RaceSync-Session-Id", String(sessionId));
    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(file, "application/octet-stream");
    file.close();
}

void RaceSyncApi::handleLegacySessionDownload(const String& filename)
{
    if (!_storage.isSafeVBoxFilename(filename)) { sendJson(400, "{\"error\":\"Invalid session reference\"}"); return; }
    File file = _storage.openRead(filename);
    if (!file) { sendJson(404, "{\"error\":\"Session not found\"}"); return; }
    addCorsHeaders();
    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    _server.sendHeader("X-RaceSync-Session-Id", String(_storage.sessionIdForFilename(filename)));
    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(file, "application/octet-stream");
    file.close();
}

void RaceSyncApi::handleSessionDeleteById(uint32_t sessionId)
{
    String filename;
    if (!_storage.findSessionById(sessionId, filename)) {
        JsonDocument doc; doc["error"] = "Session not found"; if (_storage.lastError().length()) doc["storageError"] = _storage.lastError(); String response; serializeJson(doc, response); sendJson(404, response); return;
    }
    if (_logger.recording() && filename == _logger.currentFilename()) { sendJson(409, "{\"error\":\"Cannot delete the active recording\"}"); return; }

    String deletedFilename;
    if (!_storage.deleteSessionById(sessionId, deletedFilename)) {
        JsonDocument doc; doc["error"] = "Unable to delete session"; doc["storageError"] = _storage.lastError(); String response; serializeJson(doc, response); sendJson(500, response); return;
    }

    JsonDocument doc;
    doc["deleted"] = true;
    doc["id"] = sessionId;
    doc["file"] = deletedFilename;
    doc["freeBytes"] = _storage.freeBytes();
    doc["usedBytes"] = _storage.usedBytes();
    doc["usedPercent"] = _storage.totalBytes() > 0 ? (_storage.usedBytes() * 100.0) / _storage.totalBytes() : 0.0;
    String response; serializeJson(doc, response); sendJson(200, response);
}

void RaceSyncApi::begin()
{
    _server.enableCORS(true);
    _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server.on("/api/location", HTTP_GET, [this]() { handleLocation(); });
    _server.on("/api/telemetry", HTTP_GET, [this]() { handleTelemetry(); });
    _server.on("/api/sessions", HTTP_GET, [this]() { handleSessions(); });

    _server.onNotFound([this]()
    {
        const String prefix = "/api/sessions/";
        if (_server.uri().startsWith(prefix))
        {
            uint32_t sessionId = 0;
            bool isId = parseSessionIdFromUri(sessionId);
            if (_server.method() == HTTP_GET && isId) { handleSessionDownloadById(sessionId); return; }
            if (_server.method() == HTTP_DELETE)
            {
                if (!isId) { sendJson(400, "{\"error\":\"DELETE requires a numeric session id\"}"); return; }
                handleSessionDeleteById(sessionId); return;
            }
            if (_server.method() == HTTP_GET && !isId)
            {
                handleLegacySessionDownload(_server.uri().substring(prefix.length())); return;
            }
        }
        if (_server.method() == HTTP_OPTIONS) { addCorsHeaders(); _server.send(204); return; }
        sendJson(404, "{\"error\":\"Not found\"}");
    });

    _server.begin();
    Serial.println("[HTTP] REST API ready");
}

void RaceSyncApi::update()
{
    _server.handleClient();
}
