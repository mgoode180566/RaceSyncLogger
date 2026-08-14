# RaceSync V1

RaceSync is a simple, resilient motorcycle telemetry logger.

## V1 specification

- ESP32-S3
- GPS
- IMU
- analogue throttle position
- microSD
- ignition powered
- power LED
- GPS/status LED
- automatic session start from GPS speed
- automatic session stop after configurable stationary period
- VBO output
- Wi-Fi and REST API only while idle
- NVS for configuration
- watchdog
- periodic SD flush
- no cloud dependency
- no Raspberry Pi

## Default configuration

- Start speed: 10 km/h
- Stop speed: 3 km/h
- Stop delay: 30 seconds
- IMU logging: 100 Hz
- SD flush: 250 ms
- Wi-Fi idle period: 5 minutes

## REST API

While idle:

GET /api/status
GET /api/config
PUT /api/config
GET /api/sessions

Example:

PUT /api/config

{
  "startSpeedKph": 10,
  "stopSpeedKph": 3,
  "stopDelaySeconds": 30,
  "imuRateHz": 100,
  "sdFlushMs": 250
}

Configuration is rejected while logging.

## VBO files

Files are:

/LOGS/SESSION_1.VBO
/LOGS/SESSION_2.VBO
...

The file contains VBO-style sections and telemetry columns for:

- satellites
- time
- latitude
- longitude
- velocity
- heading
- height
- accelerometer X/Y/Z
- gyro X/Y/Z
- throttle

The existing user's VBO application is the definitive compatibility test.

## Throttle sensor

The analogue input is deliberately isolated in two constants:

TPS_CLOSED_ADC
TPS_OPEN_ADC

These must be calibrated with the actual sensor and electrical interface.

Do not connect a sensor output above the ESP32 ADC voltage limit directly to the GPIO.

## Important first tests

Before track use:

1. Test GPS acquisition.
2. Test IMU readings.
3. Calibrate throttle.
4. Test SD recording.
5. Start a stationary session by temporarily lowering start speed.
6. Open the resulting VBO in the existing application.
7. Test abrupt ignition/power removal.
8. Confirm the resulting VBO remains usable.
9. Repeat power interruption at different points in a recording.
10. Only then install the logger on the motorcycle.

## Future RaceSync versions

V2 can add cloud upload.

The ESP32 should not need to know anything about AWS. Its job is reliable acquisition and local VBO storage.

A future backend can upload the original VBO to cloud storage and extract telemetry for the React application.
