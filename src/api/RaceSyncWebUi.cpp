#include "RaceSyncApi.h"
#include "../ui/RaceSyncUiPages.h"

namespace
{
void addSessionMetadata(RaceSyncStorage& storage, JsonObject session)
{
    const String filename = session["file"].as<String>();
    String logFilename = filename;
    if (logFilename.endsWith(".vbo")) logFilename.replace(".vbo", ".log");

    String startTime;
    String endTime;
    long elapsedSeconds = -1;
    bool finalized = false;
    bool metadataFound = false;

    File log = storage.openFileRead(logFilename);
    if (log)
    {
        metadataFound = true;
        while (log.available())
        {
            String line = log.readStringUntil('\n');
            line.trim();
            const int eq = line.indexOf('=');
            if (eq <= 0) continue;

            const String key = line.substring(0, eq);
            const String value = line.substring(eq + 1);

            if (key == "startTime") startTime = value;
            else if (key == "endTime") endTime = value;
            else if (key == "durationSeconds") elapsedSeconds = value.toInt();
            else if (key == "vboFinalized") finalized = value == "true";
        }
        log.close();
    }

    session["metadataAvailable"] = metadataFound;
    if (startTime.length()) session["startTime"] = startTime;
    if (endTime.length()) session["endTime"] = endTime;
    if (elapsedSeconds >= 0) session["elapsedSeconds"] = elapsedSeconds;
    session["successful"] = session["complete"].as<bool>() && (!metadataFound || finalized);
}

void sendUiPage(WebServer& server, const char* page)
{
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", page);
}
}

void RaceSyncApi::beginWebUiRoute()
{
    _server.on("/", HTTP_GET, [this]() {
        sendUiPage(_server, RACESYNC_SESSIONS_UI);
    });

    _server.on("/status", HTTP_GET, [this]() {
        sendUiPage(_server, RACESYNC_STATUS_UI);
    });

    _server.on("/control", HTTP_GET, [this]() {
        sendUiPage(_server, RACESYNC_CONTROL_UI);
    });

    _server.on("/api/session-summaries", HTTP_GET, [this]() {
        if (_logger.recording())
        {
            sendJson(423, "{\"error\":\"Session access suspended while recording\",\"racePriorityMode\":true}");
            return;
        }

        JsonDocument doc;
        doc["device"] = "RaceSync";
        JsonArray sessions = doc["sessions"].to<JsonArray>();
        _storage.addSessionsToJson(sessions, "");
        for (JsonObject session : sessions)
        {
            addSessionMetadata(_storage, session);
        }
        doc["count"] = sessions.size();

        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });
}
