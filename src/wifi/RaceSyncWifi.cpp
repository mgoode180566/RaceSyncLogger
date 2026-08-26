#include "RaceSyncWifi.h"
#include "../config/RaceSyncConfig.h"

bool RaceSyncWifi::begin()
{
    WiFi.mode(WIFI_AP);

    bool ok =
        WiFi.softAP(
            RaceSyncConfig::WIFI_SSID,
            RaceSyncConfig::WIFI_PASSWORD
        );

    if (!ok)
    {
        Serial.println("[WiFi] AP failed");
        return false;
    }

    _startedMs = millis();

    Serial.print("[WiFi] SSID: ");
    Serial.println(RaceSyncConfig::WIFI_SSID);

    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.softAPIP());

    return true;
}

String RaceSyncWifi::ip() const
{
    return WiFi.softAPIP().toString();
}

uint8_t RaceSyncWifi::connectedClients() const
{
    return WiFi.softAPgetStationNum();
}

uint32_t RaceSyncWifi::uptimeSeconds() const
{
    return _startedMs == 0
        ? 0
        : (millis() - _startedMs) / 1000;
}
