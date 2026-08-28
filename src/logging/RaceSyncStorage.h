#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class RaceSyncStorage
{
public:
    // Prefer the removable SD card for VBO sessions. If it cannot be mounted,
    // fall back to LittleFS so RaceSync remains usable for diagnostics.
    bool begin();
    bool beginSd();
    bool beginNvm();
    bool ready() const;

    String storageType() const;
    String filesystemName() const;

    bool isSafeVBoxFilename(const String& filename) const;
    bool exists(const String& filename) const;

    File openRead(const String& filename);
    File openWrite(const String& filename);

    uint32_t sessionCount();

    uint32_t sessionIdForFilename(const String& filename) const;

    bool findSessionById(
        uint32_t sessionId,
        String& filename
    );

    bool deleteSessionById(
        uint32_t sessionId,
        String& deletedFilename
    );

    String findDemoSource();

    void addSessionsToJson(
        JsonArray sessions,
        const String& activeFilename = "",
        const String& protectedFilename = ""
    );

    uint64_t totalBytes() const;
    uint64_t usedBytes() const;
    uint64_t freeBytes() const;

private:
    fs::FS* _fs = nullptr;
    String _root = "/";
    bool _ready = false;
    bool _usingSd = false;

    String basename(const String& path) const;
    String pathFor(const String& filename) const;
    bool isVBox(const String& filename) const;
};
