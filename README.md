# RaceSync V2.1 Modular

RaceSync is a standalone ESP32-S3 motorcycle GPS data logger designed
for track and race use. The firmware records VBOX-compatible session
files, provides a local Wi-Fi access point and REST API, and allows a
phone, tablet, or laptop to inspect the logger and download recorded
sessions without requiring an internet connection.

This V2.1 package uses LittleFS in the ESP32's internal flash (NVM) for
session storage. The storage layer is deliberately isolated so that a
later production version can move session storage to microSD without
changing the logger, REST API, or client workflow.

## System goals

RaceSync is intended to operate as an appliance on the motorcycle:

-   power the logger on;
-   allow it to acquire GPS automatically;
-   begin recording automatically once the motorcycle exceeds the
    configured start speed;
-   continue recording without user interaction;
-   stop and close the session after the motorcycle has remained below
    the configured stop speed for the configured period;
-   make system health and recorded sessions available over Wi-Fi;
-   allow sessions to be downloaded directly to a phone or laptop;
-   continue to provide useful functionality when the GPS is absent by
    entering Demo mode.

The logging path is the primary function. Wi-Fi, REST access,
diagnostics and Demo mode are supporting functions and must not prevent
normal data logging.

## Hardware target

The current firmware targets:

-   ESP32-S3 DevKitC-1
-   16 MB flash configuration
-   MG-902 / u-blox M10 GPS connected by UART
-   internal NVM using LittleFS
-   Wi-Fi operating as an access point

Current GPS UART configuration:

-   GPS TX -\> ESP32 GPIO16 (RX)
-   GPS RX -\> ESP32 GPIO17 (TX)
-   baud rate: 115200

Future hardware can include IMU, throttle position, brake pressure and
microSD storage. The modular firmware structure provides locations for
those functions without requiring the main application to be redesigned.

## Project structure

``` text
RaceSyncLogger_V2_1_Modular/
├── platformio.ini
├── partitions.csv
├── README.md
├── data/
│   └── VBOX0004.vbo
└── src/
    ├── api/
    │   ├── RaceSyncApi.cpp
    │   └── RaceSyncApi.h
    ├── app/
    │   ├── RaceSyncController.cpp
    │   └── RaceSyncController.h
    ├── config/
    │   ├── RaceSyncConfig.h
    │   └── RaceSyncTypes.h
    ├── gps/
    │   ├── RaceSyncGps.cpp
    │   └── RaceSyncGps.h
    ├── logging/
    │   ├── RaceSyncLogger.cpp
    │   ├── RaceSyncLogger.h
    │   ├── RaceSyncStorage.cpp
    │   └── RaceSyncStorage.h
    ├── sensors/
    │   ├── RaceSyncSensors.cpp
    │   └── RaceSyncSensors.h
    ├── sim/
    │   ├── RaceSyncSimulator.cpp
    │   └── RaceSyncSimulator.h
    ├── wifi/
    │   ├── RaceSyncWifi.cpp
    │   └── RaceSyncWifi.h
    └── main.cpp
```

### Module responsibilities

`RaceSyncController` coordinates the system. It starts the subsystems,
selects LIVE or DEMO operation, routes new telemetry samples to the
logger and keeps the REST service running.

`RaceSyncGps` owns the GPS UART and parses u-blox UBX NAV-PVT messages.
It supplies position, speed, heading, altitude, satellite count, fix
state and sample timing.

`RaceSyncSimulator` reads a VBOX file from storage and replays its data
at the sample interval recorded in the file. It is used when a physical
GPS is unavailable.

`RaceSyncLogger` controls automatic recording, creates VBOX-compatible
files, writes samples and closes sessions after the configured
stationary period.

`RaceSyncStorage` provides the storage abstraction. V2.1 uses
LittleFS/NVM. Other modules do not need to know whether the underlying
storage is NVM or, in a future release, SD card.

`RaceSyncWifi` creates the local RaceSync Wi-Fi access point.

`RaceSyncApi` exposes diagnostics, current telemetry, position and
recorded sessions to a browser or client application.

`RaceSyncSensors` is the extension point for additional sensors such as
IMU, throttle position and brake pressure.

`main.cpp` is intentionally small and only starts and updates the
application controller.

## Normal user workflow

RaceSync is designed so that logging does not require a phone or laptop.

### At the circuit

1.  Power RaceSync from the motorcycle or an external battery.
2.  The ESP32 starts its Wi-Fi access point and initializes storage and
    GPS.
3.  The GPS begins acquiring a fix.
4.  The rider can optionally connect a phone or laptop to the RaceSync
    Wi-Fi network and inspect `/api/status`.
5.  Once valid telemetry is available and speed reaches the configured
    logging threshold, RaceSync automatically creates a new session and
    starts recording.
6.  No interaction is required while riding.
7.  When speed remains at or below the stop threshold for the configured
    period, the logger flushes and closes the VBO file.

Current thresholds are:

``` text
Start recording:  >= 10 km/h
Stop condition:    <= 3 km/h
Stop delay:        60 seconds
```

These values are held centrally in `RaceSyncConfig.h`.

### Reviewing sessions

After the session:

1.  Leave RaceSync powered on.
2.  Connect the phone, tablet or laptop to the RaceSync Wi-Fi access
    point.
3.  Open the session list endpoint or use a RaceSync-compatible web
    application.
4.  Select the required session.
5.  Download the VBO file to the user device.
6.  The downloaded file can then be processed or displayed by the
    RaceSync client software.

The device does not require an internet connection for this local
workflow.

## Wi-Fi access

RaceSync operates as its own Wi-Fi access point.

Default configuration:

``` text
SSID:     RaceSync
Password: racesync
IP:       192.168.4.1
```

A connected device can therefore access the API at addresses such as:

``` text
http://192.168.4.1/api/status
http://192.168.4.1/api/sessions
http://192.168.4.1/api/telemetry
```

The API includes CORS headers so a compatible browser-based client can
communicate with the logger.

## Operating modes

RaceSync has three internal data modes.

### STARTING

Immediately after boot the controller enters `STARTING`.

The GPS is given a short grace period to begin producing valid UBX data.
This prevents the device from immediately entering Demo mode while the
GPS is still starting.

### LIVE

When the MG-902 is detected and NAV-PVT packets are being received,
RaceSync operates in `LIVE` mode.

Telemetry originates from the physical GPS. Valid GPS samples can
trigger and feed the session logger.

The GPS is considered connected while packets continue to arrive within
the configured stale-data interval.

### DEMO

If a GPS is not available after the startup grace period, RaceSync can
automatically enter `DEMO` mode.

Demo mode allows the complete logger/API/client workflow to be tested
without a GPS receiver or moving motorcycle.

The simulator searches storage dynamically for a `.vbo` file that does
not begin with `RS_`. The first suitable file found becomes the demo
source. The filename is therefore not hard-coded into the firmware.

For example:

``` text
/VBOX0004.vbo
/VBOX0005.vbo
/DoningtonTest.vbo
```

can all be potential demo source files.

Files generated by RaceSync itself use an `RS_` prefix and are excluded
from selection as the demo source. This prevents a newly recorded
RaceSync session from accidentally becoming the simulator input.

The simulator:

-   locates the `[data]` section of the VBO file;
-   reads each data row;
-   converts the stored VBOX coordinates to decimal latitude/longitude
    for telemetry;
-   reproduces speed, heading and other available channels;
-   honours the sample period contained in the VBO data;
-   loops the demo session when it reaches the end.

Importantly, Demo mode feeds the same logger path as LIVE mode. If the
replayed speed exceeds the logging threshold, RaceSync creates and
records a new `RS_DEMO_...vbo` session. This makes it possible to test
automatic start/stop, storage, session discovery, download and the UI
without live GPS hardware.

If the physical GPS becomes available while the system is not committed
to a conflicting recording transition, the controller can return to LIVE
operation.

## Session storage

V2.1 stores files in LittleFS in ESP32 internal flash.

The custom partition table reserves a large flash area for the
filesystem. `RaceSyncStorage` is the only module responsible for direct
filesystem access.

This abstraction provides:

-   session enumeration;
-   safe filename validation;
-   opening files for reading;
-   creating files for recording;
-   locating a Demo mode source;
-   storage capacity reporting;
-   session metadata for the REST API.

The intention is that a later SD-card implementation can replace the
LittleFS backend while keeping interfaces such as `/api/sessions`
unchanged.

### Session naming

LIVE sessions with valid GPS date/time use names similar to:

``` text
RS_20260826_104512.vbo
```

If full GPS date/time is unavailable, a fallback name based on operating
mode and device uptime is used, for example:

``` text
RS_LIVE_0000123456.vbo
RS_DEMO_0000123456.vbo
```

## VBOX output

RaceSync writes VBOX-style text sessions containing a header, comments,
column definitions and data section.

Current data fields include:

``` text
sats
time
lat
long
velocity
heading
height
vert-vel
Tsample
solution_type
avifileindex
avitime
ComboAcc
ADC3_Oil_Pressure
ADC2_Oil_Temp
ADC1_Water_Temp
Revs
ADC4_Fuel_Pressure
Combo_G
```

Some channels currently originate from the demo file or remain
placeholders until the associated physical sensors are fitted.

## REST API

### `GET /api/status`

This is the principal diagnostic endpoint and is intended to expose
enough information to determine the health of RaceSync remotely.

It reports the following groups.

#### System

Includes:

-   product name;
-   firmware version;
-   current mode (`STARTING`, `LIVE` or `DEMO`);
-   uptime in seconds;
-   human-readable uptime;
-   persistent boot count;
-   ESP32 reset reason.

Example:

``` json
{
  "system": {
    "product": "RaceSync",
    "firmware": "V2.1",
    "mode": "LIVE",
    "uptimeSeconds": 842,
    "uptime": "0d 00:14:02",
    "bootCount": 18,
    "resetReason": "POWER_ON"
  }
}
```

The reset reason is particularly useful during long-duration testing
because an unexpected brownout, watchdog reset or software reset can be
identified remotely after the device restarts.

#### Health

Provides a high-level summary for:

-   overall system;
-   GPS;
-   storage;
-   logger;
-   Wi-Fi.

Typical states include:

``` text
OK
WARNING
DEMO
NO_DEVICE
NO_FIX
IDLE
RECORDING
ERROR
```

This section is intended to allow a UI to provide a simple health
dashboard without interpreting every low-level diagnostic field.

#### Board

Reports ESP32 information including:

-   board model;
-   chip model;
-   chip revision;
-   core count;
-   CPU frequency;
-   ESP-IDF/SDK version;
-   flash capacity;
-   heap capacity;
-   current free heap;
-   minimum observed free heap;
-   maximum allocatable heap block;
-   PSRAM capacity and free PSRAM.

These fields are useful for stability and endurance testing and can
expose memory exhaustion or unexpected hardware configuration.

#### Wi-Fi

Reports:

-   AP state;
-   operating mode;
-   SSID;
-   password;
-   local IP address;
-   number of connected clients;
-   Wi-Fi uptime.

This allows the status UI to show exactly how the user should connect to
the device.

#### GPS

Reports:

-   whether the physical GPS is connected;
-   current data source;
-   validity of the GPS fix;
-   fix/solution type;
-   satellite count;
-   latitude;
-   longitude;
-   speed;
-   heading;
-   altitude;
-   calculated GPS sample rate;
-   age of the last GPS packet;
-   total GPS bytes received;
-   parsed packet count;
-   checksum error count;
-   selected Demo VBO source, where applicable.

These values distinguish several different conditions. For example, a
GPS may be physically communicating but not yet have a valid positional
fix.

#### Storage

Reports:

-   storage type (`NVM`);
-   filesystem (`LittleFS`);
-   readiness;
-   total capacity;
-   used capacity;
-   free capacity;
-   percentage used;
-   number of VBO sessions;
-   write-error count.

This is intended to make impending storage exhaustion or filesystem
problems visible before a race session.

#### Logger

Reports:

-   logger state;
-   whether recording is active;
-   current filename;
-   samples written;
-   configured start-speed threshold;
-   configured stop-speed threshold;
-   configured stop delay;
-   current recording duration;
-   age of the last successful write.

During a session, these fields allow a remote client to establish that
RaceSync is not merely powered on but is actively writing data.

#### Power

The current version identifies the power source as external but does not
yet measure supply voltage or battery percentage.

The API explicitly reports that voltage and battery-percentage
monitoring are unavailable rather than presenting invented values.

### `GET /api/location`

Returns a lightweight current-position response:

``` json
{
  "valid": true,
  "latitude": 52.123456,
  "longitude": -1.234567,
  "speedKmh": 84.2,
  "heading": 126.4,
  "height": 104.2,
  "satellites": 14
}
```

This endpoint is suitable when a client only needs current GPS
information rather than the complete diagnostic status.

### `GET /api/telemetry`

Returns the current telemetry sample, including:

-   validity;
-   LIVE/DEMO mode;
-   sample index;
-   sample period;
-   VBOX time;
-   GPS data;
-   available VBOX/sensor channels.

This endpoint can be polled by a development UI to display live
information.

### `GET /api/sessions`

Dynamically enumerates the VBO files currently available in storage.

Example structure:

``` json
{
  "device": "RaceSync",
  "count": 2,
  "sessions": [
    {
      "file": "VBOX0004.vbo",
      "sizeBytes": 123456,
      "complete": true,
      "downloadUrl": "/api/sessions/VBOX0004.vbo",
      "generatedByRaceSync": false
    },
    {
      "file": "RS_20260826_104512.vbo",
      "sizeBytes": 654321,
      "complete": true,
      "downloadUrl": "/api/sessions/RS_20260826_104512.vbo",
      "generatedByRaceSync": true
    }
  ]
}
```

No session names are hard-coded into the API.

### `GET /api/sessions/{filename}`

Downloads the selected VBO file.

For example:

``` text
GET /api/sessions/RS_20260826_104512.vbo
```

The response is returned as a downloadable file. Filename validation
prevents path traversal and limits this endpoint to VBO files.

## Using the status endpoint during endurance testing

A useful development workflow is to power RaceSync from an external
battery for several hours and periodically inspect:

``` text
http://192.168.4.1/api/status
```

Particularly useful fields are:

``` text
system.uptimeSeconds
system.bootCount
system.resetReason

board.freeHeapBytes
board.minFreeHeapBytes

wifi.connectedClients

gps.connected
gps.validFix
gps.satellites
gps.sampleRateHz
gps.lastPacketAgeMs
gps.packetCount
gps.checksumErrors

storage.freeBytes
storage.usedPercent
storage.writeErrors

logger.state
logger.samplesWritten
logger.lastWriteAgeMs

health.overall
```

A continuously increasing uptime with stable heap, GPS packets and
logger writes is a strong indication that the device is operating
normally.

If `bootCount` increases unexpectedly, `resetReason` provides the first
diagnostic clue.

## Loading Demo data

Place one or more `.vbo` files in the project's `data/` directory:

``` text
data/
├── VBOX0004.vbo
└── another-session.vbo
```

Upload the LittleFS filesystem image separately from the firmware.

On the current Windows/PlatformIO setup:

``` powershell
C:\Users\Dell\.platformio\penv\Scripts\platformio.exe run -t uploadfs --upload-port COM3
```

Then upload the firmware:

``` powershell
C:\Users\Dell\.platformio\penv\Scripts\platformio.exe run -t upload --upload-port COM3
```

The firmware and filesystem are separate. A normal firmware upload does
not require the demo filesystem to be uploaded again unless the contents
of `data/` have changed.

The configured COM port can be changed in `platformio.ini` if Windows
assigns the ESP32 a different port.

## Browser/client architecture

The current local architecture is deliberately simple:

``` text
                  MOTORCYCLE
                      |
                +-----------+
                | RaceSync  |
                | ESP32-S3  |
                +-----------+
                 |         |
             GPS |         | NVM
                 |         |
                 +----+----+
                      |
                 RaceSync Wi-Fi
                      |
             +--------+--------+
             |                 |
           Phone             Laptop
             |                 |
             +--------+--------+
                      |
                 REST API
                      |
             Browser / RaceSync UI
                      |
               Download VBO
```

The user device connects directly to RaceSync. No cloud server, Spring
application, Node.js service or internet connection is required for
acquiring the file from the logger.

A browser-based RaceSync client can use `/api/status` for system health,
`/api/sessions` to discover sessions and `/api/sessions/{filename}` to
download the selected data.

A later workflow can reconnect the phone or laptop to the internet and
upload the downloaded session to cloud storage for richer analysis. That
cloud stage is deliberately separate from the critical motorcycle
logger.

## Reliability behaviour

The firmware is designed around several important principles:

-   logging starts automatically;
-   the rider does not need to interact with the device while moving;
-   loss of a phone or browser connection does not stop logging;
-   session files are periodically flushed while recording;
-   the logger closes the file after the configured low-speed period;
-   GPS communication errors are counted and exposed through
    diagnostics;
-   storage errors are counted and exposed through diagnostics;
-   Demo mode is automatically available when a suitable VBO file exists
    and the GPS is absent;
-   the storage implementation is isolated from the rest of the
    application;
-   diagnostic information is available remotely over Wi-Fi.

## Current limitations

V2.1 is still a development version.

The current implementation does not yet include:

-   production microSD session storage;
-   IMU acquisition;
-   throttle-position acquisition;
-   brake-pressure acquisition;
-   supply-voltage monitoring;
-   battery percentage;
-   cloud upload from the ESP32;
-   user authentication or per-device security beyond the Wi-Fi
    password.

These are deliberately outside the current V2.1 scope.

## Future SD-card migration

The intended production storage architecture is:

``` text
RaceSyncController
        |
RaceSyncLogger
        |
RaceSyncStorage
        |
   +----+----+
   |         |
LittleFS     SD
 V2.1      future
```

The API, logger and client should continue using the same storage
interface. Moving recorded sessions to SD should therefore be primarily
a change inside the storage implementation rather than a redesign of the
application.

## Quick test

After flashing the firmware and Demo filesystem:

1.  Power the ESP32-S3.
2.  Wait for the RaceSync Wi-Fi network.
3.  Connect using password `racesync`.
4.  Browse to `http://192.168.4.1/api/status`.
5.  With no physical GPS connected, wait for the mode to become `DEMO`.
6.  Confirm that the demo source filename is reported.
7.  Browse to `http://192.168.4.1/api/telemetry` and confirm that
    position and speed change.
8.  Browse to `http://192.168.4.1/api/sessions` and confirm that VBO
    files are listed.
9.  When the replay exceeds 10 km/h, confirm that `logger.state` becomes
    `RECORDING`.
10. Download a session using its `downloadUrl`.

This exercises the core RaceSync workflow without requiring the
motorcycle or GPS hardware.
