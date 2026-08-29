# RaceSync Data Logger — User Guide

## Overview

RaceSync is a compact motorcycle data logger designed to record track and race sessions automatically without requiring the rider to operate controls while riding.

Once powered, RaceSync acquires GPS, detects when the motorcycle begins moving, records the session to microSD and saves files ready for analysis. Back in the paddock, the rider connects directly to RaceSync over Wi-Fi and uses the built-in web interface to download or manage the recordings.

The normal intention is simple: **power it on, ride, download your sessions afterwards.**

## What RaceSync records

RaceSync uses a high-speed GPS receiver operating at up to **25 samples per second**. A session records GPS position, speed, direction, altitude, satellite/fix information and accurate sample timing.

RaceSync is designed to accept additional motorcycle channels as the system develops, including engine RPM and IMU data.

Each completed RaceSync session normally produces two files:

```text
RS_20260829_103215.vbo
RS_20260829_103215.kml
```

The files belong to the same riding session.

## Automatic recording

RaceSync does not require a start/stop button.

With a valid GPS signal, the logger waits for the motorcycle to move. Recording begins automatically when the configured start speed is exceeded. It continues through the riding session, including slow corners and brief stops.

After the motorcycle has remained below the stopping threshold for the configured period, RaceSync safely closes the session.

Current settings are:

```text
Start recording: 10 km/h
Stop threshold:   3 km/h
Stop delay:       60 seconds
```

This allows the rider to leave the paddock, complete the track session and return without interacting with the logger.

## Recording indicator

The onboard RaceSync LED provides a simple indication of recording status.

While a session is actively being recorded, the LED gives a short flash approximately **once per second**. When RaceSync is idle, the logging indication is off.

This allows recording to be confirmed without fitting a screen to the motorcycle.

## GPS operation

RaceSync uses a high-rate GPS receiver capable of **25 Hz** position logging.

After power-up the GPS needs a sufficiently clear view of the sky to obtain a position fix. The antenna should ideally have a clear upward view and should not be hidden beneath metal bodywork.

Once GPS is ready, normal recording is automatic.

## Session storage

Sessions are stored on the RaceSync **microSD card**. The card can hold many recordings, so sessions do not need to be removed after every outing.

The VBO and KML files with the same base filename are treated as one RaceSync session. When a session is deleted through the RaceSync interface, the VBO and its matching KML are removed together when both are present.

## VBO files

The `.vbo` file is the main motorsport data file. It is intended for compatible motorsport analysis software such as **Circuit Tools**.

The recorded data can be used for analysis such as lap times, speed around the circuit, braking and acceleration areas, racing line and GPS position. Additional channels such as RPM and IMU information can be included as those sensors are added to RaceSync.

RaceSync does not need the rider to select a circuit before riding. The logger records the session and the analysis software performs circuit/lap analysis afterwards.

## KML files

A `.kml` GPS track is created alongside each normal RaceSync VBO recording.

The KML provides a quick visual representation of the route and can be opened in compatible mapping software such as **Google Earth**.

## Connecting in the paddock

RaceSync creates its own Wi-Fi network. It does not require mobile data, internet access or circuit Wi-Fi.

Default connection details are:

```text
Wi-Fi:    RaceSync
Password: racesync
Address:  192.168.4.1
```

After returning to the paddock:

1. Leave RaceSync powered on.
2. Connect your phone, tablet or laptop to the **RaceSync** Wi-Fi network.
3. Open a web browser.
4. Enter `http://192.168.4.1`.
5. The RaceSync session manager appears.

## RaceSync session manager

The web interface is built into RaceSync itself. Nothing needs to be installed on the phone or computer.

The main screen shows the status of RaceSync and the sessions currently stored on the logger. It is designed primarily for quickly getting the latest recordings off the motorcycle after a track session.

For each stored session the interface provides the appropriate actions:

- **Download VBO** — download the main motorsport data file.
- **Download KML** — download the GPS track when one exists.
- **Delete** — remove the stored session from RaceSync.

The interface also displays storage information so the available space can be checked before further riding.

## New sessions

The RaceSync web interface highlights sessions that have not previously been downloaded using the current browser as **NEW**.

This makes it easy to identify the sessions just recorded rather than having to remember filenames.

The **Download All New** function downloads the new VBO sessions in one operation. After a successful download, the browser remembers the session ID and no longer marks that session as NEW.

This download history is stored in the browser, not on the motorcycle. Consequently, connecting with a different laptop, phone or browser creates a separate download history and may show the stored sessions as new again.

## Deleting sessions

A session can be removed using its **Delete** control. RaceSync asks for confirmation before deletion.

Deleting a normal completed session removes the VBO and its companion KML when present. RaceSync protects files that should not be removed, such as an active recording.

Deleting old sessions is useful for keeping the SD card tidy, but it is not necessary to delete a recording immediately after downloading it.

## Typical track-day workflow

1. Power the motorcycle and RaceSync.
2. Allow the GPS to obtain a fix.
3. Ride out of the paddock.
4. RaceSync starts recording automatically.
5. Confirm the recording LED is flashing if it is visible.
6. Complete the track session normally.
7. Return to the paddock.
8. Allow RaceSync to stop and close the recording.
9. Connect your laptop, tablet or phone to RaceSync Wi-Fi.
10. Open `192.168.4.1`.
11. Look for the **NEW** session.
12. Use **Download VBO**, **Download KML**, or **Download All New**.
13. Open the downloaded VBO in your motorsport analysis software.
14. Delete old sessions from RaceSync when no longer required on the logger.

No interaction with RaceSync should be necessary while riding.

## Powering off

For the safest recording, allow RaceSync to finish the session before removing power.

Once the motorcycle has stopped for the configured period and the recording indicator has stopped flashing, the session should have been closed normally.

RaceSync periodically writes data to storage during recording to reduce the amount of data at risk if power is unexpectedly removed, but normal shutdown after the session remains preferable.

## Using RaceSync at different circuits

RaceSync is not tied to a particular circuit and there is no circuit selection required before riding.

The same unit can therefore be used at different tracks without changing the logger setup. RaceSync records the GPS/vehicle data and compatible analysis software handles circuit recognition and lap analysis afterwards.

## What the rider needs to do

**Before riding:** Power RaceSync and confirm that the system/GPS is ready.

**While riding:** Nothing. RaceSync automatically handles recording.

**After riding:** Allow the session to finish, connect to RaceSync Wi-Fi, open `192.168.4.1` and download the new recording.

RaceSync is intended to provide useful motorsport data while remaining as unobtrusive as possible on the motorcycle.
