#pragma once
#include <Arduino.h>
#include <WiFi.h>

class RaceSyncWifi
{
public:
    bool begin();

    String ip() const;
    uint8_t connectedClients() const;
    uint32_t uptimeSeconds() const;

private:
    uint32_t _startedMs = 0;
};
