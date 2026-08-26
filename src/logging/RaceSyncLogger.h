#pragma once

#include <Arduino.h>
#include <FS.h>

#include "../config/RaceSyncTypes.h"
#include "RaceSyncStorage.h"

class RaceSyncLogger
{
public:
    bool begin(RaceSyncStorage& storage);

    void processSample(
        const Telemetry& telemetry,
        DataMode mode
    );

    // Used by the controller when a source session ends,
    // for example at the end of a one-shot demo replay.
    void forceStop();

    bool recording() const;

    const String& currentFilename() const;

    uint32_t sampleCount() const;

    uint32_t storageWriteErrors() const;

    uint32_t lastWriteAgeMs() const;

    uint32_t recordingSeconds() const;

private:
    RaceSyncStorage* _storage =
        nullptr;

    bool _recording =
        false;

    File _file;

    String _filename;

    uint32_t _sampleCount =
        0;

    uint32_t _belowSpeedSince =
        0;

    uint32_t _lastFlush =
        0;

    uint32_t _lastWriteMs =
        0;

    uint32_t _startedMs =
        0;

    uint32_t _writeErrors =
        0;

    uint32_t _lastStorageCheckMs =
        0;

    bool start(
        const Telemetry& telemetry,
        DataMode mode
    );

    void stop();

    bool storageHasSafeFreeSpace();

    void writeHeader(
        File& file,
        DataMode mode
    );

    void writeSample(
        const Telemetry& telemetry
    );

    String createFilename(
        const Telemetry& telemetry,
        DataMode mode
    ) const;

    String createVBoxLine(
        const Telemetry& telemetry
    ) const;
};
