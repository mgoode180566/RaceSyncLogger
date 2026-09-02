#pragma once

#include <Arduino.h>
#include <FS.h>
#include <Preferences.h>

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

    double startSpeedKmh() const;
    double stopSpeedKmh() const;
    uint32_t stopDelaySeconds() const;
    bool updateAutomaticSettings(double startSpeedKmh, uint32_t stopDelaySeconds);

private:
    RaceSyncStorage* _storage = nullptr;
    bool _recording = false;
    bool _manualSession = false;
    bool _autoStartInhibit = false;
    File _file;
    String _filename;
    String _partFilename;
    uint32_t _sampleCount = 0;
    uint32_t _belowSpeedSince = 0;
    uint32_t _lastFlush = 0;
    uint32_t _lastWriteMs = 0;
    uint32_t _startedMs = 0;
    uint32_t _writeErrors = 0;
    uint32_t _lastStorageCheckMs = 0;

    Preferences _settingsPreferences;
    double _startSpeedKmh = 10.0;
    double _stopSpeedKmh = 3.0;
    uint32_t _stopDelayMs = 60000;

    bool start(const Telemetry& telemetry, DataMode mode, bool manual = false);
    void stop(bool finalize = true);
    bool storageHasSafeFreeSpace();
    void writeHeader(File& file, DataMode mode);
    void writeSample(const Telemetry& telemetry);
    String createFilename(const Telemetry& telemetry, DataMode mode) const;
    String createVBoxLine(const Telemetry& telemetry) const;
    void loadAutomaticSettings();
};
