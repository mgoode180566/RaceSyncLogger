#include "RaceSyncStorage.h"

#include <LittleFS.h>
#include <SD.h>

bool RaceSyncStorage::beginNvm()
{
    Serial.println("[STORE] Mounting LittleFS");
    if (!LittleFS.begin(true)) { Serial.println("[STORE] LittleFS mount failed"); _ready = false; return false; }
    _fs = &LittleFS; _root = "/"; _ready = true; _usingSd = false;
    Serial.printf("[STORE] %u VBO file(s)\n", sessionCount()); return true;
}

bool RaceSyncStorage::ready() const { return _ready; }
String RaceSyncStorage::storageType() const { return _usingSd ? "SD" : "NVM"; }
String RaceSyncStorage::filesystemName() const { return _usingSd ? "FAT" : "LittleFS"; }
bool RaceSyncStorage::usingSd() const { return _ready && _usingSd; }

String RaceSyncStorage::cardTypeName() const
{
    if (!usingSd()) return "NONE";
    switch (SD.cardType()) { case CARD_MMC: return "MMC"; case CARD_SD: return "SDSC"; case CARD_SDHC: return "SDHC/SDXC"; case CARD_NONE: return "NONE"; default: return "UNKNOWN"; }
}
uint64_t RaceSyncStorage::cardSizeBytes() const { return usingSd() ? SD.cardSize() : 0; }
String RaceSyncStorage::basename(const String& path) const { int slash = path.lastIndexOf('/'); return slash < 0 ? path : path.substring(slash + 1); }
bool RaceSyncStorage::isVBox(const String& filename) const { String lower = filename; lower.toLowerCase(); return lower.endsWith(".vbo"); }
bool RaceSyncStorage::isSafeFilename(const String& filename) const { return filename.length() > 0 && filename.indexOf("..") < 0 && filename.indexOf('/') < 0 && filename.indexOf('\\') < 0; }
bool RaceSyncStorage::isSafeVBoxFilename(const String& filename) const { return isSafeFilename(filename) && isVBox(filename); }
String RaceSyncStorage::pathFor(const String& filename) const { return _root == "/" ? "/" + filename : _root + "/" + filename; }
bool RaceSyncStorage::fileExists(const String& filename) const { return _ready && _fs != nullptr && isSafeFilename(filename) && _fs->exists(pathFor(filename)); }
bool RaceSyncStorage::exists(const String& filename) const { return isSafeVBoxFilename(filename) && fileExists(filename); }
File RaceSyncStorage::openFileRead(const String& filename) { if (!fileExists(filename)) return File(); return _fs->open(pathFor(filename), FILE_READ); }
File RaceSyncStorage::openRead(const String& filename) { if (!isSafeVBoxFilename(filename)) return File(); return openFileRead(filename); }
File RaceSyncStorage::openWrite(const String& filename) { if (!_ready || _fs == nullptr || !isSafeVBoxFilename(filename)) return File(); return _fs->open(pathFor(filename), FILE_WRITE); }
File RaceSyncStorage::openFileWrite(const String& filename) { if (!_ready || _fs == nullptr || !isSafeFilename(filename)) return File(); return _fs->open(pathFor(filename), FILE_WRITE); }

uint32_t RaceSyncStorage::sessionCount()
{
    if (!_ready || _fs == nullptr) return 0; File root = _fs->open(_root); if (!root || !root.isDirectory()) return 0;
    uint32_t count = 0; File file = root.openNextFile(); while (file) { if (!file.isDirectory() && isVBox(basename(file.name()))) count++; file.close(); file = root.openNextFile(); } root.close(); return count;
}

uint32_t RaceSyncStorage::sessionIdForFilename(const String& filename) const
{
    String normalized = filename; normalized.toLowerCase(); uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < normalized.length(); i++) { hash ^= (uint8_t)normalized[i]; hash *= 16777619UL; }
    return hash == 0 ? 1 : hash;
}

bool RaceSyncStorage::findSessionById(uint32_t sessionId, String& filename)
{
    filename = ""; if (!_ready || _fs == nullptr || sessionId == 0) return false; File root = _fs->open(_root); if (!root || !root.isDirectory()) return false;
    bool found = false; File file = root.openNextFile(); while (file) { if (!file.isDirectory()) { String current = basename(file.name()); if (isVBox(current) && sessionIdForFilename(current) == sessionId) { if (found) { Serial.println("[STORE] Session ID collision"); file.close(); root.close(); filename = ""; return false; } filename = current; found = true; } } file.close(); file = root.openNextFile(); } root.close(); return found;
}

bool RaceSyncStorage::deleteSessionById(uint32_t sessionId, String& deletedFilename)
{
    deletedFilename = ""; String filename; if (!findSessionById(sessionId, filename) || !isSafeVBoxFilename(filename)) return false;
    String kmlFilename = filename.substring(0, filename.length() - 4) + ".kml";
    if (!_fs->remove(pathFor(filename))) return false;
    if (fileExists(kmlFilename) && !_fs->remove(pathFor(kmlFilename))) Serial.printf("[STORE] Warning: unable to delete companion %s\n", kmlFilename.c_str());
    deletedFilename = filename; Serial.printf("[STORE] Deleted session %u: %s (and companion KML if present)\n", sessionId, filename.c_str()); return true;
}

void RaceSyncStorage::addSessionsToJson(JsonArray sessions, const String& activeFilename)
{
    if (!_ready || _fs == nullptr) return; File root = _fs->open(_root); if (!root || !root.isDirectory()) return; File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) { String filename = basename(file.name()); if (isVBox(filename)) {
            uint32_t id = sessionIdForFilename(filename); bool active = activeFilename.length() && filename == activeFilename; bool deletable = !active;
            String kmlFilename = filename.substring(0, filename.length() - 4) + ".kml"; bool hasKml = fileExists(kmlFilename);
            JsonObject session = sessions.add<JsonObject>(); session["id"] = id; session["file"] = filename; session["sizeBytes"] = file.size(); session["complete"] = !active; session["active"] = active; session["deletable"] = deletable; session["generatedByRaceSync"] = filename.startsWith("RS_"); session["downloadUrl"] = "/api/sessions/" + String(id); session["hasKml"] = hasKml;
            if (hasKml) session["kmlDownloadUrl"] = "/api/session-kml?id=" + String(id); if (deletable) session["deleteUrl"] = "/api/sessions/" + String(id);
        }} file.close(); file = root.openNextFile();
    } root.close();
}

uint64_t RaceSyncStorage::totalBytes() const { if (!_ready) return 0; return _usingSd ? SD.totalBytes() : LittleFS.totalBytes(); }
uint64_t RaceSyncStorage::usedBytes() const { if (!_ready) return 0; return _usingSd ? SD.usedBytes() : LittleFS.usedBytes(); }
uint64_t RaceSyncStorage::freeBytes() const { uint64_t total = totalBytes(), used = usedBytes(); return total >= used ? total - used : 0; }
