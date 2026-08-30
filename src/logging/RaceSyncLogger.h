#pragma once

#include <Arduino.h>
#include <FS.h>

#include "../config/RaceSyncTypes.h"
#include "RaceSyncStorage.h"

class RaceSyncLogger
{
public:
    bool begin(RaceSyncStorage& storage);
    void processSample(const Telemetry& telemetry, DataMode mode);
    bool manualStart(const Telemetry& telemetry, DataMode mode);
    bool manualStop();
    void forceStop();
    bool recording() const;
    bool manualSession() const;
    const String& currentFilename() const;
    uint32_t sampleCount() const;
    uint32_t storageWriteErrors() const;
    uint32_t lastWriteAgeMs() const;
    uint32_t recordingSeconds() const;

private:
    RaceSyncStorage* _storage = nullptr;
    bool _recording = false;
    bool _manualSession = false;
    bool _autoStartInhibit = false;
    File _file;
    File _kmlFile;
    String _filename;
    String _kmlFilename;
    uint32_t _sampleCount = 0;
    uint32_t _belowSpeedSince = 0;
    uint32_t _lastFlush = 0;
    uint32_t _lastWriteMs = 0;
    uint32_t _startedMs = 0;
    uint32_t _writeErrors = 0;
    uint32_t _lastStorageCheckMs = 0;

    bool start(const Telemetry& telemetry, DataMode mode, bool manual = false);
    void stop();
    bool storageHasSafeFreeSpace();
    void writeHeader(File& file, DataMode mode);
    void writeKmlHeader(File& file, const String& name);
    void writeKmlFooter(File& file);
    void writeSample(const Telemetry& telemetry);
    void writeKmlSample(const Telemetry& telemetry);
    String createFilename(const Telemetry& telemetry, DataMode mode) const;
    String createVBoxLine(const Telemetry& telemetry) const;
};
