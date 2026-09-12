#include "RaceSyncController.h"

#include "../config/RaceSyncConfig.h"
#include <esp_arduino_version.h>

RaceSyncController::RaceSyncController()
    : _sensors(_rpmSensor),
      _api(_storage, _logger, _gps, _wifi, _goPro, _telemetry, _mode, _bootCount)
{
}

void RaceSyncController::incrementBootCount()
{
    _preferences.begin("racesync", false);
    _bootCount = _preferences.getUInt("bootCount", 0) + 1;
    _preferences.putUInt("bootCount", _bootCount);
    _telemetry.rpmLedEnabled = _preferences.getBool("rpmLedEnabled", true);
}

void RaceSyncController::setStatusLed(uint8_t red, uint8_t green, uint8_t blue)
{
#if defined(RGB_BUILTIN)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(RGB_BUILTIN, red, green, blue);
#else
    neopixelWrite(RGB_BUILTIN, red, green, blue);
#endif
#elif defined(LED_BUILTIN)
    digitalWrite(LED_BUILTIN, (red || green || blue) ? HIGH : LOW);
#else
    (void)red;
    (void)green;
    (void)blue;
#endif
}

void RaceSyncController::setLoggingLed(bool on)
{
    setStatusLed(0, on ? 32 : 0, 0);
    _loggingLedOn = on;
}

void RaceSyncController::updateLoggingLed()
{
    // The RPM pulse indicator owns the RGB LED during its short blue flash.
    if (_rpmLedOn) return;

    if (!_logger.recording())
    {
        if (_loggingLedOn) setLoggingLed(false);
        _loggingLedCycleStartedMs = 0;
        return;
    }

    const uint32_t now = millis();

    if (_loggingLedCycleStartedMs == 0)
    {
        _loggingLedCycleStartedMs = now;
        setLoggingLed(true);
        return;
    }

    const uint32_t elapsed = now - _loggingLedCycleStartedMs;

    if (elapsed >= 1000)
    {
        _loggingLedCycleStartedMs = now;
        setLoggingLed(true);
    }
    else if (elapsed >= 100 && _loggingLedOn)
    {
        setLoggingLed(false);
    }
}

void RaceSyncController::updateRpmPulseLed()
{
    const uint32_t now = millis();
    const uint32_t pulseCount = _sensors.rpmPulseCount();

    if (!_telemetry.rpmLedEnabled)
    {
        // Consume pulses while disabled, without affecting capture or logging.
        _lastRpmLedPulseCount = pulseCount;
        if (_rpmLedOn)
        {
            _rpmLedOn = false;
            setStatusLed(0, 0, 0);
            _loggingLedOn = false;
            _loggingLedCycleStartedMs = 0;
        }
        return;
    }

    if (pulseCount != _lastRpmLedPulseCount)
    {
        _lastRpmLedPulseCount = pulseCount;
        _rpmLedUntilMs = now + RPM_LED_FLASH_MS;
        _rpmLedOn = true;
        setStatusLed(0, 0, 48);
        return;
    }

    if (_rpmLedOn && static_cast<int32_t>(now - _rpmLedUntilMs) >= 0)
    {
        _rpmLedOn = false;
        setStatusLed(0, 0, 0);
        _loggingLedOn = false;
        _loggingLedCycleStartedMs = 0;
    }
}

bool RaceSyncController::waitForGpsTraffic(uint32_t timeoutMs)
{
    const uint32_t started = millis();
    const uint64_t packetsAtStart = _gps.packetCount();

    while ((millis() - started) < timeoutMs)
    {
        _gps.update(_telemetry);

        if (_gps.packetCount() > packetsAtStart)
        {
            return true;
        }

        delay(2);
    }

    return false;
}

void RaceSyncController::flashDiagnosticResult(uint8_t code, bool passed)
{
    const uint8_t red = passed ? 0 : 48;
    const uint8_t green = passed ? 48 : 0;

    for (uint8_t i = 0; i < code; ++i)
    {
        setStatusLed(red, green, 0);
        delay(220);
        setStatusLed(0, 0, 0);
        delay(220);
    }

    delay(700);
}

uint8_t RaceSyncController::runStartupDiagnostics()
{
    uint8_t failures = 0;

    Serial.println();
    Serial.println("[DIAG] =================================");
    Serial.println("[DIAG] RaceSync startup diagnostics");
    Serial.println("[DIAG] =================================");

    setStatusLed(40, 0, 0);

    Serial.print("[DIAG] 1 Wi-Fi AP ............... ");
    const bool wifiOk = _wifi.begin();
    if (wifiOk) Serial.println("PASS");
    else { Serial.println("FAIL"); failures |= (1U << (DIAG_WIFI - 1)); }
    flashDiagnosticResult(DIAG_WIFI, wifiOk);

    Serial.print("[DIAG] 2 microSD/storage ........ ");
    const bool storageMounted = _storage.begin();
    const bool storageHealthy = storageMounted && _storage.runHealthCheck();
    const bool sdOk = storageHealthy && _storage.usingSd();
    if (sdOk) Serial.println("PASS");
    else
    {
        Serial.print("FAIL");
        if (storageHealthy && !_storage.usingSd()) Serial.print(" (SD unavailable; fallback storage active)");
        if (_storage.lastError().length()) { Serial.print(" - "); Serial.print(_storage.lastError()); }
        Serial.println();
        failures |= (1U << (DIAG_STORAGE - 1));
    }
    flashDiagnosticResult(DIAG_STORAGE, sdOk);

    Serial.print("[DIAG] 3 logger ................. ");
    const bool loggerOk = _logger.begin(_storage);
    if (loggerOk) Serial.println("PASS");
    else { Serial.println("FAIL"); failures |= (1U << (DIAG_LOGGER - 1)); }
    flashDiagnosticResult(DIAG_LOGGER, loggerOk);

    Serial.print("[DIAG] 4 GPS communications ..... ");
    const bool gpsStarted = _gps.begin();
    const bool gpsTraffic = gpsStarted && waitForGpsTraffic(5000);
    if (gpsTraffic)
    {
        Serial.print("PASS (");
        Serial.print((unsigned long)_gps.packetCount());
        Serial.println(" packet(s))");
    }
    else
    {
        Serial.println("FAIL (no valid GPS packets within 5 seconds)");
        failures |= (1U << (DIAG_GPS - 1));
    }
    flashDiagnosticResult(DIAG_GPS, gpsTraffic);

    Serial.print("[DIAG] 5 sensor subsystem ....... ");
    const bool sensorsOk = _sensors.begin();
    if (sensorsOk) Serial.println("PASS");
    else { Serial.println("FAIL"); failures |= (1U << (DIAG_SENSORS - 1)); }
    flashDiagnosticResult(DIAG_SENSORS, sensorsOk);

    Serial.println("[DIAG] =================================");
    return failures;
}

void RaceSyncController::showDiagnosticComplete()
{
    Serial.println("[DIAG] Startup diagnostic sequence complete");
    Serial.println("[DIAG] Signalling five blue flashes");

    for (uint8_t i = 0; i < 5; ++i)
    {
        setStatusLed(0, 0, 48);
        delay(180);
        setStatusLed(0, 0, 0);
        delay(180);
    }
}

void RaceSyncController::begin()
{
    Serial.println();
    Serial.println("=============================");
    Serial.println(" RaceSync V2.1 Modular");
    Serial.println("=============================");

#if defined(LED_BUILTIN) && !defined(RGB_BUILTIN)
    pinMode(LED_BUILTIN, OUTPUT);
#endif

    setStatusLed(40, 0, 0);
    incrementBootCount();

    const uint8_t diagnosticFailures = runStartupDiagnostics();
    if (diagnosticFailures == 0) Serial.println("[DIAG] ALL STARTUP DIAGNOSTICS PASSED");
    else
    {
        Serial.print("[DIAG] STARTUP DIAGNOSTICS COMPLETED WITH FAILURE MASK 0x");
        Serial.println(diagnosticFailures, HEX);
    }

    showDiagnosticComplete();

    _api.beginWebUiRoute();
    _api.beginKmlDownloadRoute();
    _api.beginManualLoggingRoutes();
    _api.beginSettingsRoutes();
    _api.beginCameraRoutes();
    _api.begin();
    Serial.println("[GOPRO] Bluetooth disabled; use /camera to connect manually");

    setStatusLed(0, 0, 0);
    _loggingLedOn = false;
    _loggingLedCycleStartedMs = 0;
    _lastRpmLedPulseCount = _sensors.rpmPulseCount();
    _rpmLedOn = false;

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

    // Feed continuous GPS health into the logger so a stale/no-fix state can never
    // be interpreted as proof that the motorcycle is stationary.
    _logger.observeGpsHealth(_gps.connected(), _telemetry.valid, _gps.lastPacketAgeMs());

    // Sample RPM continuously; the most recent value is attached to each GPS sample.
    _sensors.update(_telemetry);
    updateRpmPulseLed();

    if (newSample)
    {
        const bool wasRecording = _logger.recording();
        const bool wasManual = _logger.manualSession();

        _logger.processSample(_telemetry, _mode);

        const bool isRecording = _logger.recording();
        const bool isManual = _logger.manualSession();

        // Manual sessions are controlled by the UI/API route. This block only
        // follows automatic logger transitions, and always queues camera work
        // after the logger has started or finished its critical storage work.
        if (!wasRecording && isRecording && !isManual)
        {
            const GoProStatus camera = _goPro.status();
            if (camera.connected && camera.statusValid && camera.recording)
            {
                _logger.logSessionDiagnosticEvent("GOPRO_ALREADY_RECORDING_AUTO");
                Serial.println("[GOPRO] Auto logging started; GoPro already recording");
            }
            else
            {
                const bool queued = _goPro.queueVideoStart();
                _logger.logSessionDiagnosticEvent(queued ? "GOPRO_VIDEO_START_QUEUED_AUTO" : "GOPRO_VIDEO_START_NOT_QUEUED_AUTO");
                Serial.printf("[GOPRO] Auto logging started; video start %s\n", queued ? "queued" : "not queued");
            }
        }
        else if (wasRecording && !isRecording && !wasManual)
        {
            const bool queued = _goPro.queueVideoStop();
            Serial.printf("[GOPRO] Auto logging stopped; video stop %s\n", queued ? "queued" : "not queued");
        }
    }

    updateLoggingLed();
    _api.update();
    delay(1);
}
