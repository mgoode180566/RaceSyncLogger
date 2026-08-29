# RaceSync Data Logger — User Guide

## Overview

RaceSync is a compact motorcycle data logger designed to record riding and race sessions automatically, without requiring the rider to operate controls while on track.

Once powered on, RaceSync uses GPS to determine when the motorcycle is moving, records the session to a microSD card, and makes the recorded files available over its own Wi-Fi network when you return to the paddock.

The system is designed to be fitted to the motorcycle and largely forgotten about while riding.

## What RaceSync Records

RaceSync uses a high-speed GPS receiver to record the motorcycle's position and movement at up to **25 samples per second**.

A session can contain:

- GPS position
- Speed
- Direction of travel
- Altitude
- GPS satellite information
- Time and sample timing
- Engine RPM, when the RPM input is fitted and configured

RaceSync creates a **VBO data file** for motorsport analysis and a matching **KML track file** for viewing the route on mapping software.

## Automatic Recording

RaceSync does not require the rider to press a start or stop button.

When powered and receiving valid GPS data, it waits for the motorcycle to begin moving. Recording starts automatically when the configured starting speed is exceeded.

RaceSync continues recording throughout the session, including when the motorcycle slows or stops briefly. After the motorcycle remains below the stopping threshold for the configured period, RaceSync automatically closes and saves the recording.

The rider can therefore leave the pits, complete a race or track session, return to the paddock and allow RaceSync to save the complete run automatically.

## Logging Indicator

The onboard LED provides a simple indication that RaceSync is recording.

When RaceSync is idle, the logging indicator is off. During active recording, the LED gives a short **flash approximately once per second**.

This provides a quick visual confirmation without requiring a display or controls on the motorcycle.

## GPS Operation

RaceSync uses a high-rate GPS receiver capable of recording position data at **25 Hz**.

After power-up, the GPS receiver needs a sufficiently clear view of the sky to obtain a position fix. For best results, the GPS antenna should have a clear view of the sky and should not be hidden beneath metal bodywork.

Once a valid GPS signal is available, RaceSync can automatically begin recording when the motorcycle moves.

## Session Storage

Sessions are stored on the RaceSync **microSD card**.

Each RaceSync recording is stored as a pair of files, for example:

```text
RS_20260829_103215.vbo
RS_20260829_103215.kml
```

The two files represent the same session. The microSD card can hold many sessions, so recordings do not normally need to be removed after every outing.

## VBO Files

The `.vbo` file is the main RaceSync data file. It contains the high-rate GPS and vehicle data recorded during the session and is intended for use with compatible motorsport data-analysis software such as **Circuit Tools**.

It can be used to analyse information such as lap times, speed around the circuit, braking and acceleration areas, racing lines, GPS position and engine RPM when available.

RaceSync itself does not need to know which circuit is being used. It records the session and allows the analysis software to identify the circuit and perform lap analysis afterwards.

## KML Files

RaceSync creates a `.kml` file alongside each VBO recording.

The KML contains the GPS track and provides a quick way to see where the motorcycle travelled. It can be opened with compatible mapping applications such as **Google Earth**.

This is useful for visually checking that the complete route was recorded and for reviewing the GPS track.

## Connecting to RaceSync

RaceSync creates its own Wi-Fi network. It does not require internet access, a mobile connection or circuit Wi-Fi.

When back in the paddock:

1. Leave RaceSync powered on.
2. Connect a laptop, tablet or phone to the **RaceSync** Wi-Fi network.
3. Open the RaceSync interface in a web browser.
4. View the recorded sessions.
5. Download the required files.

The standard RaceSync device address is:

```text
192.168.4.1
```

Because the connection is directly between your device and RaceSync, it can be used even where internet connectivity is unavailable.

## Downloading Sessions

RaceSync provides access to recordings stored on the microSD card through its Wi-Fi connection.

Each session has a unique session ID. The VBO file can be downloaded for detailed motorsport analysis, while the matching KML can be downloaded for viewing the GPS route.

This allows data to be transferred without removing the microSD card. The card can still be removed and read directly by a computer if required.

## Typical Track-Day Workflow

A normal RaceSync session is designed to be simple:

1. Power the motorcycle and RaceSync.
2. Allow the GPS receiver to obtain a fix.
3. Ride out of the paddock.
4. RaceSync automatically starts recording.
5. The onboard LED flashes once per second while logging.
6. Complete the track session normally.
7. Return to the paddock.
8. RaceSync automatically stops and safely closes the recording.
9. Connect a laptop or other device to the RaceSync Wi-Fi.
10. Download the VBO and/or KML files.
11. Open the VBO in your preferred motorsport analysis software.

No interaction with RaceSync should be necessary while riding.

## Powering Off

For the safest recording, allow RaceSync to finish the session before removing power.

Once the motorcycle has stopped and the logging indicator has stopped flashing, the current recording has been closed.

RaceSync also periodically writes information to the microSD card during a session to reduce the amount of data at risk if power is unexpectedly lost.

## Using RaceSync at Different Circuits

RaceSync is not tied to a particular race circuit and there is no need to select the circuit before riding.

The logger records GPS and vehicle data wherever it is used. The resulting VBO file can then be opened in compatible analysis software for circuit and lap analysis.

This allows the same RaceSync unit to be used at different circuits without reconfiguring it for each event.

## What the Rider Needs to Do

RaceSync is designed so the rider's normal interaction is limited to:

**Before riding:** Power the system and confirm GPS operation.

**While riding:** Nothing.

**After riding:** Wait for logging to finish, connect to RaceSync Wi-Fi and download the session.

The aim is to provide useful race and track data without adding another distraction to the motorcycle.
