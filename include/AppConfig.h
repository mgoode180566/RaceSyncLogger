#pragma once
#include <cstdint>

struct AppConfig {
  float startSpeedKph = 10.0f;
  float stopSpeedKph = 3.0f;
  uint32_t stopDelaySeconds = 30;
  uint32_t imuRateHz = 100;
  uint32_t sdFlushMs = 250;
  uint32_t wifiTimeoutSeconds = 300;

  char ssid[33] = "RaceSync";
  char password[65] = "racesync123";
};