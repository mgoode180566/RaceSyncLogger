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
    String findDemoSource();
    void addSessionsToJson(JsonArray sessions);

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
