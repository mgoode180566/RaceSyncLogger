#include "RaceSyncController.h"

#include "../config/RaceSyncConfig.h"

RaceSyncController::RaceSyncController()
    : _api(_storage, _logger, _gps, _wifi, _telemetry, _mode, _bootCount)
{
}

void RaceSyncController::incrementBootCount()
{
    _preferences.begin("racesync", false);
    _bootCount = _preferences.getUInt("bootCount", 0) + 1;
    _preferences.putUInt("bootCount", _bootCount);
}

void RaceSyncController::setLoggingLed(bool on)
{
#if defined(RGB_BUILTIN)
    rgbLedWrite(RGB_BUILTIN, 0, on ? 32 : 0, 0);
#elif defined(LED_BUILTIN)
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
#else
    (void)on;
#endif
    _loggingLedOn = on;
}

void RaceSyncController::updateLoggingLed()
{
    if (!_logger.recording()) { if (_loggingLedOn) setLoggingLed(false); _loggingLedCycleStartedMs = 0; return; }
    const uint32_t now = millis();
    if (_loggingLedCycleStartedMs == 0) { _loggingLedCycleStartedMs = now; setLoggingLed(true); return; }
    const uint32_t elapsed = now - _loggingLedCycleStartedMs;
    if (elapsed >= 1000) { _loggingLedCycleStartedMs = now; setLoggingLed(true); }
    else if (elapsed >= 100 && _loggingLedOn) setLoggingLed(false);
}

void RaceSyncController::begin()
{
    Serial.println(); Serial.println("============================="); Serial.println(" RaceSync V2.1 Modular"); Serial.println("=============================");
#if defined(LED_BUILTIN) && !defined(RGB_BUILTIN)
    pinMode(LED_BUILTIN, OUTPUT);
#endif
    setLoggingLed(false);
    incrementBootCount();
    _wifi.begin();
    _storage.begin();
    _logger.begin(_storage);
    _gps.begin();
    _sensors.begin();

    _api.beginWebUiRoute();
    _api.beginKmlDownloadRoute();
    _api.begin();
    Serial.println("[MODE] Waiting for GPS...");
}

void RaceSyncController::updateDataMode()
{
    _mode = _gps.connected() ? DataMode::LIVE : DataMode::STARTING;
}

void RaceSyncController::update()
{
    _gps.update(_telemetry);
    updateDataMode();

    static uint32_t lastLiveIndex = 0;
    bool newSample = false;
    if (_mode == DataMode::LIVE && _telemetry.sampleIndex != lastLiveIndex)
    {
        lastLiveIndex = _telemetry.sampleIndex;
        newSample = true;
    }

    _sensors.update();
    if (newSample) _logger.processSample(_telemetry, _mode);
    updateLoggingLed();
    _api.update();
    delay(1);
}
