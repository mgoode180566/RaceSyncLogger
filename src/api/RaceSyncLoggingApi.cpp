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

        const GoProStatus cameraStatus = _goPro.status();
        JsonObject camera = doc["camera"].to<JsonObject>();
        camera["enabled"] = cameraStatus.enabled;
        camera["connected"] = cameraStatus.connected;
        camera["statusValid"] = cameraStatus.statusValid;
        camera["recording"] = cameraStatus.recording;
        camera["videoStartPending"] = cameraStatus.videoStartPending;
        camera["videoStartSent"] = cameraStatus.videoStartSent;
        camera["videoStartConfirmed"] = cameraStatus.videoStartConfirmed;
        camera["videoStartRequests"] = cameraStatus.videoStartRequests;
        camera["videoStartErrors"] = cameraStatus.videoStartErrors;
        camera["videoStopPending"] = cameraStatus.videoStopPending;
        camera["videoStopSent"] = cameraStatus.videoStopSent;
        camera["videoStopConfirmed"] = cameraStatus.videoStopConfirmed;
        camera["videoStopRequests"] = cameraStatus.videoStopRequests;
        camera["videoStopErrors"] = cameraStatus.videoStopErrors;
        camera["state"] = cameraStatus.state;
        if (cameraStatus.lastError[0] != '\0') camera["lastError"] = cameraStatus.lastError;

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

        // VBO logging is already active before any camera command is queued.
        const GoProStatus cameraBefore = _goPro.status();
        const bool cameraAlreadyRecording = cameraBefore.connected && cameraBefore.statusValid && cameraBefore.recording;
        bool cameraVideoStartQueued = false;

        if (cameraAlreadyRecording)
        {
            cameraVideoStartQueued = true;
            _logger.logSessionDiagnosticEvent("GOPRO_ALREADY_RECORDING");
            Serial.println("[GOPRO] Manual logging started while GoPro was already recording; no shutter command sent");
        }
        else
        {
            cameraVideoStartQueued = _goPro.queueVideoStart();
            _logger.logSessionDiagnosticEvent(cameraVideoStartQueued ? "GOPRO_VIDEO_START_QUEUED" : "GOPRO_VIDEO_START_NOT_QUEUED");
        }

        const GoProStatus cameraAfter = _goPro.status();

        JsonDocument doc;
        doc["started"] = true;
        doc["recording"] = true;
        doc["manual"] = true;
        doc["file"] = _logger.currentFilename();
        doc["cameraConnected"] = cameraAfter.connected;
        doc["cameraAlreadyRecording"] = cameraAlreadyRecording;
        doc["cameraVideoStartQueued"] = cameraVideoStartQueued;
        doc["cameraVideoStartPending"] = cameraAfter.videoStartPending;
        doc["cameraVideoStartConfirmed"] = cameraAfter.videoStartConfirmed || cameraAlreadyRecording;
        doc["cameraRecording"] = cameraAfter.recording || cameraAlreadyRecording;
        doc["cameraState"] = cameraAfter.state;
        if (!cameraVideoStartQueued && cameraAfter.lastError[0] != '\0') doc["cameraError"] = cameraAfter.lastError;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/logging/stop", HTTP_POST, [this]() {
        if (!_logger.recording()) {
            sendJson(409, "{\"stopped\":false,\"error\":\"Logger is not recording\"}");
            return;
        }

        if (!_logger.manualSession()) {
            sendJson(423, "{\"stopped\":false,\"error\":\"Automatic race recording is protected from web stop\"}");
            return;
        }

        const String file = _logger.currentFilename();
        const GoProStatus cameraAtStop = _goPro.status();
        if (cameraAtStop.connected)
            _logger.logSessionDiagnosticEvent(cameraAtStop.recording ? "GOPRO_RECORDING_AT_LOGGER_STOP" : "GOPRO_NOT_RECORDING_AT_LOGGER_STOP");
        else
            _logger.logSessionDiagnosticEvent("GOPRO_DISCONNECTED_AT_LOGGER_STOP");

        // Close and finalise the VBO before touching the camera. Camera failure
        // cannot prevent the session from being safely completed.
        if (!_logger.manualStop()) {
            sendJson(500, "{\"stopped\":false,\"error\":\"Unable to stop logging\"}");
            return;
        }

        const bool cameraVideoStopQueued = _goPro.queueVideoStop();
        const GoProStatus cameraAfterStopQueue = _goPro.status();

        JsonDocument doc;
        doc["stopped"] = true;
        doc["recording"] = false;
        doc["wasManual"] = true;
        doc["file"] = file;
        doc["cameraConnected"] = cameraAfterStopQueue.connected;
        doc["cameraRecording"] = cameraAfterStopQueue.recording;
        doc["cameraVideoStopQueued"] = cameraVideoStopQueued;
        doc["cameraVideoStopPending"] = cameraAfterStopQueue.videoStopPending;
        if (!cameraVideoStopQueued && cameraAfterStopQueue.lastError[0] != '\0') doc["cameraError"] = cameraAfterStopQueue.lastError;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });
}
