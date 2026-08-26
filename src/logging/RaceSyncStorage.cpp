#include "RaceSyncStorage.h"
#include <LittleFS.h>

bool RaceSyncStorage::beginNvm()
{
    Serial.println("[STORE] Mounting LittleFS");

    if (!LittleFS.begin(true))
    {
        Serial.println("[STORE] LittleFS mount failed");
        _ready = false;
        return false;
    }

    _fs = &LittleFS;
    _root = "/";
    _ready = true;

    Serial.printf("[STORE] %u VBO file(s)\n", sessionCount());
    return true;
}

bool RaceSyncStorage::ready() const
{
    return _ready;
}

String RaceSyncStorage::storageType() const
{
    return "NVM";
}

String RaceSyncStorage::filesystemName() const
{
    return "LittleFS";
}

String RaceSyncStorage::basename(const String& path) const
{
    int slash = path.lastIndexOf('/');
    return slash < 0 ? path : path.substring(slash + 1);
}

bool RaceSyncStorage::isVBox(const String& filename) const
{
    String lower = filename;
    lower.toLowerCase();
    return lower.endsWith(".vbo");
}

bool RaceSyncStorage::isSafeVBoxFilename(const String& filename) const
{
    if (filename.length() == 0) return false;
    if (filename.indexOf("..") >= 0) return false;
    if (filename.indexOf("/") >= 0) return false;
    if (filename.indexOf("\\") >= 0) return false;
    return isVBox(filename);
}

String RaceSyncStorage::pathFor(const String& filename) const
{
    return _root == "/" ? "/" + filename : _root + "/" + filename;
}

bool RaceSyncStorage::exists(const String& filename) const
{
    if (!_ready || _fs == nullptr || !isSafeVBoxFilename(filename)) return false;
    return _fs->exists(pathFor(filename));
}

File RaceSyncStorage::openRead(const String& filename)
{
    if (!exists(filename)) return File();
    return _fs->open(pathFor(filename), FILE_READ);
}

File RaceSyncStorage::openWrite(const String& filename)
{
    if (!_ready || _fs == nullptr || !isSafeVBoxFilename(filename)) return File();
    return _fs->open(pathFor(filename), FILE_WRITE);
}

uint32_t RaceSyncStorage::sessionCount()
{
    if (!_ready || _fs == nullptr) return 0;

    File root = _fs->open(_root);
    if (!root || !root.isDirectory()) return 0;

    uint32_t count = 0;
    File file = root.openNextFile();

    while (file)
    {
        if (!file.isDirectory())
        {
            String filename = basename(file.name());
            if (isVBox(filename)) count++;
        }
        file = root.openNextFile();
    }

    root.close();
    return count;
}

String RaceSyncStorage::findDemoSource()
{
    if (!_ready || _fs == nullptr) return "";

    File root = _fs->open(_root);
    if (!root || !root.isDirectory()) return "";

    File file = root.openNextFile();

    while (file)
    {
        if (!file.isDirectory())
        {
            String filename = basename(file.name());

            if (isVBox(filename) && !filename.startsWith("RS_"))
            {
                file.close();
                root.close();
                return filename;
            }
        }

        file = root.openNextFile();
    }

    root.close();
    return "";
}

void RaceSyncStorage::addSessionsToJson(JsonArray sessions)
{
    if (!_ready || _fs == nullptr) return;

    File root = _fs->open(_root);
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();

    while (file)
    {
        if (!file.isDirectory())
        {
            String filename = basename(file.name());

            if (isVBox(filename))
            {
                JsonObject session = sessions.add<JsonObject>();
                session["file"] = filename;
                session["sizeBytes"] = file.size();
                session["complete"] = true;
                session["downloadUrl"] = "/api/sessions/" + filename;
                session["generatedByRaceSync"] = filename.startsWith("RS_");
            }
        }

        file = root.openNextFile();
    }

    root.close();
}

uint64_t RaceSyncStorage::totalBytes() const
{
    return _ready ? LittleFS.totalBytes() : 0;
}

uint64_t RaceSyncStorage::usedBytes() const
{
    return _ready ? LittleFS.usedBytes() : 0;
}

uint64_t RaceSyncStorage::freeBytes() const
{
    uint64_t total = totalBytes();
    uint64_t used = usedBytes();
    return total >= used ? total - used : 0;
}
