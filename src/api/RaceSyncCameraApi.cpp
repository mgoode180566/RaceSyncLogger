#include "RaceSyncApi.h"

#include <ArduinoJson.h>

void RaceSyncApi::handleCameraStatus(int httpStatus)
{
    const GoProStatus camera = _goPro.status();
    JsonDocument doc;
    doc["type"] = "GoPro HERO9";
    doc["transport"] = "BLE";
    doc["manualControl"] = true;
    doc["commandsAllowed"] = !_logger.recording();
    doc["enabled"] = camera.enabled;
    doc["state"] = camera.state;
    doc["scanning"] = camera.scanning;
    doc["discovered"] = camera.discovered;
    doc["connected"] = camera.connected;
    doc["name"] = camera.name;
    doc["address"] = camera.address;
    doc["rssi"] = camera.rssi;
    doc["statusValid"] = camera.statusValid;
    doc["recording"] = camera.recording;
    doc["busy"] = camera.busy;
    doc["ready"] = camera.ready;
    doc["overheating"] = camera.overheating;
    doc["sdCardError"] = camera.sdCardError;
    doc["batteryPercent"] = camera.batteryPercent;
    doc["remainingVideoSeconds"] = camera.remainingVideoSeconds;
    doc["lastStatusAgeMs"] = camera.lastStatusAgeMs == UINT32_MAX ? -1 : static_cast<int64_t>(camera.lastStatusAgeMs);
    doc["connectionAttempts"] = camera.connectionAttempts;
    doc["successfulQueries"] = camera.successfulQueries;
    doc["queryErrors"] = camera.queryErrors;
    doc["videoStartPending"] = camera.videoStartPending;
    doc["videoStartSent"] = camera.videoStartSent;
    doc["videoStartConfirmed"] = camera.videoStartConfirmed;
    doc["videoStartRequests"] = camera.videoStartRequests;
    doc["videoStartErrors"] = camera.videoStartErrors;
    doc["lastError"] = camera.lastError;

    String response;
    serializeJson(doc, response);
    sendJson(httpStatus, response);
}

void RaceSyncApi::beginCameraRoutes()
{
    Serial.println("[CAMERA-API] Registering camera routes");

    _server.on("/api/camera", HTTP_GET, [this]() {
        Serial.println("[CAMERA-API] GET /api/camera");
        handleCameraStatus();
    });

    _server.on("/api/camera/connect", HTTP_POST, [this]() {
        Serial.printf("[CAMERA-API] POST /api/camera/connect recording=%s\n",
                      _logger.recording() ? "true" : "false");
        if (_logger.recording())
        {
            Serial.println("[CAMERA-API] Connect blocked: RaceSync is recording");
            sendJson(423, "{\"error\":\"Camera commands are disabled while RaceSync is recording\",\"racePriorityMode\":true}");
            return;
        }

        Serial.println("[CAMERA-API] Passing connect request to GoPro BLE client");
        handleCameraStatus(_goPro.connect() ? 200 : 503);
    });

    _server.on("/api/camera/disconnect", HTTP_POST, [this]() {
        Serial.printf("[CAMERA-API] POST /api/camera/disconnect recording=%s\n",
                      _logger.recording() ? "true" : "false");
        if (_logger.recording())
        {
            Serial.println("[CAMERA-API] Disconnect blocked: RaceSync is recording");
            sendJson(423, "{\"error\":\"Camera commands are disabled while RaceSync is recording\",\"racePriorityMode\":true}");
            return;
        }
        _goPro.disconnect();
        handleCameraStatus();
    });

    _server.on("/api/camera/refresh", HTTP_POST, [this]() {
        Serial.printf("[CAMERA-API] POST /api/camera/refresh recording=%s\n",
                      _logger.recording() ? "true" : "false");
        if (_logger.recording())
        {
            Serial.println("[CAMERA-API] Refresh blocked: RaceSync is recording");
            sendJson(423, "{\"error\":\"Camera commands are disabled while RaceSync is recording\",\"racePriorityMode\":true}");
            return;
        }
        handleCameraStatus(_goPro.refreshStatus() ? 200 : 409);
    });
}
