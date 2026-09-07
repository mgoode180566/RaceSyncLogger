#include "RaceSyncApi.h"

#include <ArduinoJson.h>
#include "../../include/Pins.h"

void RaceSyncApi::beginManualLoggingRoutes()
{
    // Lightweight, RAM-only state endpoint. This deliberately avoids session
    // enumeration and storage capacity calls so the UI can remain responsive
    // without competing with the active VBO write path.
    _server.on("/api/runtime", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["recording"] = _logger.recording();
        doc["manual"] = _logger.manualSession();
        doc["recordingSeconds"] = _logger.recordingSeconds();
        doc["samplesWritten"] = _logger.sampleCount();
        doc["currentFile"] = _logger.currentFilename();
        doc["gpsValid"] = _telemetry.valid;
        doc["gpsConnected"] = _gps.connected();
        doc["satellites"] = _telemetry.satellites;
        doc["speedKmh"] = _telemetry.velocityKmh;
        doc["rpm"] = _telemetry.revs;

        // RPM diagnostics are RAM-only and remain available both while IDLE
        // and while recording. No SD access is performed for these values.
        JsonObject rpm = doc["rpmDiagnostics"].to<JsonObject>();
        rpm["value"] = _telemetry.revs;
        rpm["rawMeasured"] = _telemetry.rpmRawMeasured;
        rpm["maxValidRpm"] = _telemetry.rpmMaxValid;
        rpm["ledEnabled"] = _telemetry.rpmLedEnabled;
        rpm["signalPresent"] = _telemetry.rpmSignalPresent;
        rpm["pulseCount"] = _telemetry.rpmPulseCount;
        rpm["rejectedPulseCount"] = _telemetry.rpmRejectedPulseCount;
        rpm["rejectedReadingCount"] = _telemetry.rpmRejectedReadingCount;
        rpm["lowSpikeCount"] = _telemetry.rpmLowSpikeCount;
        rpm["highSpikeCount"] = _telemetry.rpmHighSpikeCount;
        rpm["zeroDropCount"] = _telemetry.rpmZeroDropCount;
        rpm["minAccepted"] = _telemetry.rpmMinAccepted;
        rpm["maxAccepted"] = _telemetry.rpmMaxAccepted;
        rpm["lastPeriodUs"] = _telemetry.rpmLastPeriodUs;
        rpm["lastPulseAgeMs"] = _telemetry.rpmLastPulseAgeMs == UINT32_MAX ? -1 : static_cast<int64_t>(_telemetry.rpmLastPulseAgeMs);
        rpm["inputPin"] = Pin::RPM_INPUT;
        rpm["inputLevel"] = _telemetry.rpmInputLevel;

        doc["storageReady"] = _storage.ready();
        doc["storageWritable"] = _storage.writable();
        doc["racePriorityMode"] = _logger.recording();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/logging/start", HTTP_POST, [this]() {
        if (_logger.recording()) {
            sendJson(409, "{\"started\":false,\"error\":\"Logger is already recording\"}");
            return;
        }
        if (!_storage.ready()) {
            sendJson(503, "{\"started\":false,\"error\":\"SD storage not ready\"}");
            return;
        }
        if (!_storage.writable()) {
            JsonDocument doc;
            doc["started"] = false;
            doc["error"] = "SD storage is not writable";
            doc["storageError"] = _storage.lastError();
            String response;
            serializeJson(doc, response);
            sendJson(503, response);
            return;
        }
        if (!_telemetry.valid) {
            sendJson(409, "{\"started\":false,\"error\":\"Valid GPS fix required\"}");
            return;
        }

        if (!_logger.manualStart(_telemetry, _mode)) {
            JsonDocument doc;
            doc["started"] = false;
            doc["error"] = "Unable to start logging";
            if (_storage.lastError().length()) doc["storageError"] = _storage.lastError();
            String response;
            serializeJson(doc, response);
            sendJson(500, response);
            return;
        }

        JsonDocument doc;
        doc["started"] = true;
        doc["recording"] = true;
        doc["manual"] = true;
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

        // Automatic sessions are the race-critical path and cannot be stopped
        // from the web UI/API. Manual sessions retain Stop for bench testing.
        if (!_logger.manualSession()) {
            sendJson(423, "{\"stopped\":false,\"error\":\"Automatic race recording is protected from web stop\"}");
            return;
        }

        const String file = _logger.currentFilename();
        if (!_logger.manualStop()) {
            sendJson(500, "{\"stopped\":false,\"error\":\"Unable to stop logging\"}");
            return;
        }

        JsonDocument doc;
        doc["stopped"] = true;
        doc["recording"] = false;
        doc["wasManual"] = true;
        doc["file"] = file;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });
}
