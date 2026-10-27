#include "CircuitDatabase.h"
#include "DefaultCircuits.h"
#include <cmath>

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
void CircuitDatabase::begin(RaceSyncStorage& storage) {
    _storage=&storage;
    ensureDatabase();
}
bool CircuitDatabase::ensureDatabase() {
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
bool CircuitDatabase::load() {
    _circuits.clear();_locations.clear();_database.clear();_selectedId="";_error="";
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
        CircuitDefinition c;
        if(!entry["id"].is<const char*>() || !entry["name"].is<const char*>() || !entry["layout"].is<const char*>()) {
            _error="Circuit id, name and layout must be strings";return false;
        }
        c.id=entry["id"].as<String>();c.name=entry["name"].as<String>();c.layout=entry["layout"].as<String>();
        bool ok=identifierValid(c.id)&&textValid(c.name,96)&&textValid(c.layout,96);
        for(const CircuitDefinition& existing:_circuits) if(existing.id==c.id) ok=false;
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
        bool found=false;for(const CircuitDefinition& c:_circuits) if(c.id==_selectedId) found=true;
        if(!found) {_error="selectedCircuitId does not match a circuit";return false;}
    }
    for(const CircuitDefinition& c:_circuits)
        _locations.push_back(RaceSyncLaps::CircuitLocation(c.settings.point1,c.settings.point2,c.radius));
    return true;
}
int CircuitDatabase::preferredIndex() const {
    if(_selectedId.isEmpty()) return -1;
    for(size_t i=0;i<_circuits.size();++i)
        if(_circuits[i].id==_selectedId) return static_cast<int>(i);
    return -1; // load() rejects unknown explicit IDs.
}
