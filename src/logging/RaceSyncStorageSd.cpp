#include "RaceSyncStorage.h"

#include <SPI.h>
#include <SD.h>
#include "../config/RaceSyncConfig.h"

bool RaceSyncStorage::begin()
{
    return beginSd();
}

bool RaceSyncStorage::beginSd()
{
    Serial.printf(
        "[STORE] Mounting SD (CS=%d MOSI=%d SCK=%d MISO=%d)\n",
        RaceSyncConfig::SD_CS_PIN,
        RaceSyncConfig::SD_MOSI_PIN,
        RaceSyncConfig::SD_SCK_PIN,
        RaceSyncConfig::SD_MISO_PIN
    );

    SPI.begin(
        RaceSyncConfig::SD_SCK_PIN,
        RaceSyncConfig::SD_MISO_PIN,
        RaceSyncConfig::SD_MOSI_PIN,
        RaceSyncConfig::SD_CS_PIN
    );

    if (!SD.begin(
        RaceSyncConfig::SD_CS_PIN,
        SPI,
        RaceSyncConfig::SD_SPI_FREQUENCY
    ))
    {
        Serial.println("[STORE] SD mount failed - session logging unavailable");
        _fs = nullptr;
        _ready = false;
        _readable = false;
        _writable = false;
        _lastError = "SD mount failed";
        return false;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("[STORE] No SD card detected - session logging unavailable");
        SD.end();
        _fs = nullptr;
        _ready = false;
        _readable = false;
        _writable = false;
        _lastError = "No SD card detected";
        return false;
    }

    _fs = &SD;
    _root = "/";
    _ready = true;

    Serial.printf(
        "[STORE] SD mounted: card=%llu MB total=%llu MB used=%llu MB\n",
        SD.cardSize() / (1024ULL * 1024ULL),
        SD.totalBytes() / (1024ULL * 1024ULL),
        SD.usedBytes() / (1024ULL * 1024ULL)
    );

    if (!runHealthCheck())
    {
        Serial.printf("[STORE] SD health check failed: %s\n", _lastError.c_str());
        return false;
    }

    Serial.println("[STORE] SD health check passed: readable + writable + deletable");

    // Convert any power-loss .part sessions before the API enumerates files.
    recoverInterruptedSessions();

    Serial.printf("[STORE] %u VBO file(s) on SD\n", sessionCount());
    return true;
}
