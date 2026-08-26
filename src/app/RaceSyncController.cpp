#include "RaceSyncController.h"
#include "../config/RaceSyncConfig.h"

RaceSyncController::RaceSyncController()
    : _api(
        _storage,
        _logger,
        _gps,
        _wifi,
        _simulator,
        _telemetry,
        _mode,
        _bootCount
    )
{
}

void RaceSyncController::incrementBootCount()
{
    _preferences.begin(
        "racesync",
        false
    );

    _bootCount =
        _preferences.getUInt(
            "bootCount",
            0
        ) + 1;

    _preferences.putUInt(
        "bootCount",
        _bootCount
    );
}

void RaceSyncController::begin()
{
    Serial.println();
    Serial.println("=============================");
    Serial.println(" RaceSync V2.1 Modular");
    Serial.println("=============================");

    incrementBootCount();

    _wifi.begin();

    _storage.beginNvm();

    _logger.begin(
        _storage
    );

    _simulator.begin(
        _storage
    );

    _gps.begin();

    _sensors.begin();

    _api.begin();

    Serial.println("[MODE] Waiting for GPS...");
}

void RaceSyncController::updateDataMode()
{
    bool gpsPresent =
        _gps.connected();

    if (gpsPresent)
    {
        if (
            !_logger.recording() ||
            _mode == DataMode::LIVE
        )
        {
            _mode =
                DataMode::LIVE;
        }

        return;
    }

    if (
        _mode ==
            DataMode::STARTING &&
        millis() >=
            RaceSyncConfig::GPS_BOOT_GRACE_MS
    )
    {
        if (
            _simulator.available()
        )
        {
            _mode =
                DataMode::DEMO;

            Serial.println(
                "[MODE] GPS absent -> DEMO"
            );
        }
    }

    if (
        _mode ==
            DataMode::LIVE &&
        !_logger.recording() &&
        _simulator.available()
    )
    {
        _mode =
            DataMode::DEMO;

        Serial.println(
            "[MODE] GPS lost -> DEMO"
        );
    }
}

void RaceSyncController::update()
{
    _gps.update(
        _telemetry
    );

    updateDataMode();

    bool newSample =
        false;

    if (
        _mode ==
            DataMode::LIVE &&
        _gps.connected()
    )
    {
        // NAV-PVT updates telemetry in RaceSyncGps::update().
        // A simple change detector is enough here because
        // sampleIndex increments only when a NAV-PVT packet arrives.
        static uint32_t lastLiveIndex = 0;

        if (
            _telemetry.sampleIndex !=
            lastLiveIndex
        )
        {
            lastLiveIndex =
                _telemetry.sampleIndex;

            newSample =
                true;
        }
    }
    else if (
        _mode ==
            DataMode::DEMO
    )
    {
        newSample =
            _simulator.update(
                _telemetry
            );
    }

    _sensors.update();

    if (newSample)
    {
        _logger.processSample(
            _telemetry,
            _mode
        );
    }

    _api.update();

    delay(1);
}
