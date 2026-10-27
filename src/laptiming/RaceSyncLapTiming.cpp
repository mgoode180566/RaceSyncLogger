#include "RaceSyncLapTiming.h"
#include "DefaultCircuits.h"
#include <cmath>
#include <limits>

namespace {
bool textValid(const String& s, size_t maximum) {
    if(s.isEmpty() || s.length()>maximum) return false;
    for(size_t i=0;i<s.length();++i) if(static_cast<uint8_t>(s[i])<32) return false;
    return true;
}
bool identifierValid(const String& id) {
    if(!textValid(id,64)) return false;
    for(size_t i=0;i<id.length();++i) {
        const char c=id[i];
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_')) return false;
    }
    return true;
}
bool readPoint(JsonVariantConst value, RaceSyncLaps::Point& point) {
    if(!value["latitude"].is<double>()||!value["longitude"].is<double>()) return false;
    point.latitude=value["latitude"].as<double>();point.longitude=value["longitude"].as<double>();
    return RaceSyncLaps::validPoint(point);
}
bool readNumber(JsonObjectConst obj,const char* key,double& value) {
    if(obj[key].isNull()) return true;
    if(!obj[key].is<double>()) return false;
    value=obj[key].as<double>();return std::isfinite(value);
}
}
void RaceSyncLapTiming::begin(RaceSyncStorage& storage) {
    _storage=&storage;
    ensureDatabase();
}
bool RaceSyncLapTiming::ensureDatabase() {
    if(!_storage || !_storage->ready() || !_storage->writable()) {
        _error="Circuit database unavailable: SD card not writable";return false;
    }
    if(_storage->fileExists("circuits.json")) return true;
    File file=_storage->openFileWrite("circuits.json");
    if(!file) {_error="Unable to create circuits.json";return false;}
    const size_t length=sizeof(DEFAULT_CIRCUITS_JSON)-1;
    const bool written=file.write(reinterpret_cast<const uint8_t*>(DEFAULT_CIRCUITS_JSON),length)==length;
    file.flush();const bool ok=written && !file.getWriteError();file.close();
    if(!ok) {
        _storage->removeFile("circuits.json");_error="Unable to write circuits.json";return false;
    }
    Serial.println("[LAPS] Created circuits.json with Cadwell start/finish endpoints");
    return true;
}
bool RaceSyncLapTiming::loadDatabase() {
    _circuits.clear();_database.clear();_selectedId="";
    if(!ensureDatabase()) return false;
    File file=_storage->openFileRead("circuits.json");
    if(!file || file.isDirectory() || file.size()>32768) {
        if(file) file.close();_error="circuits.json missing, unreadable or larger than 32 KiB";return false;
    }
    const DeserializationError parse=deserializeJson(_database,file);file.close();
    JsonArrayConst entries=_database["circuits"].as<JsonArrayConst>();
    if(parse || !_database["schemaVersion"].is<int>() || _database["schemaVersion"].as<int>()!=1 ||
       entries.isNull() || entries.size()>32 || !_database["selectedCircuitId"].is<const char*>()) {
        _error="Invalid circuits.json: expected schemaVersion 1, selectedCircuitId and up to 32 circuits";return false;
    }
    _selectedId=_database["selectedCircuitId"].as<String>();
    for(JsonObjectConst entry:entries) {
        Circuit c;
        if(!entry["id"].is<const char*>() || !entry["name"].is<const char*>() || !entry["layout"].is<const char*>()) {
            _error="Circuit id, name and layout must be strings";return false;
        }
        c.id=entry["id"].as<String>();c.name=entry["name"].as<String>();c.layout=entry["layout"].as<String>();
        bool ok=identifierValid(c.id)&&textValid(c.name,96)&&textValid(c.layout,96);
        for(const Circuit& existing:_circuits) if(existing.id==c.id) ok=false;
        ok=ok && readPoint(entry["startFinish"]["point1"],c.settings.point1) &&
                 readPoint(entry["startFinish"]["point2"],c.settings.point2) &&
                 readNumber(entry,"identificationRadiusMetres",c.radius) && c.radius>=50 && c.radius<=5000 &&
                 readNumber(entry,"minimumLapSeconds",c.settings.minimumLapSeconds) &&
                 readNumber(entry,"minimumSpeedKmh",c.settings.minimumSpeedKmh) &&
                 readNumber(entry,"maximumSpeedKmh",c.settings.maximumSpeedKmh) &&
                 readNumber(entry,"rearmDistanceMetres",c.settings.rearmDistanceMetres) &&
                 readNumber(entry,"crossingHeadingDegrees",c.settings.crossingHeadingDegrees);
        if(!entry["maximumGapMs"].isNull()) {
            if(!entry["maximumGapMs"].is<uint32_t>()) ok=false;
            else c.settings.maximumGapMs=entry["maximumGapMs"].as<uint32_t>();
        }
        RaceSyncLaps::LapTimer validator;
        if(!ok || !validator.configure(c.settings)) {_error="Invalid circuit definition: "+c.id;return false;}
        _circuits.push_back(c);
    }
    if(_selectedId.length()) {
        bool found=false;for(const Circuit& c:_circuits) if(c.id==_selectedId) found=true;
        if(!found) {_error="selectedCircuitId does not match a circuit";return false;}
    }
    return true;
}
void RaceSyncLapTiming::failWrite() {
    ++_writeErrors;_error="Lap sidecar write failed; VBO recording continues";
    if(_csv) _csv.close();
    _timer.invalidate();Serial.println("[LAPS] "+_error);
}
void RaceSyncLapTiming::startSession(const String& vboFilename) {
    stopSession();_selected=-1;_writeErrors=0;_error="";_selection="waiting_for_circuit";
    _timer=RaceSyncLaps::LapTimer();
    const bool loaded=loadDatabase();
    const String base=vboFilename.substring(0,vboFilename.length()-4);
    _csvFilename=base+"_laps.csv";_metadataFilename=base+"_laps.json";
    _csv=_storage->openFileWrite(_csvFilename);
    const char* header="lap,circuit_id,start_utc,end_utc,lap_time_seconds,start_utc_ms,end_utc_ms\r\n";
    if(!_csv || _csv.print(header)!=strlen(header)) {failWrite();return;}
    _csv.flush();if(_csv.getWriteError()) {failWrite();return;}
    // Snapshot the exact database for reproducible comparisons even after edits.
    JsonDocument metadata;
    metadata["schemaVersion"]=1;metadata["algorithm"]="segment-intersection-v1";
    metadata["vboFile"]=vboFilename;metadata["lapFile"]=_csvFilename;
    metadata["timeBasis"]="GPS UTC, interpolated and rounded to milliseconds";
    metadata["configurationValid"]=loaded;metadata["configurationError"]=_error;
    metadata["circuitDatabase"].set(_database.as<JsonVariantConst>());
    File snapshot=_storage->openFileWrite(_metadataFilename);
    const size_t expected=measureJson(metadata);
    if(!snapshot) {failWrite();return;}
    const bool written=serializeJson(metadata,snapshot)==expected;
    snapshot.flush();const bool ok=written&&!snapshot.getWriteError();snapshot.close();
    if(!ok) {failWrite();return;}
    if(!loaded) {
        _selection="invalid_configuration";_csv.close();Serial.println("[LAPS] "+_error);
    }
}
void RaceSyncLapTiming::selectCircuit(const RaceSyncLaps::Point& position) {
    int best=-1;double first=std::numeric_limits<double>::max(),second=first;
    for(size_t i=0;i<_circuits.size();++i) {
        const Circuit& c=_circuits[i];
        if(_selectedId.length()&&c.id!=_selectedId) continue;
        const RaceSyncLaps::Point reference{(c.settings.point1.latitude+c.settings.point2.latitude)/2,
                                         (c.settings.point1.longitude+c.settings.point2.longitude)/2};
        const double distance=RaceSyncLaps::distanceMetres(position,reference);
        if(distance>c.radius) continue;
        if(distance<first) {second=first;first=distance;best=i;}
        else if(distance<second) second=distance;
    }
    if(best<0) return;
    if(second-first<100) {_selection="ambiguous_circuits: set selectedCircuitId";return;}
    _selected=best;_timer.configure(_circuits[best].settings);_selection="selected";
    Serial.println("[LAPS] Circuit selected: "+_circuits[best].id);
}
String RaceSyncLapTiming::utcTimestamp(uint64_t ms) {
    uint64_t days=ms/86400000;uint32_t tod=ms%86400000;
    int year=1970,month=1;
    auto leap=[](int y){return y%4==0&&(y%100!=0||y%400==0);};
    while(days>=static_cast<uint64_t>(leap(year)?366:365)) {days-=leap(year)?366:365;++year;}
    const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    while(days>=static_cast<uint64_t>(lengths[month-1]+(month==2&&leap(year)))) {
        days-=lengths[month-1]+(month==2&&leap(year));++month;
    }
    char text[32];snprintf(text,sizeof(text),"%04d-%02d-%02uT%02u:%02u:%02u.%03uZ",year,month,
        static_cast<unsigned>(days+1),tod/3600000,(tod/60000)%60,(tod/1000)%60,tod%1000);
    return String(text);
}
void RaceSyncLapTiming::process(const Telemetry& t) {
    if(!_csv) return;
    const double whole=t.hour*10000.0+t.minute*100.0+t.second;
    const uint64_t utc=t.timeValid?RaceSyncLaps::utcMilliseconds(t.year,t.month,t.day,t.hour,t.minute,t.second,t.rawTime-whole):0;
    RaceSyncLaps::Sample sample;
    sample.position={t.latitude,t.longitude};sample.utcMs=utc;sample.speedKmh=t.velocityKmh;
    sample.valid=t.valid && t.timeValid && t.satellites>=6 && utc!=0;
    if(!sample.valid || !RaceSyncLaps::validPoint(sample.position)) {_timer.invalidate();return;}
    if(_selected<0) selectCircuit(sample.position);
    if(_selected<0) return;
    const RaceSyncLaps::Crossing crossing=_timer.process(sample);
    if(!crossing.completedLap) return;
    char numbers[128];
    snprintf(numbers,sizeof(numbers),",%.3f,%llu,%llu\r\n",(crossing.endUtcMs-crossing.startUtcMs)/1000.0,
        static_cast<unsigned long long>(crossing.startUtcMs),static_cast<unsigned long long>(crossing.endUtcMs));
    const String row=String(crossing.lap)+","+_circuits[_selected].id+","+
        utcTimestamp(crossing.startUtcMs)+","+utcTimestamp(crossing.endUtcMs)+numbers;
    const bool written=_csv.print(row)==row.length();
    _csv.flush();if(!written||_csv.getWriteError()) failWrite();
}
void RaceSyncLapTiming::stopSession() {
    if(_csv) {_csv.flush();if(_csv.getWriteError()) failWrite();else _csv.close();}
    _timer.invalidate();
}
void RaceSyncLapTiming::addStatus(JsonObject status) const {
    status["circuitId"]=_selected>=0?_circuits[_selected].id:String("");
    status["selection"]=_selection;status["lapFile"]=_csvFilename;
    status["completedLaps"]=_timer.completedLaps();status["lapRunning"]=_timer.lapRunning();
    status["writeErrors"]=_writeErrors;status["error"]=_error;
}
