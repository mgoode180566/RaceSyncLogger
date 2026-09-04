# RaceSync Motorcycle Data Logger

RaceSync is a standalone ESP32-S3 motorcycle data logger for Honda CB500 track and race use. Firmware V2.1 records 25 Hz GPS and engine RPM data to microSD, produces VBOX-compatible VBO sessions, and provides an onboard Wi-Fi interface for configuration and file management.

The intended workflow is simple: **power it on, ride, then download the data in the paddock.**

For rider instructions, see [docs/USER_GUIDE.md](docs/USER_GUIDE.md).

These instructions describe the current `main` branch (the repository's default branch), including RPM diagnostics and the saved RPM blue-LED preference.

## Current functionality

- MicoAir MG-902/u-blox GPS configured for 25 Hz logging
- Engine RPM capture from an isolated ECU tachometer signal
- Live RPM, accepted-pulse count, rejected-reading count, pulse age and GPIO4 level on the Status page
- Optional blue RPM activity indication, with a preference saved across reboots
- Automatic recording using configurable speed and stop-delay settings
- Manual start and stop controls through the web interface
- FAT32 microSD storage with startup write/read/delete health testing
- Active sessions written as `.part`, then finalized as `.vbo` after a clean stop
- Automatic recovery of valid, complete VBO rows following interrupted power
- KML generated and streamed only when requested through the web interface
- Recording LED indication and five-part startup diagnostics
- Session listing, new-session tracking, download, deletion, status and reboot controls
- REST API for status, telemetry, settings, logging and sessions

## Normal workflow

1. Power RaceSync and let startup diagnostics finish.
2. Give the GPS antenna a clear view of the sky and wait for a valid fix.
3. Ride away. Logging starts when GPS speed reaches the configured start speed.
4. With the RPM blue LED disabled, the onboard LED gives a short green flash approximately once per second while recording. Blue RPM activity takes priority when enabled.
5. After returning, remain below 3 km/h for the configured stop delay.
6. Confirm recording has stopped before removing power whenever possible; use the web logger status if blue RPM activity obscures the green indicator.
7. Connect to RaceSync Wi-Fi and open `http://192.168.4.1`.
8. Download the VBO, generate a KML if required, or delete old sessions.

## Hardware connections

### MG-902 GPS

```text
MG-902 TX -> ESP32 GPIO16 (GPS RX)
MG-902 RX -> ESP32 GPIO17 (GPS TX)
```

The firmware starts the receiver at 9600 baud, changes it to 115200 baud, and configures the high-rate UBX stream used for 25 Hz logging.

### MicroSD module

The tested SD reader/writer is powered from the 5 V supply:

```text
SD VCC  -> 5 V
SD GND  -> ESP32 GND
SD CS   -> GPIO10
SD MOSI -> GPIO11
SD SCK  -> GPIO12
SD MISO -> GPIO13
```

Use a FAT32-formatted card. SPI runs at 4 MHz for conservative write reliability. ESP32 GPIO pins are 3.3 V only: use the confirmed module with suitable onboard regulation/level handling and never apply 5 V directly to an ESP32 GPIO.

### ECU tachometer RPM

```text
CB500 ECU tach output -> 12 V optocoupler input
Optocoupler OUT       -> ESP32 GPIO4
Optocoupler logic VCC -> ESP32 3V3
Optocoupler logic GND -> ESP32 GND
```

Never connect the motorcycle tachometer signal directly to the ESP32. RaceSync expects an isolated 3.3 V logic signal at GPIO4.

RPM capture uses a falling-edge interrupt, rejects edges closer than 1.5 ms, returns zero after 500 ms without a pulse, and lightly smooths period jitter. The latest RPM value is attached to each GPS sample and written to the VBO `Revs` channel.

The default calibration is two falling edges per crankshaft revolution:

```cpp
RPM_PULSES_PER_REVOLUTION = 2.0f
```

Verify it against the motorcycle tachometer before track use. If RaceSync reads exactly half or twice the displayed RPM, adjust `RPM_PULSES_PER_REVOLUTION` in `src/sensors/RaceSyncSensors.h`.

RPM is calculated as `60,000,000 / (period_us * pulses_per_revolution)`. Increasing the pulse-per-revolution value reduces the indicated RPM. This is a compile-time setting, not a web setting.

### RPM diagnostics and LED preference

Open `/status` and look under **ENGINE SPEED**. RPM diagnostics refresh every 250 ms; the full status JSON refreshes every five seconds. The Sessions page also displays RPM, refreshed every five seconds.

| Display / `/api/telemetry` field | Meaning |
|---|---|
| RPM / `rpm.value` | Current filtered RPM; also available as `channels.Revs` |
| Signal / `rpm.signalPresent` | An accepted pulse arrived within the 500 ms timeout; not a guarantee of a valid RPM reading |
| Accepted pulses / `rpm.pulseCount` | Falling edges accepted by the interrupt filter since boot |
| Rejected readings / `rpm.rejectedReadingCount` | Evaluated readings above 15,000 RPM, counted once per newly evaluated pulse interval |
| Last pulse / `rpm.lastPulseAgeMs` | Age of the last accepted pulse; `-1` in the API and `Never` in the UI before any pulse |
| GPIO4 level / `rpm.inputLevel` | Sampled digital input level, `0` or `1`; not an oscilloscope trace |
| RPM blue LED / `rpm.ledEnabled` | Saved RPM activity-indicator preference; enabled by default |

The rejected-reading counter **does not count** edges filtered out for arriving less than 1.5 ms after the last accepted edge, or signal timeouts. The main loop evaluates the latest interval, so it is not an exhaustive count of all physical noise events. Both counters reset on reboot and are not stored in VBO files or NVS. RPM remains available without GPS or recording, but there is no persistent idle RPM history.

Readings above 15,000 RPM are currently replaced with zero; losing accepted pulses for more than 500 ms also returns zero. A rising rejected-reading counter during a dropout points to an over-range interval. An increasing pulse age and `NO SIGNAL` point to missing accepted pulses. Neither observation alone proves an optocoupler fault.

Uncheck **RPM blue LED** while idle to disable only the RPM activity indication. The setting saves immediately and survives reboot; it does not disable RPM capture, VBO recording, startup diagnostics or the green recording indicator. Enabled activity retriggers a 60 ms blue indication, which may appear continuous at engine speeds and takes priority over green recording flashes. This is the logger's onboard LED, not the optocoupler module's input LED.

### Sensors not yet implemented

`include/Pins.h` reserves GPIO1 for `TPS_ADC` and GPIO8/9 for I2C, but `main` does not yet read or calibrate a throttle-position sensor, IMU or brake-pressure sensor. There is no throttle channel or throttle calibration UI. Reserved pins and placeholder VBO channels should not be interpreted as working sensor support.

## Startup diagnostics and LED sequence

RaceSync runs five checks in order. For each check, its number of flashes identifies it; green means pass and red means fail.

| Flashes | Check |
|---:|---|
| 1 | Wi-Fi access point |
| 2 | microSD storage |
| 3 | logger initialization |
| 4 | valid GPS communications within five seconds |
| 5 | sensor subsystem, including RPM input setup |

The LED is solid red while diagnostics begin. Five blue flashes indicate that the full sequence is complete. Detailed results and the final failure mask are also written to the serial monitor.

The GPS check requires valid receiver packets, but not a position fix. A red storage result means the SD card is not ready for logging.

Sensor check 5 confirms input initialization, not that an ECU signal is present. The five completion flashes still occur when the RPM blue LED preference is off. These colours require a board with the supported onboard RGB LED; a single-colour LED cannot distinguish the colours.

## Automatic and manual logging

Default settings:

```text
Start recording: 10 km/h
Stop threshold:   3 km/h
Stop delay:       60 seconds
```

The web control page can set the start speed from 1–100 km/h and stop delay from 1–600 seconds. The 3 km/h stop threshold is fixed. Settings survive a reboot and cannot be changed while recording.

Automatic logging requires valid live GPS data. Manual start also requires a valid GPS fix and ready storage. A manually started session continues until **Stop Logging** is pressed. Following manual stop, automatic restart is inhibited until speed falls below the configured start threshold.

Storage errors or low free space can also stop a manual session. Sample writing and automatic-stop evaluation depend on valid new GPS samples; if GPS data is lost, check recording state and stop manually rather than assuming the stop delay will close the file. Wi-Fi and the API remain serviced during recording in the current implementation; download/manage sessions in the paddock to avoid adding work to the recording loop.

## Session integrity and power-loss recovery

During recording, RaceSync writes only the primary VBO data path and uses an incomplete filename:

```text
While recording: RS_2026-09-02_10-32-15.part
After clean stop: RS_2026-09-02_10-32-15.vbo
```

The active file is flushed every second. Logging will not start with less than 1 MB free and stops if space later falls below that limit. On a clean stop, RaceSync flushes and closes the file, renames it to `.vbo`, and verifies the rename before publishing it in the session list.

If power is lost, the `.part` file remains. On the next boot, after the SD health check passes, RaceSync:

1. Finds incomplete `.part` sessions.
2. Copies only newline-terminated records and discards a potentially torn final row.
3. Requires a VBO header, column names, data section, and at least one complete data row.
4. Publishes the recovered data as `.vbo` before removing the original `.part`.

If the intended VBO name already exists, `_RECOVERED_n.vbo` is used. A file that fails validation remains as `.part` for manual investigation. Recovery preserves complete records already written to the card; it cannot recover bytes that had not reached storage.

Recovery validation checks section markers and the presence of complete data lines, not every column's numeric contents or a checksum. Inspect recovered recordings before relying on their data.

Recovery results appear under `storage.recovery` in `GET /api/status`:

```json
{
  "recoveredPartFiles": 1,
  "failedPartRecoveries": 0,
  "lastRecoveredFilename": "RS_2026-09-02_10-32-15.vbo"
}
```

## Web interface

```text
SSID:     RaceSync
Password: racesync
IP:       192.168.4.1
```

| Address | Purpose |
|---|---|
| `http://192.168.4.1/` | Completed sessions, downloads and deletion |
| `http://192.168.4.1/control` | Manual logging, automatic settings and reboot |
| `http://192.168.4.1/status` | Full device status |

The main page marks sessions as **NEW** using download history held in that browser. **Download All New** downloads each new VBO. A different browser or device has separate history.

RaceSync does not write or retain a KML during a track session. **Generate KML** streams a KML directly from a completed VBO on demand, keeping the SD write path focused on the primary recording.

Reboot and logging-setting changes are blocked while a session is active. Manual start is disabled until GPS and storage are ready, and the incomplete active `.part` file is never exposed in the completed-session list.

## REST API

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/status` | System, GPS, storage, logger and health diagnostics |
| GET | `/api/location` | Current GPS location |
| GET | `/api/telemetry` | Current GPS and sensor channels, including `channels.Revs` |
| GET | `/api/sessions` | Stored completed-session list |
| GET | `/api/sessions/{id}` | Download a VBO session |
| GET | `/api/session-kml?id={id}` | Generate and download KML from a completed VBO |
| DELETE | `/api/sessions/{id}` | Delete a completed session |
| POST | `/api/logging/start` | Start a manual session |
| POST | `/api/logging/stop` | Stop and finalize the active session |
| GET | `/api/settings/logging` | Read automatic logging settings |
| POST | `/api/settings/logging` | Save automatic logging settings |
| POST | `/api/settings/rpm-led` | Save RPM blue-LED preference while idle |
| POST | `/api/reboot` | Restart the ESP32 while idle |

Example logging-settings request:

```json
{
  "startSpeedKmh": 10.0,
  "stopDelaySeconds": 60
}
```

To disable RPM blue activity, POST the following JSON to `/api/settings/rpm-led`:

```json
{"enabled": false}
```

A successful response contains `{"saved":true,"enabled":false}`. Use `true` to enable it again. The route returns HTTP 409 while recording, 400 for an invalid/missing boolean, and 500 if saving fails. Read the current setting through `/api/telemetry` under `rpm.ledEnabled`; there is no GET route for this setting. RPM diagnostics are in `/api/telemetry`, not `/api/status`.

## VBO output

VBO is the primary stored format. It contains position, speed, heading, altitude, sample timing, solution data, and available sensor channels. RPM is written to `Revs` for motorsport-analysis compatibility.

The existing oil/fuel pressure, oil/water temperature and acceleration fields are placeholders, not live sensor measurements. RPM diagnostic counters and the LED preference are not VBO channels.

RaceSync remains circuit-independent. Circuit recognition, start/finish detection, and lap analysis happen afterwards in software such as Circuit Tools.

## Firmware structure

```text
src/
├── api/        REST API, KML streaming and onboard web interface
├── app/        startup diagnostics and application controller
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

The PlatformIO target is `esp32-s3-devkitc-1`. The repository currently specifies COM4; change `upload_port` and `monitor_port` in `platformio.ini` if Windows assigns another port.

## Pre-track checks

1. Confirm SD-module 5 V power, common ground, and the documented SPI connections.
2. Confirm the tachometer signal reaches GPIO4 only through the isolated interface.
3. Confirm startup storage diagnostic 2 passes.
4. Confirm GPS diagnostic 4 passes, then obtain a valid outdoor fix.
5. Compare Status-page RPM against the motorcycle tachometer; check accepted pulses, rejected readings and last-pulse age. Confirm the two-pulse-per-revolution calibration is correct for the fitted signal source.
6. Disable the RPM blue LED if you want an unobscured green recording indicator; reboot while idle and verify the preference persists.
7. Record a short test, stop normally, download the VBO, and inspect its `Revs` data.
8. Perform a controlled interrupted-power test using disposable test data and confirm recovery through `/api/status` before relying on it at the circuit.
