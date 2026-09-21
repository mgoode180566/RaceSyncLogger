#include "RaceSyncApi.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_system.h>
#include "../../include/Pins.h"

void RaceSyncApi::beginSettingsRoutes()
{
    _server.on("/api/settings/logging", HTTP_GET, [this]()
    {
        JsonDocument doc;
        doc["startSpeedKmh"] = _logger.startSpeedKmh();
        doc["stopSpeedKmh"] = _logger.stopSpeedKmh();
        doc["stopDelaySeconds"] = _logger.stopDelaySeconds();
        doc["recording"] = _logger.recording();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/logging", HTTP_POST, [this]()
    {
        if (_logger.recording())
        {
            sendJson(409, "{\"error\":\"Logging settings cannot be changed while recording\"}");
            return;
        }

        if (!_server.hasArg("plain"))
        {
            sendJson(400, "{\"error\":\"Missing JSON body\"}");
            return;
        }

        JsonDocument input;
        DeserializationError error = deserializeJson(input, _server.arg("plain"));
        if (error)
        {
            sendJson(400, "{\"error\":\"Invalid JSON\"}");
            return;
        }

        if (!input["startSpeedKmh"].is<double>() || !input["stopDelaySeconds"].is<uint32_t>())
        {
            sendJson(400, "{\"error\":\"startSpeedKmh and stopDelaySeconds are required\"}");
            return;
        }

        const double startSpeedKmh = input["startSpeedKmh"].as<double>();
        const uint32_t stopDelaySeconds = input["stopDelaySeconds"].as<uint32_t>();

        if (!_logger.updateAutomaticSettings(startSpeedKmh, stopDelaySeconds))
        {
            sendJson(400, "{\"error\":\"Invalid settings. Start speed must be 1-100 km/h and stop delay 1-600 seconds\"}");
            return;
        }

        JsonDocument doc;
        doc["saved"] = true;
        doc["startSpeedKmh"] = _logger.startSpeedKmh();
        doc["stopSpeedKmh"] = _logger.stopSpeedKmh();
        doc["stopDelaySeconds"] = _logger.stopDelaySeconds();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/rpm", HTTP_GET, [this]()
    {
        JsonDocument doc;
        doc["maxValidRpm"] = _telemetry.rpmMaxValid > 0.0 ? _telemetry.rpmMaxValid : 11000.0;
        doc["ledEnabled"] = _telemetry.rpmLedEnabled;
        doc["recording"] = _logger.recording();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/rpm", HTTP_POST, [this]()
    {
        if (_logger.recording())
        {
            sendJson(409, "{\"error\":\"Stop recording before changing the RPM limit\"}");
            return;
        }

        JsonDocument input;
        if (!_server.hasArg("plain") || deserializeJson(input, _server.arg("plain")) || !input["maxValidRpm"].is<double>())
        {
            sendJson(400, "{\"error\":\"A numeric maxValidRpm is required\"}");
            return;
        }

        const double maxValidRpm = input["maxValidRpm"].as<double>();
        if (maxValidRpm < 1000.0 || maxValidRpm > 30000.0)
        {
            sendJson(400, "{\"error\":\"RPM limit must be between 1000 and 30000 rpm\"}");
            return;
        }

        Preferences preferences;
        if (!preferences.begin("racesync", false))
        {
            sendJson(500, "{\"error\":\"Unable to open settings storage\"}");
            return;
        }
        const bool saved = preferences.putDouble("rpmMaxValid", maxValidRpm) != 0;
        preferences.end();
        if (!saved)
        {
            sendJson(500, "{\"error\":\"Unable to save RPM limit\"}");
            return;
        }

        _telemetry.rpmMaxValid = maxValidRpm;
        JsonDocument doc;
        doc["saved"] = true;
        doc["maxValidRpm"] = _telemetry.rpmMaxValid;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/rpm-led", HTTP_POST, [this]()
    {
        if (_logger.recording())
        {
            sendJson(409, "{\"error\":\"Stop recording before changing the RPM LED setting\"}");
            return;
        }
        JsonDocument input;
        if (!_server.hasArg("plain") ||
            deserializeJson(input, _server.arg("plain")) ||
            !input["enabled"].is<bool>())
        {
            sendJson(400, "{\"error\":\"A JSON boolean enabled is required\"}");
            return;
        }

        const bool enabled = input["enabled"].as<bool>();
        if (enabled != _telemetry.rpmLedEnabled)
        {
            Preferences preferences;
            if (!preferences.begin("racesync", false))
            {
                sendJson(500, "{\"error\":\"Unable to open settings storage\"}");
                return;
            }
            const bool saved = preferences.putBool("rpmLedEnabled", enabled) != 0;
            preferences.end();
            if (!saved)
            {
                sendJson(500, "{\"error\":\"Unable to save RPM LED setting\"}");
                return;
            }
            _telemetry.rpmLedEnabled = enabled;
        }

        JsonDocument doc;
        doc["saved"] = true;
        doc["enabled"] = _telemetry.rpmLedEnabled;
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/throttle", HTTP_GET, [this]()
    {
        JsonDocument doc;
        doc["raw"] = _throttleSensor.raw();
        doc["filteredRaw"] = _throttleSensor.filteredRaw();
        doc["closedRaw"] = _throttleSensor.closedRaw();
        doc["openRaw"] = _throttleSensor.openRaw();
        doc["percent"] = _throttleSensor.percent();
        doc["calibrated"] = _throttleSensor.calibrated();
        doc["connected"] = _throttleSensor.connected();
        doc["inputPin"] = Pin::TPS_ADC;
        doc["recording"] = _logger.recording();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/settings/throttle/calibrate", HTTP_POST, [this]()
    {
        if (_logger.recording())
        {
            sendJson(409, "{\"error\":\"Stop recording before calibrating the throttle sensor\"}");
            return;
        }
        JsonDocument input;
        if (!_server.hasArg("plain") || deserializeJson(input, _server.arg("plain")) || !input["position"].is<const char*>())
        {
            sendJson(400, "{\"error\":\"position must be closed, open or clear\"}");
            return;
        }
        const String position = input["position"].as<String>();
        bool saved = false;
        if (position == "closed") saved = _throttleSensor.calibrateClosed();
        else if (position == "open") saved = _throttleSensor.calibrateOpen();
        else if (position == "clear") saved = _throttleSensor.clearCalibration();
        else
        {
            sendJson(400, "{\"error\":\"position must be closed, open or clear\"}");
            return;
        }
        if (!saved)
        {
            sendJson(400, position == "open"
                ? "{\"error\":\"Calibration span is too small; fully close then fully open the throttle\"}"
                : "{\"error\":\"Unable to save throttle calibration\"}");
            return;
        }
        JsonDocument doc;
        doc["saved"] = true;
        doc["position"] = position;
        doc["closedRaw"] = _throttleSensor.closedRaw();
        doc["openRaw"] = _throttleSensor.openRaw();
        doc["calibrated"] = _throttleSensor.calibrated();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });

    _server.on("/api/reboot", HTTP_POST, [this]()
    {
        if (_logger.recording())
        {
            sendJson(409, "{\"error\":\"Cannot reboot while a session is recording\"}");
            return;
        }

        sendJson(200, "{\"rebooting\":true}");
        delay(350);
        ESP.restart();
    });
}
