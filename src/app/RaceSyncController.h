#pragma once
#include <Arduino.h>
#include <Preferences.h>

#include "../config/RaceSyncTypes.h"
#include "../logging/RaceSyncStorage.h"
#include "../logging/RaceSyncLogger.h"
#include "../gps/RaceSyncGps.h"
#include "../wifi/RaceSyncWifi.h"
#include "../sensors/RaceSyncSensors.h"
#include "../api/RaceSyncApi.h"

class RaceSyncController
{
public:
    RaceSyncController();

    void begin();
    void update();

private:
    enum DiagnosticCode : uint8_t
    {
        DIAG_WIFI    = 1,
        DIAG_STORAGE = 2,
        DIAG_LOGGER  = 3,
        DIAG_GPS     = 4,
        DIAG_SENSORS = 5
    };

    Telemetry _telemetry;
    DataMode _mode = DataMode::STARTING;

    RaceSyncStorage _storage;
    RaceSyncLogger _logger;
    RaceSyncGps _gps;
    RaceSyncWifi _wifi;
    RaceSyncSensors _sensors;

    Preferences _preferences;
    uint32_t _bootCount = 0;

    RaceSyncApi _api;

    uint32_t _loggingLedCycleStartedMs = 0;
    bool _loggingLedOn = false;

    void updateDataMode();
    void incrementBootCount();

    void setStatusLed(uint8_t red, uint8_t green, uint8_t blue);
    void updateLoggingLed();
    void setLoggingLed(bool on);

    uint8_t runStartupDiagnostics();
    bool waitForGpsTraffic(uint32_t timeoutMs);
    void showDiagnosticFailures(uint8_t failureMask);
    void flashFailureCode(uint8_t code);
    void showDiagnosticSuccess();
};
