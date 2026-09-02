# RaceSync Motorcycle Data Logger

RaceSync is a standalone ESP32-S3 motorcycle data logger for Honda CB500 track and race use. It records 25 Hz GPS and engine RPM data to microSD, creates VBOX-compatible VBO files, and provides its own Wi-Fi web interface for configuration and session management.

The operating goal is simple: **power it on, ride, then download the data in the paddock.** No phone, display or rider interaction is required while on track.

For rider-focused instructions, see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

## Current functionality

- ESP32-S3 DevKitC-1 firmware built with PlatformIO and Arduino
- MicoAir MG-902 / u-blox GPS at a target 25 Hz
- Isolated ECU tachometer input with interrupt-based RPM capture
- RPM stored in the VBO `Revs` channel
- Automatic recording controlled by configurable start speed and stop delay
- Persistent logging settings retained after reboot
- Manual start and stop controls for bench testing
- Safe software reboot control, disabled while recording
- Startup self-tests with colour-coded LED results
- Active-recording LED flash approximately once per second
- Primary microSD storage with health checks and LittleFS fallback
- Matching VBO and KML files for each session
- VBO output for analysis in software such as Circuit Tools
- Self-contained Wi-Fi access point and onboard web interface
- Session list, NEW/downloaded state, bulk download and deletion
- Detailed REST status, telemetry, settings and session APIs
- Persistent boot count and reset-reason diagnostics

## Hardware connections

### GPS

```text
MG-902 TX -> ESP32 GPIO16 (GPS RX)
MG-902 RX -> ESP32 GPIO17 (GPS TX)
```

The logging target is 25 Hz, approximately one GPS sample every 40 ms.

### MicroSD

```text
SD CS   -> GPIO10
SD MOSI -> GPIO11
SD SCK  -> GPIO12
SD MISO -> GPIO13
```

Use a FAT32-formatted microSD card.

### ECU tachometer RPM

```text
CB500 ECU tach output -> 12 V optocoupler input
Optocoupler OUT       -> ESP32 GPIO4
Optocoupler logic VCC -> ESP32 3V3
Optocoupler logic GND -> ESP32 GND
```

The motorcycle's tachometer signal must **never** be connected directly to the ESP32. RaceSync expects a clean, isolated 3.3 V logic signal from the output side of the optocoupler module.

RPM capture uses a falling-edge interrupt, rejects edges closer than 1.5 ms, returns zero after 500 ms without a valid pulse, and applies light smoothing to reduce single-period jitter. This avoids blocking the GPS and logging path.

The default calibration is one falling edge per crankshaft revolution:

```cpp
RPM_PULSES_PER_REVOLUTION = 1.0f
```

Confirm the result against the motorcycle tachometer before track use. If RaceSync reports exactly half or double the displayed RPM, adjust `RPM_PULSES_PER_REVOLUTION` in `src/sensors/RaceSyncSensors.h`.

## Startup diagnostics and LED sequence

RaceSync runs five startup tests:

| Flash count | Test |
|---:|---|
| 1 | Wi-Fi access point |
| 2 | microSD/storage |
| 3 | logger |
| 4 | GPS communications |
| 5 | sensor subsystem, including RPM input setup |

For each test:

- Green flashes indicate a pass.
- Red flashes indicate a failure.
- The number of flashes identifies the test.
- Five blue flashes indicate that the complete diagnostic sequence has finished.

The serial monitor also prints the result of each test and the final failure mask. A missing GPS signal or SD problem is therefore visible before the motorcycle goes on track.

During normal operation, the onboard LED is off while idle and gives a short green flash approximately once per second while a session is actively recording. Wait for recording to stop and the flashing to end before removing power whenever possible.

## Automatic logging

Default settings:

```text
Start recording: 10 km/h
Stop threshold:   3 km/h
Stop delay:       60 seconds
```

The start speed and stop delay can be changed from the web interface. The permitted ranges are:

| Setting | Range |
|---|---:|
| Start speed | 1–100 km/h |
| Stop delay | 1–600 seconds |
| Stop threshold | Fixed at 3 km/h |

Settings are stored in ESP32 Preferences and survive a reboot. They cannot be changed while a session is recording.

Automatic operation:

1. RaceSync waits for valid live GPS data.
2. Recording starts once speed reaches the configured start speed.
3. GPS and the most recent RPM value are written to the VBO at each GPS sample.
4. When speed remains at or below 3 km/h for the configured delay, RaceSync flushes and closes the VBO and KML files.
5. The session becomes available through the onboard web interface.

## Web interface

Connect to the RaceSync access point:

```text
SSID:     RaceSync
Password: racesync
IP:       192.168.4.1
```

Available pages:

| Address | Purpose |
|---|---|
| `http://192.168.4.1/` | Stored sessions and downloads |
| `http://192.168.4.1/control` | Manual logging, automatic settings and reboot |
| `http://192.168.4.1/status` | Full device status |

The control page provides:

- Manual start, requiring valid GPS and ready storage
- Manual stop
- Configurable automatic start speed
- Configurable stationary stop delay
- ESP32 software reboot without disconnecting motorcycle power

A manually started session continues until **Stop Logging** is pressed. Reboot and settings changes are blocked while recording so an active session cannot be interrupted accidentally.

The session page lists newest sessions first. Each browser records which session IDs it has downloaded, marks unseen recordings as **NEW**, and can download all new VBO files. Individual VBO/KML downloads and session deletion are also available.

## REST API

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/status` | System, board, GPS, storage, logger and health diagnostics |
| GET | `/api/location` | Current GPS location |
| GET | `/api/telemetry` | Current GPS and sensor channels, including `channels.Revs` |
| GET | `/api/sessions` | Stored session list |
| GET | `/api/sessions/{id}` | Download a VBO session |
| GET | `/api/session-kml?id={id}` | Download the matching KML |
| DELETE | `/api/sessions/{id}` | Delete a stored session and companion KML |
| POST | `/api/logging/start` | Start a manual session |
| POST | `/api/logging/stop` | Stop the active session |
| GET | `/api/settings/logging` | Read automatic logging settings |
| POST | `/api/settings/logging` | Save automatic logging settings |
| POST | `/api/reboot` | Restart the ESP32 while idle |

Example logging-settings request:

```json
{
  "startSpeedKmh": 10.0,
  "stopDelaySeconds": 60
}
```

The status response includes uptime, persistent boot count, reset reason, ESP32/PSRAM details, Wi-Fi state, GPS fix and packet diagnostics, storage self-test results, free space, active filename, sample count and logging thresholds.

## VBO and KML output

The VBO is the primary motorsport data file. It contains GPS position, speed, heading, altitude, sample timing and the available sensor channels. Engine speed is written to the existing `Revs` channel for compatibility with motorsport analysis tools.

RaceSync remains circuit-independent. Track recognition, start/finish detection, lap timing and analysis are performed afterwards in software such as Circuit Tools.

A matching KML track is created for quick route viewing. KML is secondary to VBO logging; a KML failure should not prevent the primary recording.

Typical session pair:

```text
RS_2026-09-02_10-32-15.vbo
RS_2026-09-02_10-32-15.kml
```

The files are treated as one logical session. Deleting the session also deletes its companion KML when present.

## Storage reliability

RaceSync attempts to mount microSD at startup and runs a write, read-back and delete self-test. Detailed results are exposed through `/api/status`.

Logging will not start unless storage is ready and writable with sufficient free space. The logger tracks write errors, periodically flushes data, refuses deletion of an active recording and closes both session files when recording stops.

LittleFS can act as fallback storage, but microSD is the intended storage for normal track use.

## Firmware structure

```text
src/
├── api/        REST API and onboard web interface
├── app/        startup diagnostics and application controller
├── config/     configuration and telemetry types
├── gps/        MG-902/u-blox interface and parser
├── logging/    VBO/KML logger and SD/LittleFS storage
├── sensors/    ECU RPM capture and future sensors
├── wifi/       RaceSync access point
└── main.cpp
```

The GPS/logger path is the primary function. Web UI, Wi-Fi and diagnostics are supporting functions and must not interfere with reliable recording.

## Building and uploading

```text
platformio run
platformio run --target upload
platformio device monitor
```

The current PlatformIO configuration targets `esp32-s3-devkitc-1` and uses COM4 for upload and monitoring. Change `upload_port` and `monitor_port` in `platformio.ini` if Windows assigns another port.

## RPM bench-test checklist

Before connecting to the motorcycle:

1. Confirm the optocoupler logic output is 3.3 V, not 12 V.
2. Confirm GPIO4 is not connected directly to the ECU.
3. Build and upload the RPM feature firmware.
4. Open `/api/telemetry` and confirm `channels.Revs` is zero without pulses.
5. Apply a safe simulated pulse signal through the isolated interface.
6. Confirm RPM rises, remains stable and returns to zero within approximately 500 ms after pulses stop.
7. Compare the logged RPM with the motorcycle tachometer at idle and at several steady engine speeds.
8. Open the resulting VBO and confirm the `Revs` channel contains plausible values.

Do not merge or use the RPM feature on track until the pulse-per-revolution calibration has been confirmed.

## Planned expansion

The architecture supports future high-rate IMU, throttle position, brake pressure, lean/pitch/acceleration channels, configurable shift-light output and camera/video synchronisation without changing the basic rider workflow.

The core principle remains: **automatically collect useful motorcycle data on track and make it easy to retrieve in the paddock.**
