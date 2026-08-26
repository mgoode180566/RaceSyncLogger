#include "RaceSyncApi.h"
#include <ArduinoJson.h>
#include "../config/RaceSyncConfig.h"
#include "esp_system.h"

RaceSyncApi::RaceSyncApi(
    RaceSyncStorage& storage,
    RaceSyncLogger& logger,
    RaceSyncGps& gps,
    RaceSyncWifi& wifi,
    RaceSyncSimulator& simulator,
    Telemetry& telemetry,
    DataMode& mode,
    uint32_t& bootCount
)
    : _storage(storage),
      _logger(logger),
      _gps(gps),
      _wifi(wifi),
      _simulator(simulator),
      _telemetry(telemetry),
      _mode(mode),
      _bootCount(bootCount)
{
}

void RaceSyncApi::addCorsHeaders()
{
    _server.sendHeader(
        "Access-Control-Allow-Origin",
        "*"
    );

    _server.sendHeader(
        "Access-Control-Allow-Methods",
        "GET,POST,OPTIONS"
    );

    _server.sendHeader(
        "Access-Control-Allow-Headers",
        "Content-Type"
    );
}

void RaceSyncApi::sendJson(
    int status,
    const String& body)
{
    addCorsHeaders();

    _server.send(
        status,
        "application/json",
        body
    );
}

String RaceSyncApi::formatUptime()
{
    uint32_t seconds =
        millis() / 1000;

    uint32_t days =
        seconds / 86400;

    seconds %= 86400;

    uint32_t hours =
        seconds / 3600;

    seconds %= 3600;

    uint32_t minutes =
        seconds / 60;

    seconds %= 60;

    char buffer[40];

    snprintf(
        buffer,
        sizeof(buffer),
        "%ud %02u:%02u:%02u",
        days,
        hours,
        minutes,
        seconds
    );

    return String(buffer);
}

String RaceSyncApi::formatVBoxTime(
    double rawTime)
{
    int hour =
        (int)(
            rawTime /
            10000.0
        );

    int minute =
        (
            (int)rawTime %
            10000
        ) /
        100;

    double seconds =
        rawTime -
        hour * 10000.0 -
        minute * 100.0;

    int whole =
        (int)seconds;

    int milliseconds =
        (int)(
            (
                seconds -
                whole
            ) *
            1000.0 +
            0.5
        );

    char buffer[32];

    snprintf(
        buffer,
        sizeof(buffer),
        "%02d:%02d:%02d.%03d",
        hour,
        minute,
        whole,
        milliseconds
    );

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

    JsonObject system =
        doc["system"]
            .to<JsonObject>();

    system["product"] =
        RaceSyncConfig::PRODUCT;

    system["firmware"] =
        RaceSyncConfig::FIRMWARE;

    system["mode"] =
        dataModeName(_mode);

    system["uptimeSeconds"] =
        millis() / 1000;

    system["uptime"] =
        formatUptime();

    system["bootCount"] =
        _bootCount;

    system["resetReason"] =
        resetReasonName();


    JsonObject health =
        doc["health"]
            .to<JsonObject>();


    JsonObject board =
        doc["board"]
            .to<JsonObject>();

    board["model"] =
        "ESP32-S3 DevKitC-1";

    board["chipModel"] =
        ESP.getChipModel();

    board["chipRevision"] =
        ESP.getChipRevision();

    board["cores"] =
        ESP.getChipCores();

    board["cpuMHz"] =
        ESP.getCpuFreqMHz();

    board["sdkVersion"] =
        ESP.getSdkVersion();

    board["flashSizeBytes"] =
        ESP.getFlashChipSize();

    board["flashSizeMB"] =
        ESP.getFlashChipSize() /
        (1024 * 1024);

    board["heapSizeBytes"] =
        ESP.getHeapSize();

    board["freeHeapBytes"] =
        ESP.getFreeHeap();

    board["minFreeHeapBytes"] =
        ESP.getMinFreeHeap();

    board["maxAllocHeapBytes"] =
        ESP.getMaxAllocHeap();

    board["psramSizeBytes"] =
        ESP.getPsramSize();

    board["freePsramBytes"] =
        ESP.getFreePsram();


    JsonObject wifi =
        doc["wifi"]
            .to<JsonObject>();

    wifi["status"] =
        "OK";

    wifi["mode"] =
        "ACCESS_POINT";

    wifi["ssid"] =
        RaceSyncConfig::WIFI_SSID;

    wifi["password"] =
        RaceSyncConfig::WIFI_PASSWORD;

    wifi["ip"] =
        _wifi.ip();

    wifi["connectedClients"] =
        _wifi.connectedClients();

    wifi["uptimeSeconds"] =
        _wifi.uptimeSeconds();


    JsonObject gps =
        doc["gps"]
            .to<JsonObject>();

    gps["connected"] =
        _gps.connected();

    gps["source"] =
        _mode == DataMode::LIVE
            ? "MG-902"
            : "DEMO";

    gps["validFix"] =
        _telemetry.valid;

    gps["fixType"] =
        _telemetry.solutionType;

    gps["satellites"] =
        _telemetry.satellites;

    gps["latitude"] =
        _telemetry.latitude;

    gps["longitude"] =
        _telemetry.longitude;

    gps["speedKmh"] =
        _telemetry.velocityKmh;

    gps["heading"] =
        _telemetry.heading;

    gps["heightM"] =
        _telemetry.height;

    gps["sampleRateHz"] =
        _telemetry.samplePeriod > 0
            ? 1.0 /
                _telemetry.samplePeriod
            : 0.0;

    uint32_t age =
        _gps.lastPacketAgeMs();

    if (age == UINT32_MAX)
        gps["lastPacketAgeMs"] = -1;
    else
        gps["lastPacketAgeMs"] = age;

    gps["bytesReceived"] =
        _gps.bytesReceived();

    gps["packetCount"] =
        _gps.packetCount();

    gps["checksumErrors"] =
        _gps.checksumErrors();

    gps["demoSource"] =
        _simulator.available()
            ? _simulator.sourceFilename()
            : "";


    JsonObject storage =
        doc["storage"]
            .to<JsonObject>();

    storage["type"] =
        _storage.storageType();

    storage["filesystem"] =
        _storage.filesystemName();

    storage["ready"] =
        _storage.ready();

    storage["totalBytes"] =
        _storage.totalBytes();

    storage["usedBytes"] =
        _storage.usedBytes();

    storage["freeBytes"] =
        _storage.freeBytes();

    storage["usedPercent"] =
        _storage.totalBytes() > 0
            ? (
                _storage.usedBytes() *
                100.0
            ) /
            _storage.totalBytes()
            : 0.0;

    storage["sessionCount"] =
        _storage.sessionCount();

    storage["writeErrors"] =
        _logger.storageWriteErrors();


    JsonObject logger =
        doc["logger"]
            .to<JsonObject>();

    logger["state"] =
        _logger.recording()
            ? "RECORDING"
            : "IDLE";

    logger["recording"] =
        _logger.recording();

    logger["currentFile"] =
        _logger.currentFilename();

    logger["samplesWritten"] =
        _logger.sampleCount();

    logger["startSpeedKmh"] =
        RaceSyncConfig::LOG_START_SPEED_KMH;

    logger["stopSpeedKmh"] =
        RaceSyncConfig::LOG_STOP_SPEED_KMH;

    logger["stopDelaySeconds"] =
        RaceSyncConfig::LOG_STOP_DELAY_MS /
        1000;

    logger["recordingSeconds"] =
        _logger.recordingSeconds();

    uint32_t writeAge =
        _logger.lastWriteAgeMs();

    if (writeAge == UINT32_MAX)
        logger["lastWriteAgeMs"] = -1;
    else
        logger["lastWriteAgeMs"] = writeAge;


    JsonObject power =
        doc["power"]
            .to<JsonObject>();

    power["source"] =
        "EXTERNAL";

    power["voltageMonitoring"] =
        false;

    power["batteryPercentageAvailable"] =
        false;


    health["system"] =
        "OK";

    if (_mode == DataMode::DEMO)
    {
        health["gps"] = "DEMO";
    }
    else if (!_gps.connected())
    {
        health["gps"] = "NO_DEVICE";
    }
    else if (!_telemetry.valid)
    {
        health["gps"] = "NO_FIX";
    }
    else
    {
        health["gps"] = "OK";
    }

    health["storage"] =
        _storage.ready()
            ? (
                _logger.storageWriteErrors() == 0
                    ? "OK"
                    : "WARNING"
            )
            : "ERROR";

    health["logger"] =
        _logger.recording()
            ? "RECORDING"
            : "IDLE";

    health["wifi"] =
        "OK";

    bool systemHealthy =
        _storage.ready() &&
        ESP.getFreeHeap() >
            RaceSyncConfig::MIN_HEALTHY_HEAP_BYTES;

    health["overall"] =
        systemHealthy
            ? "OK"
            : "WARNING";


    String response;
    serializeJson(doc, response);

    sendJson(
        200,
        response
    );
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

    String response;
    serializeJson(doc, response);

    sendJson(
        200,
        response
    );
}

void RaceSyncApi::handleTelemetry()
{
    JsonDocument doc;

    doc["valid"] =
        _telemetry.valid;

    doc["mode"] =
        dataModeName(_mode);

    doc["sampleIndex"] =
        _telemetry.sampleIndex;

    doc["samplePeriod"] =
        _telemetry.samplePeriod;

    doc["time"] =
        formatVBoxTime(
            _telemetry.rawTime
        );

    JsonObject gps =
        doc["gps"]
            .to<JsonObject>();

    gps["satellites"] =
        _telemetry.satellites;

    gps["latitude"] =
        _telemetry.latitude;

    gps["longitude"] =
        _telemetry.longitude;

    gps["speedKmh"] =
        _telemetry.velocityKmh;

    gps["heading"] =
        _telemetry.heading;

    JsonObject channels =
        doc["channels"]
            .to<JsonObject>();

    channels["ComboAcc"] =
        _telemetry.comboAcc;

    channels["OilPressure"] =
        _telemetry.oilPressure;

    channels["OilTemperature"] =
        _telemetry.oilTemperature;

    channels["WaterTemperature"] =
        _telemetry.waterTemperature;

    channels["Revs"] =
        _telemetry.revs;

    channels["FuelPressure"] =
        _telemetry.fuelPressure;

    channels["ComboG"] =
        _telemetry.comboG;

    String response;
    serializeJson(doc, response);

    sendJson(
        200,
        response
    );
}

void RaceSyncApi::handleSessions()
{
    JsonDocument doc;

    doc["device"] =
        RaceSyncConfig::PRODUCT;

    doc["count"] =
        _storage.sessionCount();

    JsonArray sessions =
        doc["sessions"]
            .to<JsonArray>();

    _storage.addSessionsToJson(
        sessions
    );

    String response;
    serializeJson(doc, response);

    sendJson(
        200,
        response
    );
}

void RaceSyncApi::handleSessionDownload()
{
    String filename =
        _server.pathArg(0);

    if (
        !_storage.isSafeVBoxFilename(
            filename
        )
    )
    {
        sendJson(
            400,
            "{\"error\":\"Invalid filename\"}"
        );
        return;
    }

    File file =
        _storage.openRead(
            filename
        );

    if (!file)
    {
        sendJson(
            404,
            "{\"error\":\"Session not found\"}"
        );
        return;
    }

    addCorsHeaders();

    _server.sendHeader(
        "Content-Disposition",
        "attachment; filename=\"" +
        filename +
        "\""
    );

    _server.sendHeader(
        "Cache-Control",
        "no-store"
    );

    _server.streamFile(
        file,
        "application/octet-stream"
    );

    file.close();
}

void RaceSyncApi::begin()
{
    _server.enableCORS(true);

    _server.on(
        "/api/status",
        HTTP_GET,
        [this]()
        {
            handleStatus();
        }
    );

    _server.on(
        "/api/location",
        HTTP_GET,
        [this]()
        {
            handleLocation();
        }
    );

    _server.on(
        "/api/telemetry",
        HTTP_GET,
        [this]()
        {
            handleTelemetry();
        }
    );

    _server.on(
        "/api/sessions",
        HTTP_GET,
        [this]()
        {
            handleSessions();
        }
    );

    _server.on(
        UriBraces(
            "/api/sessions/{}"
        ),
        HTTP_GET,
        [this]()
        {
            handleSessionDownload();
        }
    );

    _server.onNotFound(
        [this]()
        {
            if (
                _server.method() ==
                HTTP_OPTIONS
            )
            {
                addCorsHeaders();
                _server.send(204);
                return;
            }

            sendJson(
                404,
                "{\"error\":\"Not found\"}"
            );
        }
    );

    _server.begin();

    Serial.println("[HTTP] REST API ready");
}

void RaceSyncApi::update()
{
    _server.handleClient();
}
