#pragma once

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class RaceSyncStorage
{
public:
    bool begin();
    bool beginSd();
    bool ready() const;
    bool readable() const;
    bool writable() const;
    const String& lastError() const;
    bool runHealthCheck();

    bool selfTestOpenOk() const;
    size_t selfTestBytesWritten() const;
    bool selfTestExistsAfterClose() const;
    bool selfTestReadBackOpenOk() const;
    size_t selfTestReadBackBytes() const;
    bool selfTestDeleteOk() const;
    const String& selfTestFilename() const;

    String storageType() const;
    String filesystemName() const;
    bool usingSd() const;
    String cardTypeName() const;
    uint64_t cardSizeBytes() const;

    bool isSafeVBoxFilename(const String& filename) const;
    bool isSafeFilename(const String& filename) const;
    bool fileExists(const String& filename) const;
    bool exists(const String& filename) const;
    bool removeFile(const String& filename);
    File openRead(const String& filename);
    File openFileRead(const String& filename);
    File openWrite(const String& filename);
    File openPartWrite(const String& filename);
    bool finalizePartFile(const String& partFilename, const String& finalFilename);
    void recoverInterruptedSessions();
    uint32_t recoveredPartFiles() const;
    uint32_t failedPartRecoveries() const;
    const String& lastRecoveredFilename() const;
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
    bool _readable = false;
    bool _writable = false;
    String _lastError;

    uint32_t _recoveredPartFiles = 0;
    uint32_t _failedPartRecoveries = 0;
    String _lastRecoveredFilename;

    String _selfTestFilename = "RACESYNC_TEST.TXT";
    bool _selfTestOpenOk = false;
    size_t _selfTestBytesWritten = 0;
    bool _selfTestExistsAfterClose = false;
    bool _selfTestReadBackOpenOk = false;
    size_t _selfTestReadBackBytes = 0;
    bool _selfTestDeleteOk = false;

    String basename(const String& path) const;
    String pathFor(const String& filename) const;
    bool isVBox(const String& filename) const;
    bool isPart(const String& filename) const;
    bool recoverPartFile(const String& partFilename);
    String availableRecoveredFilename(const String& partFilename) const;
    void setError(const String& message);
};
