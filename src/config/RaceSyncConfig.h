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

    // The fitted MG-902 has been verified on the bench to boot at 9600 baud
    // and output NMEA. RaceSyncGps starts at this rate and configures the
    // receiver for the high-rate UBX stream used by the logger.
    constexpr uint32_t GPS_STARTUP_BAUD = 9600;
    constexpr uint32_t GPS_BAUD = 115200;

    constexpr uint32_t GPS_BOOT_GRACE_MS = 3000;
    constexpr uint32_t GPS_STALE_MS = 3000;

    // microSD card - SPI mode.
    // Wiring: CS=10, MOSI=11, SCK=12, MISO=13.
    constexpr int SD_CS_PIN = 10;
    constexpr int SD_MOSI_PIN = 11;
    constexpr int SD_SCK_PIN = 12;
    constexpr int SD_MISO_PIN = 13;
    constexpr uint32_t SD_SPI_FREQUENCY = 20000000;

    constexpr double LOG_START_SPEED_KMH = 10.0;
    constexpr double LOG_STOP_SPEED_KMH = 3.0;
    constexpr uint32_t LOG_STOP_DELAY_MS = 60000;
    constexpr uint32_t LOG_FLUSH_INTERVAL_MS = 1000;

    // Protect the filesystem from being completely consumed.
    // Logging will close the active file if free space falls below 1 MB.
    constexpr uint64_t MIN_FREE_STORAGE_BYTES = 1024ULL * 1024ULL;

    // Status thresholds.
    constexpr double STORAGE_WARNING_PERCENT = 90.0;
    constexpr double STORAGE_FULL_PERCENT = 98.0;

    constexpr uint32_t MIN_HEALTHY_HEAP_BYTES = 30000;
}
