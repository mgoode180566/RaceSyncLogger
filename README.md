# RaceSync Motorcycle Data Logger

RaceSync is a standalone ESP32-S3 motorcycle data logger for Honda CB500 track and race use. Firmware V2.1 records 25 Hz GPS, engine RPM and calibrated throttle position to microSD, produces VBOX-compatible VBO sessions, and provides an onboard Wi-Fi interface for paddock configuration and file access.

The design priority is simple: **protect the race recording first; web-interface convenience is secondary while the motorcycle is on track.**

For rider instructions, see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

This branch is the dedicated **board-development branch for the Seeed Studio XIAO ESP32-S3 Plus**. It is intended to bring up and validate the smaller RaceSync hardware while keeping the production ESP32-S3 DevKitC-1 implementation on `main` unchanged. The logger algorithms should remain functionally equivalent unless a board-specific change is required.

> **Development status:** the XIAO ESP32-S3 Plus hardware has been ordered and the pin allocation below is the planned initial bench configuration. It must be verified on the physical board before motorcycle use.

## Current functionality

- MicoAir MG-902/u-blox GPS configured for 25 Hz logging
- Engine RPM capture from an isolated ECU tachometer signal on GPIO4
- Throttle-position capture from a 3.3 V potentiometric sensor on GPIO1, with saved two-point calibration
- VBOX-compatible `.vbo` output with filtered RPM in `Revs`/`rc_rpm` and calibrated `throttle`
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

## XIAO ESP32-S3 Plus board development

The XIAO ESP32-S3 Plus keeps RaceSync on the ESP32-S3 family while reducing the controller footprint. Power the MG-902 from the development board's configured 5 V GPS output. Its UART signals use 3.3 V logic. The compatible microSD interface and throttle test circuit use 3.3 V. The existing isolated motorcycle-side RPM interface remains mandatory.

Planned initial pin allocation:

| Function | XIAO pin | ESP32-S3 GPIO |
|---|---|---:|
| Throttle ADC | D0 | GPIO1 |
| SD CS | D2 | GPIO3 |
| RPM input | D3 | GPIO4 |
| I2C SDA / spare | D4 | GPIO5 |
| I2C SCL / spare | D5 | GPIO6 |
| GPS TX | D6 | GPIO43 |
| GPS RX | D7 | GPIO44 |
| SD SCK | D8 | GPIO7 |
| SD MISO | D9 | GPIO8 |
| SD MOSI | D10 | GPIO9 |

The first acceptance sequence is: board/USB and firmware upload, Wi-Fi AP, GPS UART and 25 Hz data, SD initialization and sustained write test, RPM input, throttle ADC/calibration, then a full logger regression test. Do not install this board on the motorcycle until those tests pass.

## Hardware connections

See the [complete wiring schematic](docs/XIAO_WIRING_SCHEMATIC.md) for the power, GPS, microSD and isolated RPM connections.

### MG-902 GPS

```text
MG-902 TX -> XIAO D7 / GPIO44 (GPS RX)
MG-902 RX -> XIAO D6 / GPIO43 (GPS TX)
MG-902 VCC -> development board's configured 5 V GPS output
MG-902 GND -> XIAO GND
```

The receiver is started at 9600 baud, switched to 115200 baud, and configured for the high-rate UBX stream used for 25 Hz logging.
The MG-902 supply is 5 V; its TX/RX UART interface uses 3.3 V logic. Do not connect its VCC to the 3V3 output for this build.

### MicroSD

```text
SD VCC  -> XIAO 3V3 (initial bench test)
SD GND  -> XIAO GND
SD CS   -> D2 / GPIO3
SD MOSI -> D10 / GPIO9
SD SCK  -> D8 / GPIO7
SD MISO -> D9 / GPIO8
```

Use a FAT32 card. For this board-development branch the initial test is at 3.3 V so the SD interface and XIAO use the same logic supply. Confirm the particular SD breakout initializes and passes sustained write testing at 3.3 V before relying on it. SPI should initially remain conservative at 4 MHz.

### ECU tachometer RPM

```text
CB500 ECU tach output -> 12 V optocoupler input
Optocoupler OUT       -> XIAO D3 / GPIO4
Optocoupler logic VCC -> XIAO 3V3
Optocoupler logic GND -> XIAO GND
```

Never connect the motorcycle tachometer output directly to the ESP32.

RPM uses a falling-edge interrupt, rejects edges closer than 1.5 ms, and returns zero after 500 ms without an accepted pulse. Default calibration is:

```cpp
RPM_PULSES_PER_REVOLUTION = 2.0f
```

Verify against the motorcycle tachometer. If indicated RPM is exactly half or double, adjust `RPM_PULSES_PER_REVOLUTION` in `src/sensors/RaceSyncSensors.h` and rebuild.

### Throttle-position sensor

RaceSync supports a 3.3 V potentiometric TPS such as the Vishay 6127V1A60L.5:

```text
XIAO 3V3 ----------------- TPS supply
XIAO GND ----------------- TPS ground
TPS output ---- 1 kΩ ----- D0 / GPIO1 (ADC)
                  |
                100 nF
                  |
                 GND
```

Never allow the TPS output to exceed 3.3 V. Keep the sensor wiring away from
ignition and HT wiring. The 1 kΩ series resistor and 100 nF capacitor should be
mounted close to the ESP32. The sensor must not become a throttle stop or prevent
the carburettors returning freely.

The ADC is sampled at 200 Hz and lightly filtered; the latest value is attached
to each 25 Hz GPS record. Closed and full-throttle ADC values are calibrated from
the Control page and stored in NVM. A minimum span of 400 ADC counts prevents an
accidental two-point calibration at nearly the same position.

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

Starting a VBO download clears any pending automatic-start candidate. After the transfer, automatic logging remains inhibited until fresh GPS confirms the motorcycle is stationary for three continuous seconds. Automatic start itself requires 50 consecutive qualifying GPS samples, so time spent in a blocking web transfer cannot satisfy the two-second movement confirmation.

If an automatic session starts accidentally, **Stop Logging** is accepted only when GPS is fresh, valid, and at or below the 3 km/h stop threshold. This preserves protection against stopping a genuine on-track recording while providing a safe paddock recovery path.

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

Each VBO row writes `avifileindex` as `0000` and `avisynctime` as elapsed
GPS milliseconds from the first logged sample. The first value is `000000000`;
later values follow actual GPS timing, including delayed or missing packet
intervals. This provides the video/data timing reference for Circuit Tools. It
does not place the GoPro's MP4 filename in the VBO.

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
| GET | `/api/settings/throttle` | Read live TPS value and calibration |
| POST | `/api/settings/throttle/calibrate` | Capture `closed`, `open`, or `clear` calibration while idle |
| POST | `/api/reboot` | Restart ESP32 while idle |
| GET | `/api/camera` | Read cached manual camera state |
| POST | `/api/camera/connect` | Enable BLE, scan once and connect while idle |
| POST | `/api/camera/refresh` | Request camera status while idle |
| POST | `/api/camera/disconnect` | Disconnect the camera while idle |

## RPM diagnostics

The Status page exposes current RPM, signal-present state, accepted pulse count, rejected over-range readings, last-pulse age and GPIO4 input level. These counters are live debugging aids and reset on reboot; they are not VBO channels.

The RPM blue activity LED can be disabled while idle without disabling RPM capture. Its preference survives reboot. Disabling it makes the green recording indication easier to see.

## Throttle diagnostics and calibration

The Control page shows raw and filtered GPIO1 ADC readings, sensor connection
state, saved endpoints and calculated throttle percentage. With the engine
stopped, capture closed throttle first, then hold the carburettors fully open and
capture full throttle. Confirm the live display returns close to 0% and reaches
close to 100%. Recalibrate after any sensor, bracket or linkage adjustment.

GPIO8/9 remain reserved for I2C. IMU and brake-pressure capture are not currently
implemented.

## VBO output

VBO is the primary motorsport data format. It contains GPS position, speed, heading, altitude, timing and solution information. Filtered engine RPM is written to `Revs` and duplicated unchanged as `rc_rpm` for RaceChrono compatibility. Calibrated throttle opening is written to the custom `throttle` channel as 0–100%. RaceChrono Pro is the preferred analysis application because it imports custom VBO channels and can link external action-camera video.

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

## Firmware version and build identity

Every PlatformIO build runs `scripts/generate_build_info.py`. The firmware version
remains the deliberately assigned release value (currently `V2.1`), while the
build number is the Git commit count and the build also records the eight-character
commit hash. A local build with tracked uncommitted changes adds `-dirty` to the
hash.

The Status page and both `/api/status` and `/api/runtime` show values such as:

```text
V2.1 · build 184 · a1b2c3d4
```

This makes firmware built locally or by GitHub traceable to its exact repository
state. Commit changes before producing firmware intended for the motorcycle so
the displayed identity does not carry the `-dirty` suffix.

## Building and uploading

```text
platformio run
platformio run --target upload
platformio device monitor
```
