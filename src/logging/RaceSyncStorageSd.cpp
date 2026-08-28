#include "RaceSyncStorage.h"

#include <SPI.h>
#include <SD.h>
#include "../config/RaceSyncConfig.h"

bool RaceSyncStorage::begin()
{
    if (beginSd())
        return true;

    Serial.println("[STORE] SD unavailable - falling back to LittleFS");
    return beginNvm();
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
        Serial.println("[STORE] SD mount failed");
        return false;
    }

    if (SD.cardType() == CARD_NONE)
    {
        Serial.println("[STORE] No SD card detected");
        SD.end();
        return false;
    }

    _fs = &SD;
    _root = "/";
    _ready = true;
    _usingSd = true;

    Serial.printf(
        "[STORE] SD ready: card=%llu MB total=%llu MB used=%llu MB\n",
        SD.cardSize() / (1024ULL * 1024ULL),
        SD.totalBytes() / (1024ULL * 1024ULL),
        SD.usedBytes() / (1024ULL * 1024ULL)
    );

    Serial.printf("[STORE] %u VBO file(s) on SD\n", sessionCount());
    return true;
}
