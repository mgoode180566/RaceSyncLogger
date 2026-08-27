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
        "GET,DELETE,OPTIONS"
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

    seconds %=
        86400;

    uint32_t hours =
        seconds / 3600;

    seconds %=
        3600;

    uint32_t minutes =
        seconds / 60;

    seconds %=
        60;

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

    return
        String(buffer);
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

    if (
        milliseconds >=
        1000
    )
    {
        milliseconds =
            0;

        whole++;
    }

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

    return
        String(buffer);
}


const char* RaceSyncApi::resetReasonName()
{
    switch (
        esp_reset_reason()
    )
    {
        case ESP_RST_POWERON:
            return "POWER_ON";

        case ESP_RST_EXT:
            return "EXTERNAL_RESET";

        case ESP_RST_SW:
            return "SOFTWARE_RESET";

        case ESP_RST_PANIC:
            return "PANIC";

        case ESP_RST_INT_WDT:
            return "INTERRUPT_WATCHDOG";

        case ESP_RST_TASK_WDT:
            return "TASK_WATCHDOG";

        case ESP_RST_WDT:
            return "WATCHDOG";

        case ESP_RST_DEEPSLEEP:
            return "DEEP_SLEEP";

        case ESP_RST_BROWNOUT:
            return "BROWNOUT";

        case ESP_RST_SDIO:
            return "SDIO";

        default:
            return "UNKNOWN";
    }
}


void RaceSyncApi::handleStatus()
{
    JsonDocument doc;


    // --------------------------------------------------------
    // SYSTEM
    // --------------------------------------------------------

    JsonObject system =
        doc["system"]
            .to<JsonObject>();

    system["product"] =
        RaceSyncConfig::PRODUCT;

    system["firmware"] =
        RaceSyncConfig::FIRMWARE;

    system["mode"] =
        dataModeName(
            _mode
        );

    system["uptimeSeconds"] =
        millis() /
        1000;

    system["uptime"] =
        formatUptime();

    system["bootCount"] =
        _bootCount;

    system["resetReason"] =
        resetReasonName();


    // --------------------------------------------------------
    // BOARD
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // GPS / DATA SOURCE
    // --------------------------------------------------------

    JsonObject gps =
        doc["gps"]
            .to<JsonObject>();

    gps["connected"] =
        _gps.connected();

    gps["source"] =
        _mode ==
            DataMode::LIVE
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
        _telemetry.samplePeriod >
            0
            ? 1.0 /
                _telemetry.samplePeriod
            : 0.0;

    uint32_t packetAge =
        _gps.lastPacketAgeMs();

    if (
        packetAge ==
        UINT32_MAX
    )
    {
        gps["lastPacketAgeMs"] =
            -1;
    }
    else
    {
        gps["lastPacketAgeMs"] =
            packetAge;
    }

    gps["bytesReceived"] =
        _gps.bytesReceived();

    gps["packetCount"] =
        _gps.packetCount();

    gps["checksumErrors"] =
        _gps.checksumErrors();

    gps["demoSource"] =
        _simulator.sourceFilename();

    gps["demoAvailable"] =
        _simulator.available();

    gps["demoFinished"] =
        _simulator.finished();


    // --------------------------------------------------------
    // STORAGE
    // --------------------------------------------------------

    JsonObject storage =
        doc["storage"]
            .to<JsonObject>();

    uint64_t totalStorage =
        _storage.totalBytes();

    uint64_t usedStorage =
        _storage.usedBytes();

    uint64_t freeStorage =
        _storage.freeBytes();

    double usedPercent =
        totalStorage >
            0
            ? (
                usedStorage *
                100.0
            ) /
                totalStorage
            : 0.0;

    storage["type"] =
        _storage.storageType();

    storage["filesystem"] =
        _storage.filesystemName();

    storage["ready"] =
        _storage.ready();

    storage["totalBytes"] =
        totalStorage;

    storage["usedBytes"] =
        usedStorage;

    storage["freeBytes"] =
        freeStorage;

    storage["usedPercent"] =
        usedPercent;

    storage["sessionCount"] =
        _storage.sessionCount();

    storage["writeErrors"] =
        _logger.storageWriteErrors();

    storage["minimumFreeReserveBytes"] =
        RaceSyncConfig::MIN_FREE_STORAGE_BYTES;


    // --------------------------------------------------------
    // LOGGER
    // --------------------------------------------------------

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

    if (
        writeAge ==
        UINT32_MAX
    )
    {
        logger["lastWriteAgeMs"] =
            -1;
    }
    else
    {
        logger["lastWriteAgeMs"] =
            writeAge;
    }


    // --------------------------------------------------------
    // POWER
    // --------------------------------------------------------

    JsonObject power =
        doc["power"]
            .to<JsonObject>();

    power["source"] =
        "EXTERNAL";

    power["voltageMonitoring"] =
        false;

    power["batteryPercentageAvailable"] =
        false;


    // --------------------------------------------------------
    // HEALTH
    // --------------------------------------------------------

    JsonObject health =
        doc["health"]
            .to<JsonObject>();

    health["system"] =
        "OK";

    if (
        _mode ==
        DataMode::DEMO
    )
    {
        health["gps"] =
            _simulator.finished()
                ? "DEMO_FINISHED"
                : "DEMO";
    }
    else if (
        !_gps.connected()
    )
    {
        health["gps"] =
            "NO_DEVICE";
    }
    else if (
        !_telemetry.valid
    )
    {
        health["gps"] =
            "NO_FIX";
    }
    else
    {
        health["gps"] =
            "OK";
    }

    if (
        !_storage.ready()
    )
    {
        health["storage"] =
            "ERROR";
    }
    else if (
        usedPercent >=
        RaceSyncConfig::STORAGE_FULL_PERCENT
    )
    {
        health["storage"] =
            "FULL";
    }
    else if (
        usedPercent >=
        RaceSyncConfig::STORAGE_WARNING_PERCENT
    )
    {
        health["storage"] =
            "WARNING";
    }
    else
    {
        health["storage"] =
            "OK";
    }

    health["logger"] =
        _logger.recording()
            ? "RECORDING"
            : "IDLE";

    health["wifi"] =
        "OK";

    bool systemHealthy =
        _storage.ready() &&
        usedPercent <
            RaceSyncConfig::STORAGE_WARNING_PERCENT &&
        ESP.getFreeHeap() >
            RaceSyncConfig::MIN_HEALTHY_HEAP_BYTES;

    health["overall"] =
        systemHealthy
            ? "OK"
            : "WARNING";


    String response;

    serializeJson(
        doc,
        response
    );

    sendJson(
        200,
        response
    );
}


void RaceSyncApi::handleLocation()
{
    JsonDocument doc;

    doc["valid"] =
        _telemetry.valid;

    doc["latitude"] =
        _telemetry.latitude;

    doc["longitude"] =
        _telemetry.longitude;

    doc["speedKmh"] =
        _telemetry.velocityKmh;

    doc["heading"] =
        _telemetry.heading;

    doc["height"] =
        _telemetry.height;

    doc["satellites"] =
        _telemetry.satellites;

    String response;

    serializeJson(
        doc,
        response
    );

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
        dataModeName(
            _mode
        );

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

    serializeJson(
        doc,
        response
    );

    sendJson(
        200,
        response
    );
}


// ------------------------------------------------------------
// SESSION LIST
// ------------------------------------------------------------

void RaceSyncApi::handleSessions()
{
    JsonDocument doc;

    doc["device"] =
        RaceSyncConfig::PRODUCT;

    JsonArray sessions =
        doc["sessions"]
            .to<JsonArray>();

    String activeFilename =
        _logger.recording()
            ? _logger.currentFilename()
            : "";

    String demoFilename =
        _simulator.sourceFilename();

    _storage.addSessionsToJson(
        sessions,
        activeFilename,
        demoFilename
    );

    doc["count"] =
        sessions.size();

    String response;

    serializeJson(
        doc,
        response
    );

    sendJson(
        200,
        response
    );
}


// ------------------------------------------------------------
// SESSION URI PARSER
//
// /api/sessions/123456789
// ------------------------------------------------------------

bool RaceSyncApi::parseSessionIdFromUri(
    uint32_t& sessionId) const
{
    sessionId =
        0;

    const String prefix =
        "/api/sessions/";

    String value =
        _server.uri().substring(
            prefix.length()
        );

    if (
        value.length() == 0
    )
    {
        return false;
    }

    for (
        size_t i = 0;
        i < value.length();
        i++
    )
    {
        if (
            value[i] < '0' ||
            value[i] > '9'
        )
        {
            return false;
        }
    }

    unsigned long parsed =
        strtoul(
            value.c_str(),
            nullptr,
            10
        );

    if (
        parsed == 0
    )
    {
        return false;
    }

    sessionId =
        (uint32_t)parsed;

    return true;
}


// ------------------------------------------------------------
// DOWNLOAD BY SESSION ID
// ------------------------------------------------------------

void RaceSyncApi::handleSessionDownloadById(
    uint32_t sessionId)
{
    String filename;

    if (
        !_storage.findSessionById(
            sessionId,
            filename
        )
    )
    {
        sendJson(
            404,
            "{\"error\":\"Session not found\"}"
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
            "{\"error\":\"Session file not found\"}"
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
        "X-RaceSync-Session-Id",
        String(
            sessionId
        )
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


// ------------------------------------------------------------
// LEGACY FILENAME DOWNLOAD
//
// Existing clients using /api/sessions/file.vbo still work.
// New clients should use the numeric session ID.
// ------------------------------------------------------------

void RaceSyncApi::handleLegacySessionDownload(
    const String& filename)
{
    if (
        !_storage.isSafeVBoxFilename(
            filename
        )
    )
    {
        sendJson(
            400,
            "{\"error\":\"Invalid session reference\"}"
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
        "X-RaceSync-Session-Id",
        String(
            _storage.sessionIdForFilename(
                filename
            )
        )
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


// ------------------------------------------------------------
// DELETE BY SESSION ID
// ------------------------------------------------------------

void RaceSyncApi::handleSessionDeleteById(
    uint32_t sessionId)
{
    String filename;

    if (
        !_storage.findSessionById(
            sessionId,
            filename
        )
    )
    {
        sendJson(
            404,
            "{\"error\":\"Session not found\"}"
        );

        return;
    }


    // Never delete the file currently being written.

    if (
        _logger.recording() &&
        filename ==
            _logger.currentFilename()
    )
    {
        sendJson(
            409,
            "{\"error\":\"Cannot delete the active recording\"}"
        );

        return;
    }


    // Never delete the selected demo source. Without this file
    // DEMO mode would no longer be available after reboot.

    String demoFilename =
        _simulator.sourceFilename();

    if (
        demoFilename.length() > 0 &&
        filename ==
            demoFilename
    )
    {
        sendJson(
            403,
            "{\"error\":\"The demo source is protected and cannot be deleted\"}"
        );

        return;
    }


    String deletedFilename;

    if (
        !_storage.deleteSessionById(
            sessionId,
            deletedFilename
        )
    )
    {
        sendJson(
            500,
            "{\"error\":\"Unable to delete session\"}"
        );

        return;
    }


    JsonDocument doc;

    doc["deleted"] =
        true;

    doc["id"] =
        sessionId;

    doc["file"] =
        deletedFilename;

    doc["freeBytes"] =
        _storage.freeBytes();

    doc["usedBytes"] =
        _storage.usedBytes();

    double usedPercent =
        _storage.totalBytes() > 0
            ? (
                _storage.usedBytes() *
                100.0
            ) /
                _storage.totalBytes()
            : 0.0;

    doc["usedPercent"] =
        usedPercent;


    String response;

    serializeJson(
        doc,
        response
    );

    sendJson(
        200,
        response
    );
}


// ------------------------------------------------------------
// ROUTES
// ------------------------------------------------------------

void RaceSyncApi::begin()
{
    _server.enableCORS(
        true
    );

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


    // Dynamic ID endpoints are handled here because UriBraces
    // is not available in every ESP32 Arduino WebServer build.

    _server.onNotFound(
        [this]()
        {
            const String prefix =
                "/api/sessions/";

            if (
                _server.uri().startsWith(
                    prefix
                )
            )
            {
                uint32_t sessionId =
                    0;

                bool isId =
                    parseSessionIdFromUri(
                        sessionId
                    );


                // GET /api/sessions/{id}

                if (
                    _server.method() ==
                        HTTP_GET &&
                    isId
                )
                {
                    handleSessionDownloadById(
                        sessionId
                    );

                    return;
                }


                // DELETE /api/sessions/{id}

                if (
                    _server.method() ==
                        HTTP_DELETE
                )
                {
                    if (!isId)
                    {
                        sendJson(
                            400,
                            "{\"error\":\"DELETE requires a numeric session id\"}"
                        );

                        return;
                    }

                    handleSessionDeleteById(
                        sessionId
                    );

                    return;
                }


                // Backward-compatible filename download.

                if (
                    _server.method() ==
                        HTTP_GET &&
                    !isId
                )
                {
                    String filename =
                        _server.uri().substring(
                            prefix.length()
                        );

                    handleLegacySessionDownload(
                        filename
                    );

                    return;
                }
            }


            // CORS pre-flight.

            if (
                _server.method() ==
                    HTTP_OPTIONS
            )
            {
                addCorsHeaders();

                _server.send(
                    204
                );

                return;
            }


            sendJson(
                404,
                "{\"error\":\"Not found\"}"
            );
        }
    );

    _server.begin();

    Serial.println(
        "[HTTP] REST API ready"
    );
}


void RaceSyncApi::update()
{
    _server.handleClient();
}
