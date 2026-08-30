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
    void updateLoggingLed();
    void setLoggingLed(bool on);
};
