#pragma once
#include <Arduino.h>

namespace RaceSyncConfig
{
    constexpr const char* PRODUCT = "RaceSync";
    constexpr const char* FIRMWARE = "V2.1";

    constexpr const char* WIFI_SSID = "RaceSync";
    constexpr const char* WIFI_PASSWORD = "racesync";

    constexpr int GPS_RX_PIN = 16;
    constexpr int GPS_TX_PIN = 17;
    constexpr uint32_t GPS_BAUD = 115200;

    constexpr uint32_t GPS_BOOT_GRACE_MS = 3000;
    constexpr uint32_t GPS_STALE_MS = 3000;

    constexpr double LOG_START_SPEED_KMH = 10.0;
    constexpr double LOG_STOP_SPEED_KMH = 3.0;
    constexpr uint32_t LOG_STOP_DELAY_MS = 60000;
    constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 1000;

    constexpr uint32_t MIN_HEALTHY_HEAP_BYTES = 30000;
}
