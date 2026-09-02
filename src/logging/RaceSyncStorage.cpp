#include "RaceSyncStorage.h"

#include <SD.h>
#include <vector>

bool RaceSyncStorage::ready() const { return _ready; }
bool RaceSyncStorage::readable() const { return _readable; }
bool RaceSyncStorage::writable() const { return _writable; }
const String& RaceSyncStorage::lastError() const { return _lastError; }
void RaceSyncStorage::setError(const String& message) { _lastError = message; Serial.print("[STORE] "); Serial.println(message); }

bool RaceSyncStorage::selfTestOpenOk() const { return _selfTestOpenOk; }
size_t RaceSyncStorage::selfTestBytesWritten() const { return _selfTestBytesWritten; }
bool RaceSyncStorage::selfTestExistsAfterClose() const { return _selfTestExistsAfterClose; }
bool RaceSyncStorage::selfTestReadBackOpenOk() const { return _selfTestReadBackOpenOk; }
size_t RaceSyncStorage::selfTestReadBackBytes() const { return _selfTestReadBackBytes; }
bool RaceSyncStorage::selfTestDeleteOk() const { return _selfTestDeleteOk; }
const String& RaceSyncStorage::selfTestFilename() const { return _selfTestFilename; }

String RaceSyncStorage::storageType() const { return "SD"; }
String RaceSyncStorage::filesystemName() const { return "FAT"; }
bool RaceSyncStorage::usingSd() const { return _ready; }

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

bool RaceSyncStorage::removeFile(const String& filename)
{
    if (!_ready || _fs == nullptr || !isSafeFilename(filename)) { setError("Invalid or unavailable file remove request"); return false; }
    const String path = pathFor(filename);
    if (!_fs->exists(path)) return true;
    if (!_fs->remove(path)) { setError("Failed to remove " + filename); return false; }
    if (_fs->exists(path)) { setError("Remove reported success but file still exists: " + filename); return false; }
    return true;
}

File RaceSyncStorage::openFileRead(const String& filename) { if (!fileExists(filename)) return File(); return _fs->open(pathFor(filename), FILE_READ); }
File RaceSyncStorage::openRead(const String& filename) { if (!isSafeVBoxFilename(filename)) return File(); return openFileRead(filename); }
File RaceSyncStorage::openWrite(const String& filename) { if (!_ready || !_writable || _fs == nullptr || !isSafeVBoxFilename(filename)) return File(); return _fs->open(pathFor(filename), FILE_WRITE); }
File RaceSyncStorage::openFileWrite(const String& filename) { if (!_ready || !_writable || _fs == nullptr || !isSafeFilename(filename)) return File(); return _fs->open(pathFor(filename), FILE_WRITE); }

bool RaceSyncStorage::runHealthCheck()
{
    _readable = false;
    _writable = false;
    _selfTestOpenOk = false;
    _selfTestBytesWritten = 0;
    _selfTestExistsAfterClose = false;
    _selfTestReadBackOpenOk = false;
    _selfTestReadBackBytes = 0;
    _selfTestDeleteOk = false;

    if (!_ready || _fs == nullptr) { setError("SD not mounted"); return false; }

    File root = _fs->open(_root);
    if (!root || !root.isDirectory()) { if (root) root.close(); setError("SD root directory is not readable"); return false; }
    root.close();
    _readable = true;

    const String testPath = pathFor(_selfTestFilename);
    if (_fs->exists(testPath)) {
        if (!_fs->remove(testPath)) {
            setError("Could not remove stale SD test file");
            return false;
        }
    }

    File test = _fs->open(testPath, FILE_WRITE);
    _selfTestOpenOk = (bool)test;
    Serial.printf("[STORE-TEST] file=%s open=%s\n", _selfTestFilename.c_str(), _selfTestOpenOk ? "true" : "false");
    if (!_selfTestOpenOk) { setError("SD write test could not create file"); return false; }

    const char* marker = "RaceSync SD test";
    _selfTestBytesWritten = test.print(marker);
    test.flush();
    test.close();

    _selfTestExistsAfterClose = _fs->exists(testPath);
    Serial.printf("[STORE-TEST] bytesWritten=%u existsAfterClose=%s\n",
                  (unsigned)_selfTestBytesWritten,
                  _selfTestExistsAfterClose ? "true" : "false");

    if (_selfTestBytesWritten == 0) { setError("SD write returned zero bytes"); return false; }
    if (!_selfTestExistsAfterClose) { setError("SD test file missing after close"); return false; }

    File verify = _fs->open(testPath, FILE_READ);
    _selfTestReadBackOpenOk = (bool)verify;
    if (_selfTestReadBackOpenOk) {
        _selfTestReadBackBytes = verify.size();
        verify.close();
    }
    Serial.printf("[STORE-TEST] readBackOpen=%s readBackBytes=%u\n",
                  _selfTestReadBackOpenOk ? "true" : "false",
                  (unsigned)_selfTestReadBackBytes);

    if (!_selfTestReadBackOpenOk) { setError("SD read-back test could not open file"); return false; }
    if (_selfTestReadBackBytes == 0) { setError("SD read-back file is empty"); return false; }

    const bool removeReturned = _fs->remove(testPath);
    const bool stillExists = _fs->exists(testPath);
    _selfTestDeleteOk = removeReturned && !stillExists;
    Serial.printf("[STORE-TEST] removeReturned=%s existsAfterDelete=%s deleteOk=%s\n",
                  removeReturned ? "true" : "false",
                  stillExists ? "true" : "false",
                  _selfTestDeleteOk ? "true" : "false");

    if (!_selfTestDeleteOk) { setError("SD delete test failed"); return false; }

    _writable = true;
    _lastError = "";
    return true;
}

uint32_t RaceSyncStorage::sessionCount()
{
    if (!_ready || _fs == nullptr) return 0;
    File root = _fs->open(_root); if (!root || !root.isDirectory()) { if (root) root.close(); setError("Unable to enumerate SD root"); _readable = false; return 0; }
    uint32_t count = 0; File file = root.openNextFile();
    while (file) { if (!file.isDirectory() && isVBox(basename(file.name()))) count++; file.close(); file = root.openNextFile(); }
    root.close(); _readable = true; return count;
}

uint32_t RaceSyncStorage::sessionIdForFilename(const String& filename) const
{
    String normalized = filename; normalized.toLowerCase(); uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < normalized.length(); i++) { hash ^= (uint8_t)normalized[i]; hash *= 16777619UL; }
    return hash == 0 ? 1 : hash;
}

bool RaceSyncStorage::findSessionById(uint32_t sessionId, String& filename)
{
    filename = ""; if (!_ready || _fs == nullptr || sessionId == 0) return false;
    File root = _fs->open(_root); if (!root || !root.isDirectory()) { if (root) root.close(); setError("Unable to scan SD for session"); _readable = false; return false; }
    bool found = false; File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String current = basename(file.name());
            if (isVBox(current) && sessionIdForFilename(current) == sessionId) {
                if (found) { file.close(); root.close(); setError("Session ID collision"); filename = ""; return false; }
                filename = current; found = true;
            }
        }
        file.close(); file = root.openNextFile();
    }
    root.close(); _readable = true; return found;
}

bool RaceSyncStorage::deleteSessionById(uint32_t sessionId, String& deletedFilename)
{
    deletedFilename = "";
    String filename;
    if (!findSessionById(sessionId, filename) || !isSafeVBoxFilename(filename)) { setError("Session not found for delete"); return false; }

    const String kmlFilename = filename.substring(0, filename.length() - 4) + ".kml";
    const bool hasKml = fileExists(kmlFilename);

    if (!removeFile(filename)) return false;
    if (hasKml && !removeFile(kmlFilename)) return false;

    deletedFilename = filename;
    _lastError = "";
    Serial.printf("[STORE] Deleted session %u: %s%s\n", sessionId, filename.c_str(), hasKml ? " + KML" : "");
    return true;
}

void RaceSyncStorage::addSessionsToJson(JsonArray sessions, const String& activeFilename)
{
    if (!_ready || _fs == nullptr) return;

    struct SessionInfo { String filename; size_t sizeBytes; };
    std::vector<SessionInfo> found;

    File root = _fs->open(_root);
    if (!root || !root.isDirectory()) { if (root) root.close(); setError("Unable to enumerate sessions"); _readable = false; return; }
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            String filename = basename(file.name());
            if (isVBox(filename)) found.push_back({filename, (size_t)file.size()});
        }
        file.close(); file = root.openNextFile();
    }
    root.close(); _readable = true;

    for (const SessionInfo& info : found) {
        const String& filename = info.filename;
        uint32_t id = sessionIdForFilename(filename);
        bool active = activeFilename.length() && filename == activeFilename;
        bool deletable = !active;
        // Completed VBO sessions can always be converted to KML on demand.
        // No persistent KML companion is written during recording.
        bool kmlAvailable = !active;

        JsonObject session = sessions.add<JsonObject>();
        session["id"] = id;
        session["file"] = filename;
        session["sizeBytes"] = info.sizeBytes;
        session["complete"] = !active;
        session["active"] = active;
        session["deletable"] = deletable;
        session["generatedByRaceSync"] = filename.startsWith("RS_");
        session["downloadUrl"] = "/api/sessions/" + String(id);
        session["hasKml"] = kmlAvailable;
        session["kmlGeneratedOnDemand"] = true;
        if (kmlAvailable) session["kmlDownloadUrl"] = "/api/session-kml?id=" + String(id);
        if (deletable) session["deleteUrl"] = "/api/sessions/" + String(id);
    }
}

uint64_t RaceSyncStorage::totalBytes() const { return _ready ? SD.totalBytes() : 0; }
uint64_t RaceSyncStorage::usedBytes() const { return _ready ? SD.usedBytes() : 0; }
uint64_t RaceSyncStorage::freeBytes() const { uint64_t total = totalBytes(), used = usedBytes(); return total >= used ? total - used : 0; }
