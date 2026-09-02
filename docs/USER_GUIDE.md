# RaceSync Data Logger — User Guide

## What RaceSync does

RaceSync records GPS and engine RPM automatically while the motorcycle is moving. It saves each completed ride as a VBO file on microSD. In the paddock, connect a phone, tablet or laptop directly to RaceSync to download the recording or generate a KML route.

Normal use is: **power it on, check it, ride, wait for it to stop, then download the session.** No controls are needed while riding.

## Before first use

- Fit a FAT32-formatted microSD card.
- Power the tested SD reader/writer from 5 V and ensure it shares ground with the ESP32.
- Mount the GPS antenna with a clear upward view, away from metal shielding where practical.
- Confirm the ECU tachometer connection is isolated; the 12 V motorcycle signal must never connect directly to the ESP32.
- Set the desired start speed and stop delay on the web control page.

## Startup lights

The LED is solid red when startup diagnostics begin. RaceSync then signals five checks. The number of flashes identifies the check; green flashes mean pass and red flashes mean fail.

| Flashes | Check |
|---:|---|
| 1 | RaceSync Wi-Fi |
| 2 | microSD card and storage health |
| 3 | logger initialization |
| 4 | GPS receiver communications |
| 5 | sensor subsystem, including RPM input |

Five blue flashes mean the full diagnostic sequence has finished.

The GPS check confirms that valid receiver packets arrive within five seconds. It does not require a satellite position fix. You may therefore see a green GPS diagnostic while the web page still says **GPS WAITING** indoors.

Do not rely on the logger for a track session if check 2 is red. Review any other red result through the Status page or serial monitor before riding.

## Starting a track session

1. Power RaceSync.
2. Let all five startup checks and the final five blue flashes complete.
3. Give the GPS antenna a clear view of the sky.
4. If desired, connect to RaceSync and confirm **GPS READY** and **STORAGE READY**.
5. Ride away normally.

Automatic recording starts when valid GPS speed reaches the configured start speed. The default is 10 km/h.

While recording, the onboard LED gives a short green flash approximately once per second. RaceSync continues through slow corners and brief stops.

## Ending a track session

Return to the paddock and remain at or below 3 km/h for the configured stop delay. The default delay is 60 seconds.

When the delay expires, RaceSync flushes and closes the recording and changes it from an incomplete `.part` file into a completed `.vbo` session. The recording LED then stops flashing.

Wait for the flashing to stop before switching off whenever possible.

## Connecting to RaceSync

RaceSync creates its own Wi-Fi network. It does not need mobile data, internet access or circuit Wi-Fi.

```text
Wi-Fi:    RaceSync
Password: racesync
Address:  http://192.168.4.1
```

After connecting, open `http://192.168.4.1` in a browser.

| Page | Use |
|---|---|
| Sessions `/` | Download, generate KML, or delete completed sessions |
| Control `/control` | Manual logging, automatic settings, and reboot |
| Status `/status` | Detailed GPS, storage, logger, and recovery information |

## Downloading sessions

The Sessions page lists completed VBO recordings newest first.

- **Download VBO** downloads the primary motorsport data file.
- **Generate KML** creates a route from the completed VBO and downloads it immediately.
- **Download All New** downloads all VBO files not previously downloaded by this browser.
- **Delete** removes a completed session from the card after confirmation.

The **NEW** marker is remembered by the browser, not by RaceSync. The same session may appear new again on another phone, computer or browser.

RaceSync does not save KML files during recording. A KML is generated only when requested and is not stored on the microSD card.

## Using the VBO file

VBO is the main RaceSync data file. It contains GPS position, speed, heading, altitude, timing, and available sensor channels. Engine RPM is recorded in the `Revs` channel.

Open the VBO in compatible motorsport analysis software such as Circuit Tools. RaceSync does not need a circuit selected before riding; circuit recognition, start/finish detection, and lap analysis happen afterwards.

## Changing automatic logging settings

Open `http://192.168.4.1/control` while RaceSync is idle.

| Setting | Default | Allowed range |
|---|---:|---:|
| Start recording speed | 10 km/h | 1–100 km/h |
| Stop delay | 60 seconds | 1–600 seconds |
| Stop threshold | 3 km/h | Fixed |

Choose the values and press **Save Logging Settings**. They remain saved after power-off or reboot. Settings cannot be changed during recording.

## Manual recording

The Control page provides **Start Manual Logging** and **Stop Logging**.

Manual start is available only when GPS has a valid fix and storage is ready. A manually started recording ignores the automatic stop delay and continues until **Stop Logging** is pressed.

After a manual stop, automatic recording will not immediately start again while the motorcycle is above the configured start speed. It is re-enabled after speed falls below that threshold.

## Rebooting RaceSync

Use **Reboot RaceSync** on the Control page only while the logger is idle. The control is disabled during recording. Wi-Fi disappears briefly; reconnect when the `RaceSync` network returns.

## Unexpected loss of power

RaceSync writes an active session as `.part` and flushes it every second. If power is removed during recording, that incomplete file remains on the card.

At the next boot, after the SD health test passes, RaceSync automatically checks `.part` files. It keeps complete VBO rows, discards a possibly incomplete last row, validates the recovered session, and publishes it as a VBO. If that name already exists, the recovered filename includes `_RECOVERED_1`, `_RECOVERED_2`, and so on.

An invalid or empty interrupted recording remains as `.part` for investigation rather than being silently deleted. Such files do not appear in the normal completed-session list.

Open the Status page and look at `storage.recovery` to see how many files were recovered or failed and the last recovered filename.

Recovery greatly reduces data loss, but it cannot restore a sample that had not reached the microSD card when power disappeared. A normal stop remains safest.

## Storage safeguards

At every boot, RaceSync mounts the microSD card and tests that it can create, read back, and delete a temporary file. Logging is unavailable if that test fails.

RaceSync will not start a session with less than 1 MB free. It also checks free space during recording and closes the session if remaining space becomes too low.

The active recording cannot be deleted through the web interface. Completed sessions appear only after the active `.part` file has been closed and successfully renamed to `.vbo`.

## Typical track-day checklist

### Before going out

1. Insert the FAT32 microSD card and power RaceSync.
2. Watch all startup diagnostics; pay particular attention to storage check 2.
3. Confirm the diagnostic sequence ends with five blue flashes.
4. Obtain an outdoor GPS fix.
5. If checking through the browser, confirm **GPS READY** and **STORAGE READY**.

### On track

1. Ride normally; no RaceSync interaction is required.
2. If visible, confirm the short green recording flash repeats approximately once per second.

### Back in the paddock

1. Stay stopped until the recording light ends.
2. Leave RaceSync powered while downloading.
3. Join `RaceSync` Wi-Fi and open `192.168.4.1`.
4. Download the new VBO and generate a KML only if wanted.
5. Open the VBO in Circuit Tools or your preferred compatible software.
6. Delete old sessions when they are no longer required on the logger.

## Troubleshooting

| Symptom | What to check |
|---|---|
| Red check 2 | Card fitted, FAT32 format, SD-module 5 V supply, common ground, and SPI wiring |
| Red check 4 | GPS power, TX/RX wiring, and valid receiver output |
| GPS WAITING | Move outdoors and give the antenna a clear view of the sky |
| Recording does not start | GPS fix, storage state, free space, and configured start speed |
| RPM is zero | Optocoupler power, isolated output wiring to GPIO4, and ECU connection |
| RPM is half or double | Pulse-per-revolution calibration needs adjustment before track use |
| Session absent after power loss | Reboot with the card fitted, then check `storage.recovery` on Status |
| Cannot change settings, reboot, or delete | Wait until the active recording has stopped |
