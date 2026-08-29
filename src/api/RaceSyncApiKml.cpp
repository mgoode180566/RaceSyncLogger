#include "RaceSyncApi.h"

void RaceSyncApi::handleSessionKmlDownloadById(uint32_t sessionId)
{
    String vboFilename;
    if (!_storage.findSessionById(sessionId, vboFilename)) {
        sendJson(404, "{\"error\":\"Session not found\"}");
        return;
    }

    String kmlFilename = vboFilename.substring(0, vboFilename.length() - 4) + ".kml";
    File file = _storage.openFileRead(kmlFilename);
    if (!file) {
        sendJson(404, "{\"error\":\"KML file not found for this session\"}");
        return;
    }

    addCorsHeaders();
    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + kmlFilename + "\"");
    _server.sendHeader("X-RaceSync-Session-Id", String(sessionId));
    _server.sendHeader("Cache-Control", "no-store");
    _server.streamFile(file, "application/vnd.google-earth.kml+xml");
    file.close();
}

void RaceSyncApi::beginKmlDownloadRoute()
{
    _server.on(
        "/api/session-kml",
        HTTP_GET,
        [this]()
        {
            if (!_server.hasArg("id")) {
                sendJson(400, "{\"error\":\"Missing session id\"}");
                return;
            }

            String idValue = _server.arg("id");
            if (idValue.length() == 0) {
                sendJson(400, "{\"error\":\"Invalid session id\"}");
                return;
            }

            for (size_t i = 0; i < idValue.length(); i++) {
                if (idValue[i] < '0' || idValue[i] > '9') {
                    sendJson(400, "{\"error\":\"Invalid session id\"}");
                    return;
                }
            }

            uint32_t sessionId = (uint32_t)strtoul(idValue.c_str(), nullptr, 10);
            if (sessionId == 0) {
                sendJson(400, "{\"error\":\"Invalid session id\"}");
                return;
            }

            handleSessionKmlDownloadById(sessionId);
        }
    );
}
