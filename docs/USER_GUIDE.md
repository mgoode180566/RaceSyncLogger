# RaceSync Data Logger — User Guide

## What RaceSync does

RaceSync automatically records GPS, engine RPM and throttle position while the motorcycle is moving. Each completed run is saved as a VBOX-compatible VBO file on microSD for analysis in software such as RaceChrono Pro or Circuit Tools. It can also provide RaceChrono Pro with a GPS-only live Bluetooth feed for speed, heading, position and lap timing.

The normal race-day workflow is deliberately simple: **power it on, check it, ride, wait for it to stop, then download the session.** No rider interaction is required on track.

## Pairing a GoPro HERO9 with RaceSync

RaceSync uses Bluetooth Low Energy, not the GoPro Wi-Fi network, for camera
control. HERO9 firmware 1.70 or newer is required; firmware 1.72 is supported.

Complete the first pairing while RaceSync is idle:

1. Power the GoPro and enable its wireless connections.
2. On the GoPro, open its connection/pairing screen and leave it advertising.
   Depending on the camera menu, this may be labelled **Connect Device** or
   pairing with the GoPro app.
3. Power RaceSync and allow startup diagnostics to finish.
4. Join the `RaceSync` Wi-Fi network using password `racesync`.
5. Open `http://192.168.4.1/camera`.
6. Select **Enable Bluetooth & Connect** once.
7. Wait for **GoPro connected — not recording**, then check battery, remaining
   video time, SD-card status, overheating status, and GPS time sync.

RaceSync connects to the advertising camera named `GoPro XXXX` and saves its
BLE address. Keep only the intended camera in pairing mode for the first scan.
No PIN and no RaceSync connection to the GoPro's Wi-Fi network are required.

On future boots, keep the GoPro powered with wireless enabled. RaceSync waits
15 seconds and connects directly to the saved address. Once connected and idle,
RaceSync sends a BLE Keep Alive every three seconds. This prevents the camera's
inactivity timer from putting an externally powered camera to sleep between
sessions. RaceSync pauses Keep Alive while video is recording and never sends
the camera Sleep command. If the camera is not
available, it retries every 60 seconds only while the logger is idle and the
motorcycle is stationary. When valid GPS time is available, RaceSync also sets
the GoPro clock to UK local time with automatic GMT/BST handling.

If the ESP32 resets during its first automatic Bluetooth attempt, RaceSync
suppresses automatic connection on the following boot to prevent a boot loop.
Open the Camera page and complete one successful manual connection to restore
automatic connection.

## Automatic logging with GoPro video

Once the saved camera is connected, no camera interaction is required to record
a session:

1. Automatic logging starts when valid GPS speed reaches the configured start
   speed. Manual logging starts when **Start Logging** is selected.
2. RaceSync opens the VBO first, then asks the connected GoPro to start video.
3. A blue logging flash means this session was initiated with a connected
   camera. The usual green flash means RaceSync is logging data without an
   initiated camera recording.
4. Automatic logging stops after the full stationary delay. Manual logging
   stops when **Stop Logging** is selected.
5. RaceSync finalizes the VBO first, then asks the GoPro to stop recording.

Logging never waits for the camera. If the GoPro is disconnected, busy, or
unable to record, the RaceSync session continues normally and remains green.
Check the Camera page before going out whenever matching video is required.
For unattended operation, fit the GoPro battery as a backup and use a stable,
regulated USB-C supply. Keep Alive improves readiness but does not prove that
external power is present; monitor battery percentage and overheating status.

The VBO `avisynctime` column starts at zero and records elapsed GPS
milliseconds on every row. This aligns the RaceSync data timeline with video
started for the session; the GoPro MP4 filename is not written into the VBO.

GPS, RPM and throttle position are the current logger inputs. IMU and brake-pressure capture are not yet implemented.

## Using RaceChrono Pro as a live display

RaceSync can act as an external GPS receiver for RaceChrono Pro. The live feed
contains only GPS information:

- position;
- speed;
- heading;
- altitude;
- valid/invalid fix state;
- satellite count; and
- GPS date and time.

RPM, throttle position and other custom RaceSync channels are not sent over
Bluetooth. They remain available in the recorded VBO file.

To connect:

1. Power RaceSync and wait for the startup checks to finish.
2. Open RaceChrono Pro on the phone.
3. Open **Settings** and select **Add other device**.
4. Select **RaceChrono DIY**, then **Bluetooth LE**, then **GPS**.
5. Select **RaceSync GPS**.
6. Confirm that RaceChrono shows a valid external GPS fix before relying on its
   live speed or lap display.

Do not connect to or pair RaceSync from Android's Bluetooth settings. Android
may show any transmitting BLE device, whereas RaceChrono discovers RaceSync by
its advertised 16-bit DIY service UUID `0x1FF8`. Initiate the connection from
RaceChrono Pro.

The phone connection is optional and is not the recording-status indicator.
RaceSync records the authoritative VBO directly to microSD whether or not a
phone is connected.

### Recording always has priority

RaceSync finishes processing and logging every GPS sample before offering a
copy to Bluetooth. Bluetooth runs as a separate low-priority task with a
single latest-fix mailbox. It cannot build up a queue. If RaceChrono or the
phone is slow, RaceSync discards an older unsent Bluetooth fix and retains the
newest one instead of delaying the logger.

The following events must not interrupt the VBO recording:

- RaceChrono being opened or closed;
- the phone connecting or disconnecting;
- Bluetooth being switched off on the phone;
- the phone moving out of range; or
- RaceChrono failing to consume notifications quickly enough.

If the phone display freezes or disconnects, continue the session normally.
RaceSync's SD recording and automatic stop logic remain independent. After the
session, wait for RaceSync to return to `IDLE` and download the VBO in the usual
way.

## Before going out

- Fit a FAT32-formatted microSD card.
- Power the tested SD reader/writer from 5 V with a common ground to the ESP32.
- Give the GPS antenna a clear upward view.
- Ensure the ECU tachometer signal reaches GPIO4 only through the 12 V optocoupler/isolation circuit.
- Set the desired automatic start speed and stop delay while RaceSync is idle.

## Startup lights

RaceSync performs five startup checks. Green flashes mean pass and red flashes mean fail; the number of flashes identifies the check.

| Flashes | Check |
|---:|---|
| 1 | RaceSync Wi-Fi |
| 2 | microSD card/storage health |
| 3 | logger initialization |
| 4 | GPS receiver communications |
| 5 | sensor subsystem including RPM input setup |

Five blue flashes indicate that the startup sequence is complete.

The GPS communications test does not require a satellite position fix, so it can pass while the Status page still shows GPS waiting for a fix. Do not use the logger for a race session if the storage check fails.

## Starting a session

1. Power RaceSync.
2. Allow all startup diagnostics to complete.
3. Give the GPS antenna a clear view of the sky.
4. If desired, connect before going out and confirm GPS and storage are ready.
5. Ride away normally.

Automatic recording starts when valid GPS speed reaches the configured start speed. The default is **10 km/h**.

The onboard LED flashes approximately once per second while recording. It flashes blue when RaceSync initiated the session with a connected GoPro, and green for a data-only session. The separate RPM activity indication can overlap these flashes when enabled.

## While riding — recording has priority

Once recording begins, RaceSync enters race-priority behaviour. The logger and sequential SD recording are given priority over paddock web features.

The RaceChrono BLE GPS feed is also subordinate to recording. A fix is offered
to Bluetooth only after the logger has processed it. RaceSync never waits for
the phone, and a missed live-display update is preferable to delaying an SD
write.

Do not attempt to download, delete or browse sessions while on track. Session listing, VBO download and session deletion are deliberately suspended while recording and return HTTP 423 if requested.

The Status page can still be opened, but while recording it uses a lightweight RAM-only status response. It avoids SD capacity checks and session-directory enumeration. This is intentional and reduces unnecessary SD work during a race.

No rider action is required if GPS temporarily becomes stale or loses a valid fix. RaceSync treats this as an **unknown movement state**, not as the motorcycle being stationary, so a GPS dropout cannot by itself run out the automatic stop timer.

## Ending a session

Return to the paddock and remain at or below **3 km/h** for the configured stop delay. The default is **60 seconds**.

A normal automatic stop requires healthy GPS data showing the bike below the stop threshold for the complete delay. If GPS becomes stale, disconnected or invalid, the stationary timer is cleared. After GPS recovers, the stationary period must begin again.

When the stop delay completes, RaceSync flushes and closes the active `.part` recording, renames it to `.vbo`, writes the final diagnostic summary and returns to `IDLE`.

Confirm that RaceSync is idle before switching off whenever practical.

## Connecting in the paddock

```text
Wi-Fi:    RaceSync
Password: racesync
Address:  http://192.168.4.1
```

| Page | Use |
|---|---|
| Sessions `/` | View/download completed sessions and request KML |
| Control `/control` | Manual logging, automatic settings and reboot |
| Status `/status` | GPS, RPM, storage, logger and reliability diagnostics |
| Camera `/camera` | Manually enable, connect, refresh and disconnect GoPro BLE |

RaceChrono live GPS is configured in the RaceChrono Pro app rather than through
the RaceSync web interface.

Use file-management features after the recording has stopped.

Starting a session download temporarily locks out automatic logging. RaceSync clears any pending movement candidate and will not re-enable automatic start until it has received three seconds of fresh stationary GPS after the transfer. This prevents download time from being mistaken for the two-second movement confirmation.

If an automatic session nevertheless starts while the bike is stationary, open **Control** and select **Stop Logging**. RaceSync accepts this for an automatic session only when GPS is fresh and valid and speed is at or below 3 km/h; it remains protected while the motorcycle may be moving.

## Session files

A normal session produces a VBO recording and a matching diagnostic log:

```text
RS_2026-09-07_10-32-15.vbo
RS_2026-09-07_10-32-15.log
```

The VBO is the primary motorsport data file. The `.log` file is there to explain what happened to the logger during that session if a recording ends unexpectedly.

The log is intentionally low traffic. It records important events rather than writing a second 25 Hz data stream.

Typical events include:

```text
SESSION_START
GPS_STALE
GPS_RECOVERED
STATIONARY_CANDIDATE
MOVEMENT_RESUMED
SESSION_STOP_REQUESTED
```

A completed log records the start/end time, final VBO size, samples written, session duration, stop reason, GPS state at the stop, GPS-dropout count, maximum packet age, invalid-GPS sample count and storage/write-error information.

Typical stop reasons include `STATIONARY_TIMEOUT`, `MANUAL`, `LOW_STORAGE`, `SD_WRITE_ERROR` and `FORCED`.

If a future session unexpectedly stops early, **keep both the VBO and its matching `.log` file**. The log is the first file to inspect when diagnosing the cause.

## Downloading and analysing sessions

After RaceSync is idle, use the Sessions page to download completed VBO recordings. KML is generated only when requested; RaceSync does not continuously create KML files while riding.

The browser's **NEW** indication is local to that browser/device and is not a flag stored in the VBO.

**RaceChrono Pro is the preferred RaceSync analysis application.** RaceSync does not require the circuit or start/finish line to be configured before riding; select or create the correct circuit during import/review.

To import a RaceSync session and external-camera video:

1. Download the completed `.vbo` from the RaceSync Sessions page while the logger is idle.
2. Copy the VBO to the phone or tablet running RaceChrono Pro.
3. In RaceChrono Pro open **Import**, choose the VBOX/VBO file and complete the import. Select the correct circuit/layout if prompted.
4. Copy the camera's original MP4 files to the device. For a recording split into chapters, copy every chapter and keep the original chronological order.
5. Open the imported session, open its **Video** list and choose **Add/Import video**. Select all MP4 chapters belonging to that session.
6. Link the video to the session. Use automatic linking when RaceChrono offers it; otherwise select a clear shared event—passing a junction, leaving pit lane or the first obvious acceleration—and adjust the video time offset until map position, speed and picture agree.
7. Check synchronisation at both the beginning and end. If it drifts, confirm no camera chapter is missing and that the original unedited files were used.
8. Analyse laps using the synchronized map, speed, `rc_rpm`/RPM and custom `throttle` channels. Retain the original VBO and MP4 files even after producing an overlay export.

RaceChrono Pro's menu wording can vary slightly between Android/iOS releases.
Filtered RPM is stored in `Revs` and duplicated in `rc_rpm`; calibrated
throttle opening is stored in `throttle` as 0–100%.

Circuit Tools remains a useful alternative for VBO-only analysis.

## Throttle-position calibration and track use

Calibrate only while RaceSync is idle, with the motorcycle secure and the engine
stopped:

1. Check that the sensor bracket and flexible linkage cannot restrict throttle return.
2. Switch on RaceSync and open `http://192.168.4.1/control`.
3. Confirm the raw ADC value changes smoothly as the throttle is opened.
4. Release the throttle fully and select **Capture Closed Throttle**.
5. Hold the throttle fully open against the carburettor stop without forcing it, then select **Capture Full Throttle**.
6. Release the throttle and verify approximately 0%; reopen it and verify approximately 100%.
7. Repeat calibration after moving the sensor, bracket, carburettors or linkage.

Do not ride if the linkage binds, prevents positive throttle return, or acts as a
throttle stop. A `CHECK WIRING` indication or a value stuck at one ADC rail
means the session may contain invalid throttle data.

At a trackday or race meeting, use throttle alongside speed and RPM to compare:

- where the throttle closes before each braking zone;
- time spent coasting between closing the throttle and braking/turn-in;
- how early and progressively throttle is reopened at corner exit;
- partial-throttle hesitation or repeated corrections;
- whether rising RPM produces matching acceleration in each gear.

Before the first session, perform a closed/full sweep in the paddock. After each
session, download the VBO, import it into RaceChrono Pro, attach the camera files,
and compare a consistent lap with the fastest lap before changing technique or setup.

## Automatic logging settings

Open `http://192.168.4.1/control` while RaceSync is idle.

| Setting | Default | Range |
|---|---:|---:|
| Start recording speed | 10 km/h | 1-100 km/h |
| Stop delay | 60 s | 1-600 s |
| Stop threshold | 3 km/h | Fixed |

Settings survive power-off/reboot. Do not change them while a session is active.

## Manual recording

The Control page provides manual start and stop controls. Manual start requires a valid GPS fix and ready storage. A manually started recording continues until **Stop Logging** is selected; the normal automatic stationary timeout is ignored.

Storage faults or low free space can still end a manual recording. Without valid new GPS samples, no new VBO rows are written even though the session can remain open.

After manual stop, automatic restart is inhibited until speed falls back below the configured start threshold.

## GPS dropout behaviour

RaceSync considers GPS unsuitable for stationary detection if any of these apply:

- receiver is disconnected;
- GPS fix is invalid;
- last packet age exceeds 1000 ms.

In those cases movement is **unknown**. The automatic stationary timer is cleared rather than advanced. When valid fresh GPS returns, normal movement/stationary detection resumes.

The matching diagnostic log records `GPS_STALE` and `GPS_RECOVERED` transitions so intermittent receiver problems can be identified after the session.

## RPM checking and diagnostics

The Status page shows:

| Item | Meaning |
|---|---|
| RPM | Current filtered engine speed |
| Signal | Whether an accepted pulse arrived within the 500 ms timeout |
| Accepted pulses | Accepted GPIO4 falling edges since boot |
| Rejected readings | Evaluated readings above 15,000 RPM |
| Last pulse | Time since the last accepted pulse |
| GPIO4 level | Current sampled input level |

These diagnostics work without starting a VBO recording. The default calibration is two pulses per crankshaft revolution.

If RPM is exactly half or double the motorcycle tachometer, the pulse-per-revolution calibration requires adjustment in the firmware.

A rising rejected-reading count during an RPM dropout suggests an over-range interval. A last-pulse age exceeding 500 ms with `NO SIGNAL` indicates that accepted pulses have stopped arriving. Neither observation by itself proves that the optocoupler is faulty.

RPM diagnostic counters reset on reboot and are not persistent VBO channels.

## RPM blue LED

The **RPM blue LED** option on the Status page controls only the onboard RPM activity indication. It does not disable RPM capture or logging.

Change the option only while idle. The preference is saved and survives reboot. At normal engine speed the 60 ms activity indications can overlap and appear almost continuously blue, so disabling the option is useful when you want the green recording indication to remain obvious.

## Unexpected power loss

The active VBO is stored as `.part` and flushed approximately once per second. If power is lost during recording, the incomplete file remains on the SD card.

At the next successful boot RaceSync scans interrupted `.part` recordings. It keeps complete newline-terminated VBO records, discards a potentially incomplete final row, checks the required VBO sections and publishes valid recovered data as a `.vbo` file. A name such as `_RECOVERED_1.vbo` is used if necessary.

An invalid or empty interrupted file is retained as `.part` for investigation rather than silently deleted.

Recovery cannot recreate samples that had not reached the card before power disappeared.

A diagnostic `.log` belonging to a power-interrupted session may not contain its normal final summary; that absence is itself useful evidence that the session did not close normally.

## Storage safeguards

At boot, RaceSync tests that the microSD can create, read back and delete a temporary file. Logging is unavailable if the storage test fails.

A session will not start with less than 1 MB free. RaceSync also protects the reserve during recording and can close the session with a low-storage stop reason.

Completed sessions are exposed only after the active `.part` has been successfully finalized to `.vbo`.

## Checking the installed firmware

Open the Status page. The release version, Git-derived build number and abbreviated
commit are displayed below **RaceSync Device Status**, including while recording.
The same values are available from `/api/status` and the lightweight
`/api/runtime` endpoint.

A value such as `V2.1 · build 184 · a1b2c3d4` identifies the exact source used
for the installed firmware. A commit ending in `-dirty` was built with local
tracked changes that had not been committed.

## Rebooting

Use **Reboot RaceSync** from the Control page only while idle. Wi-Fi disappears briefly and returns after the ESP32 restarts.

The last-session diagnostic values shown in `/api/status` are held in RAM and therefore reset on reboot. The `.log` file on the SD card is the persistent record.

## Race-day checklist

### Before going out

1. Insert the FAT32 microSD card and power RaceSync.
2. Check the startup sequence, especially storage check 2.
3. Obtain an outdoor GPS fix.
4. Confirm GPS and storage are ready if using the browser pre-session.
5. Check RPM against the bike tachometer.
6. Sweep the throttle and confirm the display moves smoothly from approximately 0% to 100%.
7. If video is required, power the paired GoPro and confirm **GoPro connected — not recording** on the Camera page.
8. Confirm blue logging flashes after the session starts; green means RaceSync is logging without an initiated camera recording.
9. Leave file downloads and other session management until after the race.

### On track

1. Ride normally; RaceSync requires no interaction.
2. Do not use the web interface for file management.
3. Temporary GPS loss should not terminate the recording through the stationary timer.

### Back in the paddock

1. Remain stopped until the logger returns to `IDLE`.
2. Keep RaceSync powered while downloading.
3. Join `RaceSync` Wi-Fi and open `192.168.4.1`.
4. Download the new VBO.
5. If anything unusual happened, retain and inspect the matching `.log` file.
6. Import the VBO into RaceChrono Pro and add the matching camera file(s).

## Troubleshooting

| Symptom | What to check |
|---|---|
| Red storage check | FAT32 card, SD 5 V supply, common ground and SPI wiring |
| Red GPS communications check | GPS power, TX/RX wiring and receiver output |
| GPS WAITING | Move outdoors and give the antenna a clear view of the sky |
| Recording does not start | Valid GPS fix, storage state, free space and configured start speed |
| Session listing/download says suspended or HTTP 423 | RaceSync is recording; wait until it returns to `IDLE` |
| Session stopped unexpectedly | Preserve the VBO and matching `.log`; inspect stop reason, GPS events and write errors |
| RPM is zero | Check accepted pulses, last-pulse age, rejected readings, optocoupler and GPIO4 wiring |
| RPM is half/double | Correct the pulse-per-revolution calibration |
| Throttle shows CHECK WIRING | Check 3.3 V, ground, signal continuity and ensure the output is not pinned to a supply rail |
| Throttle does not reach 0/100% | Repeat closed/full calibration and inspect the sensor linkage |
| Logging flashes green when video was expected | Confirm the GoPro was powered, wireless was enabled, and the Camera page showed it connected before the session started |
| RPM activity obscures the logging flash | Disable the RPM blue LED while idle |
| Session missing after power loss | Reboot with SD fitted and inspect `storage.recovery` on Status |
| Settings/reboot unavailable | Wait for the active recording to stop |
