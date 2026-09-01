#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "../config/RaceSyncTypes.h"
#include "../logging/RaceSyncStorage.h"
#include "../logging/RaceSyncLogger.h"
#include "../gps/RaceSyncGps.h"
#include "../wifi/RaceSyncWifi.h"

class RaceSyncApi
{
public:
    RaceSyncApi(
        RaceSyncStorage& storage,
        RaceSyncLogger& logger,
        RaceSyncGps& gps,
        RaceSyncWifi& wifi,
        Telemetry& telemetry,
        DataMode& mode,
        uint32_t& bootCount
    );

    void begin();
    void beginKmlDownloadRoute();
    void beginWebUiRoute();
    void beginManualLoggingRoutes();
    void beginSettingsRoutes();
    void update();

private:
    WebServer _server = WebServer(80);
    RaceSyncStorage& _storage;
    RaceSyncLogger& _logger;
    RaceSyncGps& _gps;
    RaceSyncWifi& _wifi;
    Telemetry& _telemetry;
    DataMode& _mode;
    uint32_t& _bootCount;

    void addCorsHeaders();
    void sendJson(int status, const String& body);
    void handleStatus();
    void handleLocation();
    void handleTelemetry();
    void handleSessions();
    void handleSessionDownloadById(uint32_t sessionId);
    void handleSessionKmlDownloadById(uint32_t sessionId);
    void handleSessionDeleteById(uint32_t sessionId);
    void handleLegacySessionDownload(const String& filename);
    bool parseSessionIdFromUri(uint32_t& sessionId) const;
    static String formatUptime();
    static String formatVBoxTime(double rawTime);
    static const char* resetReasonName();
};
