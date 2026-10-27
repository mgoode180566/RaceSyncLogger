# GPS lap timing

RaceSync writes completed GPS lap times beside each VBO session. This is intended
for comparison with RaceChrono Pro before using live timing on a display.

## Files on the SD card

- `/circuits.json`: editable circuit database. The firmware creates this file on
  first boot if absent, using the example in `sdcard/circuits.json`. Existing files
  are never overwritten. No filesystem upload is needed to seed it.
- `RS_<session>_laps.csv`: one row per completed lap; created for every new session,
  even if no circuit can be selected. Header-only means no completed laps.
- `RS_<session>_laps.json`: session filename, algorithm version, configuration
  validity/error and a snapshot of the circuit database used for that session.
  CSV `circuit_id` selects the corresponding entry in the snapshot.

The VBO and diagnostic log continue as before. Lap file errors disable lap output
and appear in `/api/runtime` under `lapTiming`; they do not stop the VBO logger.
Completed lap rows are flushed individually. Power loss can still lose/corrupt an
SD write in progress; this is not a transactional storage system. Incomplete final
laps and out-laps are not written as completed laps. No historical sessions are
modified or recalculated automatically.

## Circuit configuration

Copy `sdcard/circuits.json` to the card root if preparing the card yourself. Edit it
with recording stopped and the card safely removed. The firmware reloads it at
session start, then holds a fixed copy until that session ends.

Coordinates are WGS84 decimal **latitude, longitude**, as copied from Google Maps.
The two endpoints are used exactly as supplied, without centring or extending the
line. Cadwell's initial gate uses:

| Endpoint | Latitude | Longitude |
| --- | --- | --- |
| 1 | 53.310286 | -0.059360 |
| 2 | 53.310344 | -0.059583 |

Add circuits to the array with unique `id` values. Version 1 supports at most 32
circuits in a 32 KiB file. Each entry needs string `id`, `name`, `layout` and two
numeric `startFinish` endpoint objects. The settings below have firmware defaults
when omitted (or null), except `selectedCircuitId`, which is a required string:

| Setting | Default | Purpose |
| --- | --- | --- |
| `selectedCircuitId` (top level) | `""` | Automatic proximity selection; set a circuit ID for overlapping layouts. |
| `identificationRadiusMetres` | 1500 | Maximum distance from gate reference for selecting that circuit. |
| `minimumLapSeconds` | 60 | Reject early repeated crossings. Tune for each layout. |
| `minimumSpeedKmh` | 10 | Minimum reported speed at crossing. |
| `maximumSpeedKmh` | 300 | Reject excessive reported speed and implausible position jumps. |
| `maximumGapMs` | 1000 | Maximum interval between consecutive fixes. |
| `rearmDistanceMetres` | 10 | Move this far away from the gate's infinite line before rearming. |
| `crossingHeadingDegrees` | `null` | Learn direction from first crossing; optionally set expected compass heading (north 0, east 90). |

Automatic selection chooses the nearest gate within its identification radius.
If the two closest candidates differ in distance by less than 100 m, timing waits
for an unambiguous position or an explicit `selectedCircuitId`. Selection is
locked for the session. Explicit selection also requires proximity; it does not
start Cadwell timing at another circuit. Circuit identification uses the gate
midpoint only as a distance reference; neither endpoint is changed.

With heading null, the **first valid crossing** establishes direction. A wrong-way
first crossing can therefore learn the wrong direction; set a heading where this
is possible. A configured heading accepts crossings within 90 degrees of that
heading, as well as checking consistency with the first accepted direction.
Choose a gate away from pit-lane crossings and neighbouring sections of track.

Malformed, unsupported or invalid configuration disables timing for that session,
with an error exposed by `/api/runtime`. It is preserved for you to correct rather
than silently replaced. An empty valid circuit array is allowed and yields no laps.

## Timing behaviour

1. A valid GPS fix with UTC time and at least six satellites selects a nearby circuit.
2. RaceSync checks the finite movement segment between consecutive GPS fixes
   against the finite start/finish segment. Crossing the line's extension does not count.
3. It interpolates crossing time along the movement segment using GPS UTC
   measurement timestamps, rounded to milliseconds. ESP32 arrival time is not used.
4. The first accepted crossing starts timing. Every later accepted crossing after
   the minimum lap interval completes one lap and starts the next at the same instant.
5. The vehicle must leave the gate area before the detector rearms. Reverse
   crossings and early recrossings are ignored.
6. Invalid fixes, stale GPS health, time reversals, long time gaps or position jumps
   discard the current incomplete lap and break position continuity. The next
   accepted crossing starts a fresh lap; previously saved laps remain intact.
   Duplicate timestamps are ignored. Midnight/date changes and signed UBX nano
   fractions are handled using full UTC dates.

Lap numbers count only completed valid laps. After GPS loss, they may differ from
RaceChrono's lap numbers; use the UTC start/end timestamps to match laps.
Millisecond output is a calculation resolution, not a promise of millisecond GNSS accuracy.

## Downloads and comparison

The Sessions page provides **Download Laps CSV** and **Lap Details JSON** beside
sessions that have these files. The API also exposes:

- `GET /api/sessions/{id}/laps`
- `GET /api/sessions/{id}/lap-metadata`

Session lists include `hasLaps`, `lapsDownloadUrl` and `lapMetadataDownloadUrl`.
Downloads are blocked while recording, just like VBO transfers. Archiving a session
moves both lap companions with its VBO, log and optional KML; nothing is deleted.

CSV columns:

```csv
lap,circuit_id,start_utc,end_utc,lap_time_seconds,start_utc_ms,end_utc_ms
```

UTC strings include the date and milliseconds; integer epoch columns provide exact
millisecond values. Match completed laps by their start/end times when comparing.
Import the **same VBO recording** into RaceChrono and configure its start/finish
gate using the **same two endpoints**. A different gate, different GPS recording,
filtering, or crossing algorithm can produce different lap times. In particular,
the seeded Google Maps gate differs from the earlier gate exported by RaceChrono;
update RaceChrono's gate before expecting close comparisons.

This version records full laps only. Sector timing and a live lap display are not
implemented by this change. Other circuit metadata can be added to JSON and will
be retained in the session snapshot.

## Validation

Run the native geometry/timing regression tests without a board:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic src/laptiming/LapTimer.cpp src/laptiming/UtcTime.cpp tests/lap_timer_test.cpp -o /tmp/lap-tests
/tmp/lap-tests
g++ -std=c++11 -Wall -Wextra -Werror -pedantic src/laptiming/LapTimer.cpp src/laptiming/UtcTime.cpp src/laptiming/CircuitSelector.cpp tests/lap_modules_test.cpp -o /tmp/lap-module-tests
/tmp/lap-module-tests
python3 tests/check_circuits.py
```

CI runs these tests and builds both supported PlatformIO environments. These tests
cover interpolated crossings, complete laps, finite gates, wrong direction,
duplicate/exact-on-line samples, minimum lap time, GPS loss/gaps/jumps, UTC rollover,
invalid configuration and preservation of the supplied Cadwell endpoints.
Hardware SD behaviour and actual Cadwell lap agreement still require on-bike or
recorded-session validation.

## Module boundaries

The logger still uses the same `RaceSyncLapTiming` API. That class coordinates
session start/stop and routes samples and completed laps between these modules:

| Module | Responsibility | Dependencies |
| --- | --- | --- |
| `LapTimer` | Finite gate crossings, GPS continuity, direction/rearm rules, interpolated timestamps and lap state | Standard C++ only |
| `CircuitSelector` | Nearest circuit, identification radius, overlapping layouts and explicit preference | Standard C++ and core position helpers |
| `UtcTime` | GPS date/fraction conversion and UTC string formatting | Standard C++ only |
| `GpsSampleAdapter` | Convert RaceSync telemetry to the timer's sample model | Telemetry and UTC conversion |
| `CircuitDatabase` | Seed/load/validate SD JSON; retain definitions and the session snapshot | Storage and ArduinoJson |
| `LapSessionWriter` | CSV rows, JSON snapshots, flushing, closing and write-error reporting | Storage, ArduinoJson and UTC formatting |
| `RaceSyncLapTiming` | Session lifecycle, locked circuit selection, sample/event routing and status | The modules above |

The timer has no knowledge of the SD card, JSON, CSV or RaceSync telemetry.
The writer has no knowledge of GPS crossing detection or circuit selection.
The database does not select a circuit or create lap output files. No module
changes the two configured gate endpoints.

Native regression tests exercise the timing state machine, selection policy and
UTC conversion/formatting independently of the ESP32. Both firmware builds check
that the storage and telemetry adapters still integrate with the existing logger.
The circuit JSON schema, session filenames, CSV columns, metadata format, API
routes and download controls remain unchanged by this refactoring.
