# RaceSync Motorcycle Data Logger

RaceSync is a standalone ESP32-S3 motorcycle data logger for Honda CB500 track and race use. Firmware V2.1 records 25 Hz GPS and engine RPM to microSD, produces VBOX-compatible VBO sessions, and provides an onboard Wi-Fi interface for paddock configuration and file access.

The design priority is simple: **protect the race recording first; web-interface convenience is secondary while the motorcycle is on track.**

For rider instructions, see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

This document describes the `docs/gopro-auto-session-pairing` branch.

## Current functionality

- MicoAir MG-902/u-blox GPS configured for 25 Hz logging
- Engine RPM capture from an isolated ECU tachometer signal on GPIO4
- VBOX-compatible `.vbo` output with RPM in the `Revs` channel
- Automatic recording with configurable start speed and stop delay
- Manual start/stop from the web interface
- FAT32 microSD storage with startup write/read/delete health test
- Active recordings written as `.part` and finalized to `.vbo` after a clean stop
- Automatic recovery of complete VBO rows from interrupted `.part` recordings
- Event-driven `.log` diagnostic file for every recording
- GPS-dropout protection: stale/invalid GPS is treated as unknown movement, not stationary
- Recording-priority web/API behaviour that suppresses unnecessary SD access while recording
- Live RPM diagnostics and saved RPM blue-LED preference
- KML generation on demand only
- GoPro HERO9 BLE pairing, saved-camera reconnection, GPS clock sync, and automatic video start/stop with logger sessions
- Five-part startup diagnostics

## Proven race use

RaceSync has been used through qualifying and multiple CB500 races. VBO files imported successfully into Circuit Tools and recorded lap times agreed with official timing to within 0.01 s on the tested event. The reliability work on this branch follows investigation of one race in which recording ended early despite the ESP32 remaining powered and returning to `IDLE`.

## Normal workflow

1. Power RaceSync and allow startup diagnostics to finish.
2. Give the GPS antenna a clear view of the sky and obtain a valid fix.
3. Before first use, pair the GoPro from the RaceSync Camera page. Later boots reconnect to the saved camera automatically while RaceSync is idle.
4. Confirm the Camera page reports **GoPro connected — not recording** before going out if video is required.
5. Ride away. Automatic logging begins when valid GPS speed reaches the configured start speed (10 km/h by default), and RaceSync requests GoPro video start for the same session.
6. While logging, green flashes mean a data-only session; blue flashes mean RaceSync initiated the session with a connected camera.
7. Back in the paddock, remain at or below 3 km/h for the configured stop delay (60 seconds by default). RaceSync finalizes the VBO and requests GoPro video stop.
8. Confirm the logger has returned to `IDLE` before removing power whenever possible.
9. Connect to the `RaceSync` Wi-Fi network at `http://192.168.4.1` and download the VBO.

## Hardware connections

### MG-902 GPS

```text
MG-902 TX -> ESP32 GPIO16 (GPS RX)
MG-902 RX -> ESP32 GPIO17 (GPS TX)
```

The receiver is started at 9600 baud, switched to 115200 baud, and configured for the high-rate UBX stream used for 25 Hz logging.

### MicroSD

```text
SD VCC  -> 5 V
SD GND  -> ESP32 GND
SD CS   -> GPIO10
SD MOSI -> GPIO11
SD SCK  -> GPIO12
SD MISO -> GPIO13
```

Use a FAT32 card. The tested SD module is powered from 5 V; ESP32 GPIO remains 3.3 V only. SPI runs conservatively at 4 MHz.

### ECU tachometer RPM

```text
CB500 ECU tach output -> 12 V optocoupler input
Optocoupler OUT       -> ESP32 GPIO4
Optocoupler logic VCC -> ESP32 3V3
Optocoupler logic GND -> ESP32 GND
```

Never connect the motorcycle tachometer output directly to the ESP32.

RPM uses a falling-edge interrupt, rejects edges closer than 1.5 ms, and returns zero after 500 ms without an accepted pulse. Default calibration is:

```cpp
RPM_PULSES_PER_REVOLUTION = 2.0f
```

Verify against the motorcycle tachometer. If indicated RPM is exactly half or double, adjust `RPM_PULSES_PER_REVOLUTION` in `src/sensors/RaceSyncSensors.h` and rebuild.

## Automatic stop and GPS-dropout protection

Defaults:

```text
Start recording: 10 km/h
Stop threshold:   3 km/h
Stop delay:       60 seconds
GPS stale limit:  1000 ms
```

The start speed can be configured from 1-100 km/h and the stop delay from 1-600 seconds. The stop threshold is fixed at 3 km/h.

A disconnected GPS, invalid fix, or GPS packet age over 1000 ms is treated as **unknown movement state**. It does not count as stationary. Any active stationary timer is cleared and must begin again after healthy GPS data returns. This prevents a GPS dropout from directly satisfying the automatic-stop delay.

Significant GPS transitions are also recorded in the session diagnostic log.

## Recording-priority mode

While a VBO is being recorded, RaceSync deliberately minimizes SD work caused by the web interface.

`GET /api/status` remains available, but returns a lightweight RAM-only recording status. It does not calculate SD capacity or enumerate sessions. During this state:

```json
"racePriorityMode": true
```

and storage reports:

```json
"ioSuppressedForWeb": true
```

The recording status includes the live GPS state, storage-ready flags already held in memory, current file, sample count, recording duration, write-error count and last-write age.

Session listing, VBO download and session deletion are suspended while recording and return HTTP 423. These operations become available again after the logger returns to `IDLE`. Do not use the web interface for file management while on track.

This mode is intended to keep the SD path focused on sequential VBO writes and low-frequency diagnostic events.

## Per-session diagnostic log

Every recording creates a matching text log:

```text
RS_2026-09-07_10-32-15.vbo
RS_2026-09-07_10-32-15.log
```

The log is event-driven rather than sampled at 25 Hz. Events can include:

```text
event=SESSION_START
event=GPS_STALE
event=GPS_RECOVERED
event=STATIONARY_CANDIDATE
event=MOVEMENT_RESUMED
event=SESSION_STOP_REQUESTED
```

Events include available UTC time, uptime, speed, GPS-fix state, satellite count, solution type and GPS packet age.

A normal final summary records start/end time, stop reason, duration, samples written, final VBO size, stop speed, GPS state at stop, GPS-dropout count, maximum GPS packet age, invalid-GPS sample count and SD/write-error information.

Typical stop reasons include:

- `STATIONARY_TIMEOUT`
- `MANUAL`
- `LOW_STORAGE`
- `SD_WRITE_ERROR`
- `FORCED`

The last-session summary is available in RAM through `/api/status` after returning to `IDLE`, but resets on reboot. The `.log` file is the persistent diagnostic record.

Failure to open the diagnostic log must not prevent the primary VBO recording from continuing; the race data remains the priority.

## Session integrity and power-loss recovery

During recording:

```text
Primary data: RS_YYYY-MM-DD_HH-MM-SS.part
Diagnostics:  RS_YYYY-MM-DD_HH-MM-SS.log
```

After a clean stop the `.part` file is flushed, closed and renamed to `.vbo`. The active VBO is flushed approximately once per second. Diagnostic events are flushed when written.

Logging will not start with less than 1 MB free and will stop if free storage falls below the configured reserve.

After unexpected power loss, the next boot scans `.part` files, retains newline-terminated VBO records, discards a potentially torn final row, validates the required VBO sections and publishes a recovered `.vbo`. If necessary a `_RECOVERED_n.vbo` filename is used. Invalid/empty interrupted files remain as `.part` for investigation.

Recovery can preserve complete records that reached the card; it cannot recreate samples that were never written.

## Web interface

### Pairing a GoPro HERO9

RaceSync controls the camera over Bluetooth Low Energy using Open GoPro. HERO9
firmware 1.70 or newer is required; firmware 1.72 is supported.

Pair the camera while RaceSync is idle:

1. On the GoPro, enable wireless connections.
2. Open the GoPro connection/pairing screen (the menu may describe this as
   **Connect Device** or pairing with the GoPro app) and leave it advertising.
3. Join the `RaceSync` Wi-Fi network and open
   `http://192.168.4.1/camera`.
4. Select **Enable Bluetooth & Connect** once and wait for the page to report
   **GoPro connected — not recording**.
5. Check battery, SD-card state, remaining video time, and time-sync status.
   Use **Refresh status** if required.

RaceSync selects the advertising camera named `GoPro XXXX` and saves its BLE
address in ESP32 NVS. No PIN or Wi-Fi connection to the GoPro is required.
Keep only the intended GoPro in pairing mode during the first scan.

On later boots RaceSync waits 15 seconds, then connects directly to the saved
camera without scanning. While connected and idle, RaceSync sends the official
BLE Keep Alive value every three seconds so an externally powered camera remains
awake and ready for the next session. Keep Alive pauses during recording because
the active capture already keeps the camera awake. Failed connection attempts
retry every 60 seconds only while
RaceSync is idle and stationary. If the ESP32 resets during its first automatic
Bluetooth attempt, automatic connection is suppressed for the next boot to
prevent a boot loop. Selecting **Enable Bluetooth & Connect** successfully once
restores saved-camera automatic connection.

When valid GPS date/time is available at connection, RaceSync sets the GoPro
clock to UK local time and applies GMT/BST automatically. Pairing still succeeds
if GPS time is unavailable; the Camera page reports that clock synchronisation
was skipped.

### Automatic sessions with GoPro video

Both automatic and manual logger starts use the same camera behaviour:

- RaceSync opens the VBO first, so camera failure cannot prevent data logging.
- If the saved GoPro is connected, RaceSync queues shutter-on in a low-priority
  task. A camera already recording is accepted without sending another command.
- While logging, the status LED flashes blue when that session was initiated
  with a connected camera. It retains the normal green flash for a data-only
  session.
- On logger stop, RaceSync safely finalizes the VBO before queuing shutter-off.
- Camera commands never block the 25 Hz logging path.
- RaceSync does not send the Sleep command; an externally powered GoPro with its
  battery installed remains available between sessions.
- Camera status exposes Keep Alive count, age and error diagnostics.

Each VBO row writes `avifileindex` as `0000`. The `avisynctime` value uses
elapsed GPS milliseconds with a 500 ms GoPro calibration subtracted, based on a
30 ft observed displacement at 40 mph. Values in the initial 500 ms are clamped
to zero; later values follow actual GPS timing, including delayed or missing
packet intervals. This advances the GPS position in Circuit Tools relative to
the video without delaying or otherwise changing data capture. It does not place
the GoPro's MP4 filename in the VBO.

```text
SSID:     RaceSync
Password: racesync
IP:       192.168.4.1
```

| Address | Purpose |
|---|---|
| `/` | Completed sessions and paddock file actions |
| `/control` | Manual logging, automatic settings and reboot |
| `/status` | Device, GPS, RPM, storage and logger diagnostics |
| `/camera` | Manual GoPro Bluetooth connection and status |

The Sessions page marks recordings as **NEW** using browser-local download history. KML is generated only when requested and is not continuously stored during recording.

## REST API

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/status` | Full idle diagnostics; lightweight RAM-only status while recording |
| GET | `/api/location` | Current GPS location |
| GET | `/api/telemetry` | Current GPS/sensor telemetry including RPM |
| GET | `/api/sessions` | Completed sessions; suspended while recording |
| GET | `/api/sessions/{id}` | Download VBO; suspended while recording |
| DELETE | `/api/sessions/{id}` | Delete completed session; suspended while recording |
| GET | `/api/session-kml?id={id}` | Generate KML from a completed VBO |
| POST | `/api/logging/start` | Start manual logging |
| POST | `/api/logging/stop` | Stop/finalize active logging |
| GET | `/api/settings/logging` | Read automatic logging settings |
| POST | `/api/settings/logging` | Save automatic logging settings |
| POST | `/api/settings/rpm-led` | Save RPM blue-LED preference while idle |
| POST | `/api/reboot` | Restart ESP32 while idle |
| GET | `/api/camera` | Read cached manual camera state |
| POST | `/api/camera/connect` | Enable BLE, scan once and connect while idle |
| POST | `/api/camera/refresh` | Request camera status while idle |
| POST | `/api/camera/disconnect` | Disconnect the camera while idle |

## RPM diagnostics

The Status page exposes current RPM, signal-present state, accepted pulse count, rejected over-range readings, last-pulse age and GPIO4 input level. These counters are live debugging aids and reset on reboot; they are not VBO channels.

The RPM blue activity LED can be disabled while idle without disabling RPM capture. Its preference survives reboot. Disabling it makes the green recording indication easier to see.

## Sensors not yet implemented

GPIO1 is reserved for a future throttle-position input and GPIO8/9 for I2C. Throttle position, IMU and brake-pressure capture are not currently implemented. Placeholder VBO columns must not be interpreted as live sensors.

## VBO output

VBO is the primary motorsport data format. It contains GPS position, speed, heading, altitude, timing, solution information and available sensor channels. RPM is written to `Revs`.

RaceSync remains circuit-independent. Circuit recognition, start/finish detection and lap analysis are performed afterwards in software such as Circuit Tools.

## Firmware structure

```text
src/
├── api/        REST API, KML and onboard web interface
├── app/        startup diagnostics and controller
├── config/     configuration and telemetry types
├── gps/        MG-902/u-blox interface and parser
├── logging/    VBO logger, recovery and SD storage
├── sensors/    ECU RPM capture and future sensors
├── wifi/       RaceSync access point
└── main.cpp
```

## Building and uploading

```text
platformio run
platformio run --target upload
platformio device monitor
```
