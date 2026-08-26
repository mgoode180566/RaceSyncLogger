# RaceSync V2.1 Modular

ESP32-S3 motorcycle data logger firmware.

## Current features

- MG-902 UART GPS support
- UBX NAV-PVT parser
- Automatic LIVE / DEMO source selection
- Demo replay from any non-`RS_*.vbo` file in LittleFS
- Dynamic session discovery
- Session download API
- Speed-triggered logging
- Detailed `/api/status` diagnostics
- NVM/LittleFS storage abstraction ready to switch to SD later
- Placeholder sensor module for IMU/TPS/brake pressure

## REST API

- `GET /api/status`
- `GET /api/location`
- `GET /api/telemetry`
- `GET /api/sessions`
- `GET /api/sessions/{filename}`

## Demo data

Place one or more `.vbo` files in the `data/` directory and upload the filesystem:

```powershell
C:\Users\Dell\.platformio\penv\Scripts\platformio.exe run -t uploadfs --upload-port COM3
```

Then upload firmware:

```powershell
C:\Users\Dell\.platformio\penv\Scripts\platformio.exe run -t upload --upload-port COM3
```

The first non-`RS_*.vbo` file found in LittleFS is used for demo replay when no physical GPS is detected.

## Project structure

```text
src/
├── api/
├── app/
├── config/
├── gps/
├── logging/
├── sensors/
├── sim/
├── wifi/
└── main.cpp
```

## Production SD transition

The REST API and logger use `RaceSyncStorage`.

For production, replace the LittleFS implementation inside `RaceSyncStorage` with an SD-backed implementation while leaving the rest of the application unchanged.
