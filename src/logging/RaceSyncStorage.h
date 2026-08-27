#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class RaceSyncStorage
{
public:
    bool beginNvm();
    bool ready() const;

    String storageType() const;
    String filesystemName() const;

    bool isSafeVBoxFilename(const String& filename) const;
    bool exists(const String& filename) const;

    File openRead(const String& filename);
    File openWrite(const String& filename);

    uint32_t sessionCount();

    // Stable opaque ID derived from the session filename.
    // The ID remains unchanged across reboots while the filename
    // remains unchanged. This avoids exposing filenames in API URLs.
    uint32_t sessionIdForFilename(const String& filename) const;

    // Resolve an opaque session ID back to the stored VBO filename.
    bool findSessionById(
        uint32_t sessionId,
        String& filename
    );

    // Delete a VBO identified by session ID.
    bool deleteSessionById(
        uint32_t sessionId,
        String& deletedFilename
    );

    String findDemoSource();

    // Build the API session list.
    // activeFilename is protected while logging.
    // protectedFilename is normally the demo source VBO.
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

    String basename(const String& path) const;
    String pathFor(const String& filename) const;
    bool isVBox(const String& filename) const;
};
