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
    void observeGpsHealth(bool connected, bool fixValid, uint32_t packetAgeMs);
    bool manualStart(const Telemetry& telemetry, DataMode mode);
    bool manualStop();
    void forceStop();
    bool recording() const;
    bool manualSession() const;
    const String& currentFilename() const;
    const String& currentLogFilename() const;
    uint32_t sampleCount() const;
    uint32_t storageWriteErrors() const;
    uint32_t lastWriteAgeMs() const;
    uint32_t recordingSeconds() const;

    double startSpeedKmh() const;
    double stopSpeedKmh() const;
    uint32_t stopDelaySeconds() const;
    bool updateAutomaticSettings(double startSpeedKmh, uint32_t stopDelaySeconds);

    const String& lastStopReason() const;
    const String& lastSessionFilename() const;
    const String& lastSessionLogFilename() const;
    uint64_t lastSessionFileSizeBytes() const;
    uint32_t lastSessionSamples() const;
    uint32_t lastSessionDurationSeconds() const;
    double lastStopSpeedKmh() const;
    uint32_t lastStopGpsAgeMs() const;
    bool lastStopGpsFixValid() const;
    uint8_t lastStopSatellites() const;
    uint32_t gpsDropouts() const;
    uint32_t maxGpsPacketAgeMs() const;
    uint32_t invalidGpsSamples() const;

private:
    RaceSyncStorage* _storage = nullptr;
    bool _recording = false;
    bool _manualSession = false;
    bool _autoStartInhibit = false;
    File _file;
    File _logFile;
    String _filename;
    String _partFilename;
    String _logFilename;
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

    Telemetry _lastTelemetry{};
    bool _haveLastTelemetry = false;
    bool _gpsStale = false;
    uint32_t _currentGpsAgeMs = UINT32_MAX;
    uint32_t _sessionGpsDropouts = 0;
    uint32_t _sessionMaxGpsPacketAgeMs = 0;
    uint32_t _sessionInvalidGpsSamples = 0;
    bool _stationaryCandidateLogged = false;

    String _lastStopReason = "NONE";
    String _lastSessionFilename;
    String _lastSessionLogFilename;
    uint64_t _lastSessionFileSizeBytes = 0;
    uint32_t _lastSessionSamples = 0;
    uint32_t _lastSessionDurationSeconds = 0;
    double _lastStopSpeedKmh = 0.0;
    uint32_t _lastStopGpsAgeMs = UINT32_MAX;
    bool _lastStopGpsFixValid = false;
    uint8_t _lastStopSatellites = 0;
    uint32_t _lastGpsDropouts = 0;
    uint32_t _lastMaxGpsPacketAgeMs = 0;
    uint32_t _lastInvalidGpsSamples = 0;

    bool start(const Telemetry& telemetry, DataMode mode, bool manual = false);
    void stop(bool finalize = true, const char* reason = "UNKNOWN");
    bool storageHasSafeFreeSpace();
    void writeHeader(File& file, DataMode mode);
    void writeSample(const Telemetry& telemetry);
    void writeDiagnosticEvent(const char* event, const Telemetry* telemetry = nullptr);
    void writeDiagnosticSummary(bool finalized);
    String createFilename(const Telemetry& telemetry, DataMode mode) const;
    String createVBoxLine(const Telemetry& telemetry) const;
    String telemetryTimestamp(const Telemetry& telemetry) const;
    void loadAutomaticSettings();
};
