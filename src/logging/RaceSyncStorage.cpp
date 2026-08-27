#include "RaceSyncStorage.h"

#include <LittleFS.h>


bool RaceSyncStorage::beginNvm()
{
    Serial.println(
        "[STORE] Mounting LittleFS"
    );

    if (!LittleFS.begin(true))
    {
        Serial.println(
            "[STORE] LittleFS mount failed"
        );

        _ready = false;

        return false;
    }

    _fs = &LittleFS;
    _root = "/";
    _ready = true;

    Serial.printf(
        "[STORE] %u VBO file(s)\n",
        sessionCount()
    );

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


String RaceSyncStorage::basename(
    const String& path) const
{
    int slash =
        path.lastIndexOf('/');

    return
        slash < 0
            ? path
            : path.substring(
                slash + 1
            );
}


bool RaceSyncStorage::isVBox(
    const String& filename) const
{
    String lower =
        filename;

    lower.toLowerCase();

    return
        lower.endsWith(
            ".vbo"
        );
}


bool RaceSyncStorage::isSafeVBoxFilename(
    const String& filename) const
{
    if (
        filename.length() == 0
    )
    {
        return false;
    }

    if (
        filename.indexOf("..") >= 0
    )
    {
        return false;
    }

    if (
        filename.indexOf("/") >= 0
    )
    {
        return false;
    }

    if (
        filename.indexOf("\\") >= 0
    )
    {
        return false;
    }

    return
        isVBox(
            filename
        );
}


String RaceSyncStorage::pathFor(
    const String& filename) const
{
    return
        _root == "/"
            ? "/" + filename
            : _root + "/" + filename;
}


bool RaceSyncStorage::exists(
    const String& filename) const
{
    if (
        !_ready ||
        _fs == nullptr ||
        !isSafeVBoxFilename(
            filename
        )
    )
    {
        return false;
    }

    return
        _fs->exists(
            pathFor(
                filename
            )
        );
}


File RaceSyncStorage::openRead(
    const String& filename)
{
    if (
        !exists(
            filename
        )
    )
    {
        return File();
    }

    return
        _fs->open(
            pathFor(
                filename
            ),
            FILE_READ
        );
}


File RaceSyncStorage::openWrite(
    const String& filename)
{
    if (
        !_ready ||
        _fs == nullptr ||
        !isSafeVBoxFilename(
            filename
        )
    )
    {
        return File();
    }

    return
        _fs->open(
            pathFor(
                filename
            ),
            FILE_WRITE
        );
}


uint32_t RaceSyncStorage::sessionCount()
{
    if (
        !_ready ||
        _fs == nullptr
    )
    {
        return 0;
    }

    File root =
        _fs->open(
            _root
        );

    if (
        !root ||
        !root.isDirectory()
    )
    {
        return 0;
    }

    uint32_t count =
        0;

    File file =
        root.openNextFile();

    while (file)
    {
        if (
            !file.isDirectory()
        )
        {
            String filename =
                basename(
                    file.name()
                );

            if (
                isVBox(
                    filename
                )
            )
            {
                count++;
            }
        }

        file =
            root.openNextFile();
    }

    root.close();

    return count;
}


// ------------------------------------------------------------
// SESSION ID
//
// RaceSync exposes an opaque integer ID instead of using the
// physical filename in its REST API.
//
// FNV-1a is deterministic, tiny and fast on ESP32.
// Lower-casing makes the ID insensitive to filename case.
//
// The filename remains the actual storage key internally.
// ------------------------------------------------------------

uint32_t RaceSyncStorage::sessionIdForFilename(
    const String& filename) const
{
    String normalized =
        filename;

    normalized.toLowerCase();

    uint32_t hash =
        2166136261UL;

    for (
        size_t i = 0;
        i < normalized.length();
        i++
    )
    {
        hash ^=
            (uint8_t)normalized[i];

        hash *=
            16777619UL;
    }

    // Reserve zero as "invalid/no session".
    if (
        hash == 0
    )
    {
        hash =
            1;
    }

    return hash;
}


bool RaceSyncStorage::findSessionById(
    uint32_t sessionId,
    String& filename)
{
    filename = "";

    if (
        !_ready ||
        _fs == nullptr ||
        sessionId == 0
    )
    {
        return false;
    }

    File root =
        _fs->open(
            _root
        );

    if (
        !root ||
        !root.isDirectory()
    )
    {
        return false;
    }

    bool found =
        false;

    String matchedFilename;

    File file =
        root.openNextFile();

    while (file)
    {
        if (
            !file.isDirectory()
        )
        {
            String current =
                basename(
                    file.name()
                );

            if (
                isVBox(
                    current
                ) &&
                sessionIdForFilename(
                    current
                ) ==
                sessionId
            )
            {
                // A 32-bit hash collision is extremely unlikely,
                // but do not resolve ambiguously if one occurs.
                if (found)
                {
                    Serial.println(
                        "[STORE] Session ID collision"
                    );

                    file.close();
                    root.close();

                    filename = "";

                    return false;
                }

                matchedFilename =
                    current;

                found =
                    true;
            }
        }

        file =
            root.openNextFile();
    }

    root.close();

    if (found)
    {
        filename =
            matchedFilename;
    }

    return found;
}


bool RaceSyncStorage::deleteSessionById(
    uint32_t sessionId,
    String& deletedFilename)
{
    deletedFilename = "";

    if (
        !_ready ||
        _fs == nullptr
    )
    {
        return false;
    }

    String filename;

    if (
        !findSessionById(
            sessionId,
            filename
        )
    )
    {
        return false;
    }

    if (
        !isSafeVBoxFilename(
            filename
        )
    )
    {
        return false;
    }

    bool removed =
        _fs->remove(
            pathFor(
                filename
            )
        );

    if (removed)
    {
        deletedFilename =
            filename;

        Serial.print(
            "[STORE] Deleted session "
        );

        Serial.print(
            sessionId
        );

        Serial.print(
            ": "
        );

        Serial.println(
            filename
        );
    }

    return removed;
}


String RaceSyncStorage::findDemoSource()
{
    if (
        !_ready ||
        _fs == nullptr
    )
    {
        return "";
    }

    File root =
        _fs->open(
            _root
        );

    if (
        !root ||
        !root.isDirectory()
    )
    {
        return "";
    }

    File file =
        root.openNextFile();

    while (file)
    {
        if (
            !file.isDirectory()
        )
        {
            String filename =
                basename(
                    file.name()
                );

            if (
                isVBox(
                    filename
                ) &&
                !filename.startsWith(
                    "RS_"
                )
            )
            {
                file.close();
                root.close();

                return filename;
            }
        }

        file =
            root.openNextFile();
    }

    root.close();

    return "";
}


void RaceSyncStorage::addSessionsToJson(
    JsonArray sessions,
    const String& activeFilename,
    const String& protectedFilename)
{
    if (
        !_ready ||
        _fs == nullptr
    )
    {
        return;
    }

    File root =
        _fs->open(
            _root
        );

    if (
        !root ||
        !root.isDirectory()
    )
    {
        return;
    }

    File file =
        root.openNextFile();

    while (file)
    {
        if (
            !file.isDirectory()
        )
        {
            String filename =
                basename(
                    file.name()
                );

            if (
                isVBox(
                    filename
                )
            )
            {
                uint32_t sessionId =
                    sessionIdForFilename(
                        filename
                    );

                bool active =
                    activeFilename.length() > 0 &&
                    filename ==
                        activeFilename;

                bool protectedFile =
                    protectedFilename.length() > 0 &&
                    filename ==
                        protectedFilename;

                bool generated =
                    filename.startsWith(
                        "RS_"
                    );

                bool deletable =
                    !active &&
                    !protectedFile;

                JsonObject session =
                    sessions.add<JsonObject>();

                session["id"] =
                    sessionId;

                // Filename remains informational only.
                // Client operations should use the ID.
                session["file"] =
                    filename;

                session["sizeBytes"] =
                    file.size();

                session["complete"] =
                    !active;

                session["active"] =
                    active;

                session["protected"] =
                    protectedFile;

                session["deletable"] =
                    deletable;

                session["generatedByRaceSync"] =
                    generated;

                session["downloadUrl"] =
                    "/api/sessions/" +
                    String(
                        sessionId
                    );

                if (deletable)
                {
                    session["deleteUrl"] =
                        "/api/sessions/" +
                        String(
                            sessionId
                        );
                }
            }
        }

        file =
            root.openNextFile();
    }

    root.close();
}


uint64_t RaceSyncStorage::totalBytes() const
{
    return
        _ready
            ? LittleFS.totalBytes()
            : 0;
}


uint64_t RaceSyncStorage::usedBytes() const
{
    return
        _ready
            ? LittleFS.usedBytes()
            : 0;
}


uint64_t RaceSyncStorage::freeBytes() const
{
    uint64_t total =
        totalBytes();

    uint64_t used =
        usedBytes();

    return
        total >= used
            ? total - used
            : 0;
}
