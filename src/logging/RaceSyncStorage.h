#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class RaceSyncStorage
{
public:
    bool begin();
    bool beginSd();
    bool beginNvm();
    bool ready() const;

    String storageType() const;
    String filesystemName() const;
    bool usingSd() const;
    String cardTypeName() const;
    uint64_t cardSizeBytes() const;

    bool isSafeVBoxFilename(const String& filename) const;
    bool isSafeFilename(const String& filename) const;
    bool fileExists(const String& filename) const;
    bool exists(const String& filename) const;
    File openRead(const String& filename);
    File openFileRead(const String& filename);
    File openWrite(const String& filename);
    File openFileWrite(const String& filename);
    uint32_t sessionCount();
    uint32_t sessionIdForFilename(const String& filename) const;
    bool findSessionById(uint32_t sessionId, String& filename);
    bool deleteSessionById(uint32_t sessionId, String& deletedFilename);
    void addSessionsToJson(JsonArray sessions, const String& activeFilename = "");

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
