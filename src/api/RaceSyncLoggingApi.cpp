#include "RaceSyncApi.h"

#include <ArduinoJson.h>

void RaceSyncApi::beginManualLoggingRoutes()
{
    _server.on("/api/logging/start", HTTP_POST, [this]() {
        if (!_storage.ready()) {
            sendJson(503, "{\"started\":false,\"error\":\"SD storage not ready\"}");
            return;
        }
        if (!_telemetry.valid) {
            sendJson(409, "{\"started\":false,\"error\":\"Valid GPS fix required\"}");
            return;
        }

        const bool alreadyRecording = _logger.recording();
        if (!_logger.manualStart(_telemetry, _mode)) {
            sendJson(500, "{\"started\":false,\"error\":\"Unable to start logging\"}");
            return;
        }

        JsonDocument doc;
        doc["started"] = true;
        doc["recording"] = _logger.recording();
        doc["manual"] = _logger.manualSession();
        doc["alreadyRecording"] = alreadyRecording;
        doc["file"] = _logger.currentFilename();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/logging/stop", HTTP_POST, [this]() {
        if (!_logger.recording()) {
            sendJson(409, "{\"stopped\":false,\"error\":\"Logger is not recording\"}");
            return;
        }

        const String file = _logger.currentFilename();
        const bool wasManual = _logger.manualSession();
        if (!_logger.manualStop()) {
            sendJson(500, "{\"stopped\":false,\"error\":\"Unable to stop logging\"}");
            return;
        }

        JsonDocument doc;
        doc["stopped"] = true;
        doc["recording"] = false;
        doc["wasManual"] = wasManual;
        doc["file"] = file;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });
}
