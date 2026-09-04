#include "RaceSyncApi.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_system.h>

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
