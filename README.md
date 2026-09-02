# RaceSync Motorcycle Data Logger

RaceSync is a standalone ESP32-S3 motorcycle data logger designed for track and race use. It records high-rate GPS data automatically, stores sessions on microSD, creates VBO and KML files, and provides its own Wi-Fi web interface for transferring and managing sessions in the paddock.

The design goal is simple: **power it on, ride, then download the data when you return to the paddock.** No phone, display or rider interaction is required while on track.

For rider instructions, see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Current functionality

- ESP32-S3 standalone logger
- MicoAir MG-902 / u-blox GPS at 25 Hz
- Engine RPM capture from the isolated ECU tachometer output
- Automatic start/stop recording based on motorcycle speed
- microSD session storage with LittleFS fallback
- Matching VBO and KML files for each RaceSync session
- VBO output for motorsport analysis such as Circuit Tools
- KML output for GPS route viewing
- Onboard LED flashes while actively recording
- Self-contained RaceSync Wi-Fi access point
- Onboard web session manager at `192.168.4.1`
- Stored-session list
- Browser-based NEW/downloaded session indication
- Download All New VBO sessions
- Individual VBO and KML downloads
- Session deletion, including the companion KML
- REST status, telemetry and session API
- Demo mode for bench testing

## Normal workflow

1. Power RaceSync and allow the GPS to obtain a fix.
2. Ride onto the circuit. RaceSync starts automatically when the configured start speed is exceeded.
3. The onboard LED flashes approximately once per second while recording.
4. Complete the session normally; no interaction with RaceSync is required.
5. Return to the paddock and allow RaceSync to stop and close the recording automatically.
6. Connect a phone, tablet or laptop to the `RaceSync` Wi-Fi network.
7. Browse to `http://192.168.4.1`.
8. Download new sessions, individual VBO/KML files, or delete sessions no longer required.
9. Open the VBO in compatible motorsport analysis software such as Circuit Tools.

Current thresholds:

```text
Start recording: >= 10 km/h
Stop condition:  <= 3 km/h
Stop delay:      60 seconds
```

## Onboard web interface

The ESP32 serves the RaceSync session manager itself. No separate application, internet connection or circuit Wi-Fi is required.

The interface focuses on the paddock workflow: device/GPS/storage/logger status and the sessions stored on the logger. Sessions can be downloaded as VBO or KML files and deleted when no longer required.

The browser keeps a local record of downloaded session IDs. Sessions not previously downloaded by that browser are shown as **NEW**, and **Download All New** transfers the latest VBO recordings. The NEW state is browser-local, so another phone or laptop has its own download history.

## Wi-Fi

```text
SSID:     RaceSync
Password: racesync
IP:       192.168.4.1
```

User interface:

```text
http://192.168.4.1/
```

## GPS

Current hardware uses a MicoAir MG-902 / u-blox receiver configured for high-rate UBX NAV-PVT operation.

```text
MG-902 TX -> ESP32 GPIO16 (RX)
MG-902 RX -> ESP32 GPIO17 (TX)
```

The logging target is **25 Hz**, approximately one GPS sample every 40 ms.

## Engine RPM

RaceSync captures the CB500 ECU tachometer output on ESP32 GPIO4 and writes the calculated value to the VBO `Revs` channel. The live value is also available as `channels.Revs` from `GET /api/telemetry`.

The motorcycle's nominal 12 V tachometer wire must never be connected directly to the ESP32. Use the 12 V optocoupler isolation module, configured for a 3.3 V logic output:

```text
CB500 ECU tach output -> optocoupler 12 V input
Optocoupler OUT       -> ESP32 GPIO4
Optocoupler logic VCC -> ESP32 3V3
Optocoupler logic GND -> ESP32 GND
```

The default calibration is one falling edge per crankshaft revolution. Confirm this against a known tachometer reading before track use. If the logged RPM is exactly half or double the displayed RPM, change `RPM_PULSES_PER_REVOLUTION` in `src/sensors/RaceSyncSensors.h`.

The capture rejects pulses closer than 1.5 ms, reports zero after 500 ms without a valid pulse, and applies light smoothing to reduce optocoupler jitter.

## Storage

Primary session storage is **microSD**. RaceSync attempts to mount SD at startup and can fall back to internal LittleFS storage when necessary.

```text
SD CS   -> GPIO10
SD MOSI -> GPIO11
SD SCK  -> GPIO12
SD MISO -> GPIO13
```

FAT32 is recommended.

RaceSync sessions normally contain a matching pair:

```text
RS_20260829_103215.vbo
RS_20260829_103215.kml
```

The pair is treated as one logical session. Deleting the session removes the VBO and companion KML when present.

## VBO and KML

The VBO is the primary motorsport data file and contains GPS position, speed, heading, altitude, timing and other available channels in a VBOX-compatible text format. RaceSync remains circuit-independent; analysis software performs track recognition, lap timing and detailed analysis afterwards.

A KML GPS track is generated alongside the VBO for quick route viewing in compatible mapping software. KML generation is secondary to the VBO so a KML problem should not prevent primary logging.

## REST API

```text
GET    /api/status
GET    /api/location
GET    /api/telemetry
GET    /api/sessions
GET    /api/sessions/{id}
GET    /api/session-kml?id={id}
DELETE /api/sessions/{id}
```

`GET /api/sessions` enumerates stored VBO sessions using stable numeric session IDs and supplies VBO/KML URLs where available. Deletion is refused for an active recording or protected Demo source.

## Operating modes

- **STARTING** — initial startup while the GPS begins communicating.
- **LIVE** — telemetry comes from the physical GPS.
- **DEMO** — a stored VBO can be replayed through the normal logger path for bench testing.

## Firmware structure

```text
src/
├── api/        HTTP API and onboard web interface
├── app/        application controller
├── config/     configuration and telemetry types
├── gps/        MG-902/u-blox interface and parser
├── logging/    VBO/KML logger and SD/LittleFS storage
├── sensors/    extension point for RPM, IMU and other sensors
├── sim/        VBO replay / Demo mode
├── wifi/       RaceSync access point
└── main.cpp
```

The logging path is the primary function. Wi-Fi, web UI, diagnostics and Demo mode are supporting functions and should not interfere with reliable on-bike recording.

## Development

RaceSync uses PlatformIO and the Arduino framework for ESP32-S3.

```text
platformio run
platformio run --target upload
platformio device monitor
```

## Planned expansion

The architecture supports future high-rate IMU, throttle position, brake pressure, lean/pitch/acceleration channels and camera/video synchronisation without changing the basic rider workflow.

The core principle remains: **automatically collect useful motorcycle data on track and make it easy to retrieve in the paddock.**
