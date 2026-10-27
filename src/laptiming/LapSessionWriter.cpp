#include "LapSessionWriter.h"
#include "UtcTime.h"
#include <cstring>

bool LapSessionWriter::failWrite() {
    ++_writeErrors;
    _error="Lap sidecar write failed; VBO recording continues";
    if(_csv) _csv.close();
    Serial.println("[LAPS] "+_error);
    return false;
}

bool LapSessionWriter::startSession(const String& vboFilename, JsonVariantConst database,
    bool configurationValid, const String& configurationError) {
    close();
    _writeErrors=0;
    _error="";
    const String base=vboFilename.substring(0,vboFilename.length()-4);
    _csvFilename=base+"_laps.csv";
    if(!_storage) return failWrite();
    _csv=_storage->openFileWrite(_csvFilename);
    const char* header="lap,circuit_id,start_utc,end_utc,lap_time_seconds,start_utc_ms,end_utc_ms\r\n";
    if(!_csv || _csv.print(header)!=strlen(header)) return failWrite();
    _csv.flush();
    if(_csv.getWriteError()) return failWrite();

    JsonDocument metadata;
    metadata["schemaVersion"]=1;
    metadata["algorithm"]="segment-intersection-v1";
    metadata["vboFile"]=vboFilename;
    metadata["lapFile"]=_csvFilename;
    metadata["timeBasis"]="GPS UTC, interpolated and rounded to milliseconds";
    metadata["configurationValid"]=configurationValid;
    metadata["configurationError"]=configurationError;
    metadata["circuitDatabase"].set(database);
    File snapshot=_storage->openFileWrite(base+"_laps.json");
    if(!snapshot) return failWrite();
    const bool written=serializeJson(metadata,snapshot)==measureJson(metadata);
    snapshot.flush();
    const bool ok=written && !snapshot.getWriteError();
    snapshot.close();
    return ok ? true : failWrite();
}

bool LapSessionWriter::append(const String& circuitId, const RaceSyncLaps::Crossing& lap) {
    if(!lap.completedLap) return true;
    if(!_csv) return false;
    char start[32],end[32],numbers[128];
    if(!RaceSyncLaps::formatUtcTimestamp(lap.startUtcMs,start,sizeof(start)) ||
       !RaceSyncLaps::formatUtcTimestamp(lap.endUtcMs,end,sizeof(end))) return failWrite();
    snprintf(numbers,sizeof(numbers),",%.3f,%llu,%llu\r\n",(lap.endUtcMs-lap.startUtcMs)/1000.0,
        static_cast<unsigned long long>(lap.startUtcMs),static_cast<unsigned long long>(lap.endUtcMs));
    const String row=String(lap.lap)+","+circuitId+","+start+","+end+numbers;
    const bool written=_csv.print(row)==row.length();
    _csv.flush();
    return written && !_csv.getWriteError() ? true : failWrite();
}

bool LapSessionWriter::close() {
    if(!_csv) return _error.isEmpty();
    _csv.flush();
    if(_csv.getWriteError()) return failWrite();
    _csv.close();
    return true;
}
