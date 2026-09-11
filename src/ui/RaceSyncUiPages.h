#pragma once
#include <Arduino.h>

// Embedded HTML pages live in separate translation units so the API routing
// code stays small and each page can be maintained independently. All page
// data remains in flash via PROGMEM; this refactor adds no runtime allocation.
extern const char RACESYNC_SESSIONS_UI[] PROGMEM;
extern const char RACESYNC_STATUS_UI[] PROGMEM;
extern const char RACESYNC_CONTROL_UI[] PROGMEM;
extern const char RACESYNC_CAMERA_UI[] PROGMEM;
