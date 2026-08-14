#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <SD.h>
#include <SPI.h>
#include <esp_task_wdt.h>
#include <time.h>

/*
 RaceSync V1
 Motorcycle GPS / IMU / TPS logger

 Default assumptions:
 - ESP32-S3
 - u-blox GPS on UART
 - MPU6050 on I2C
 - analogue throttle position sensor
 - microSD on SPI
 - Wi-Fi/REST only while IDLE
 - logging starts from GPS speed
 - logging stops after configurable stationary period
 - VBO written continuously to SD

 IMPORTANT:
 Change PIN_* values for the actual board.
 Calibrate TPS_CLOSED_ADC / TPS_OPEN_ADC after installation.
*/

namespace Pin {
  constexpr int GPS_RX   = 16;
  constexpr int GPS_TX   = 17;

  constexpr int I2C_SDA  = 8;
  constexpr int I2C_SCL  = 9;

  constexpr int TPS_ADC  = 1;

  constexpr int SD_CS    = 10;
  constexpr int SD_SCK   = 12;
  constexpr int SD_MISO  = 13;
  constexpr int SD_MOSI  = 11;

  constexpr int LED_POWER = 2;
  constexpr int LED_GPS   = 3;
}

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;
Adafruit_MPU6050 imu;
WebServer server(80);
Preferences prefs;

enum class State {
  BOOT,
  IDLE,
  LOGGING,
  ERROR
};

State state = State::BOOT;

struct Config {
  float startSpeedKph = 10.0f;
  float stopSpeedKph = 3.0f;
  uint32_t stopDelaySeconds = 30;
  uint32_t imuRateHz = 100;
  uint32_t sdFlushMs = 250;
  uint32_t wifiTimeoutSeconds = 300;

  char ssid[33] = "RaceSync";
  char password[65] = "racesync123";
};

Config cfg;

File vboFile;
String currentFile;
uint32_t sessionNumber = 0;
uint32_t lastSampleMs = 0;
uint32_t lastFlushMs = 0;
uint32_t stationarySinceMs = 0;
uint32_t wifiStartedMs = 0;

bool sdOk = false;
bool imuOk = false;

constexpr int TPS_CLOSED_ADC = 620;
constexpr int TPS_OPEN_ADC   = 2450;

struct Sample {
  uint32_t ms;

  uint32_t satellites;
  bool fix;

  double lat;
  double lon;
  double speedKph;
  double heading;
  double altitude;

  float ax;
  float ay;
  float az;
  float gx;
  float gy;
  float gz;

  float throttle;
};

const char* stateName() {
  switch (state) {
    case State::BOOT: return "BOOT";
    case State::IDLE: return "IDLE";
    case State::LOGGING: return "LOGGING";
    case State::ERROR: return "ERROR";
  }
  return "UNKNOWN";
}

void loadConfig() {
  prefs.begin("racesync", true);

  cfg.startSpeedKph =
      prefs.getFloat("start", cfg.startSpeedKph);
  cfg.stopSpeedKph =
      prefs.getFloat("stop", cfg.stopSpeedKph);
  cfg.stopDelaySeconds =
      prefs.getUInt("stopDelay", cfg.stopDelaySeconds);
  cfg.imuRateHz =
      prefs.getUInt("imuHz", cfg.imuRateHz);
  cfg.sdFlushMs =
      prefs.getUInt("flushMs", cfg.sdFlushMs);
  cfg.wifiTimeoutSeconds =
      prefs.getUInt("wifiTo", cfg.wifiTimeoutSeconds);

  String s = prefs.getString("ssid", cfg.ssid);
  String p = prefs.getString("password", cfg.password);

  strlcpy(cfg.ssid, s.c_str(), sizeof(cfg.ssid));
  strlcpy(cfg.password, p.c_str(), sizeof(cfg.password));

  prefs.end();
}

void saveConfig() {
  prefs.begin("racesync", false);

  prefs.putFloat("start", cfg.startSpeedKph);
  prefs.putFloat("stop", cfg.stopSpeedKph);
  prefs.putUInt("stopDelay", cfg.stopDelaySeconds);
  prefs.putUInt("imuHz", cfg.imuRateHz);
  prefs.putUInt("flushMs", cfg.sdFlushMs);
  prefs.putUInt("wifiTo", cfg.wifiTimeoutSeconds);
  prefs.putString("ssid", cfg.ssid);
  prefs.putString("password", cfg.password);

  prefs.end();
}

void serviceGps() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

float throttlePercent() {
  const int adc = analogRead(Pin::TPS_ADC);

  float p = 100.0f *
            (float)(adc - TPS_CLOSED_ADC) /
            (float)(TPS_OPEN_ADC - TPS_CLOSED_ADC);

  return constrain(p, 0.0f, 100.0f);
}

bool readImu(Sample &s) {
  if (!imuOk)
    return false;

  sensors_event_t a, g, temperature;

  if (!imu.getEvent(&a, &g, &temperature))
    return false;

  s.ax = a.acceleration.x / 9.80665f;
  s.ay = a.acceleration.y / 9.80665f;
  s.az = a.acceleration.z / 9.80665f;

  s.gx = g.gyro.x * 57.2957795f;
  s.gy = g.gyro.y * 57.2957795f;
  s.gz = g.gyro.z * 57.2957795f;

  return true;
}

Sample makeSample() {
  Sample s{};

  s.ms = millis();
  s.fix = gps.location.isValid();

  if (s.fix) {
    s.satellites =
      gps.satellites.isValid()
        ? gps.satellites.value()
        : 0;

    s.lat = gps.location.lat();
    s.lon = gps.location.lng();

    s.speedKph =
      gps.speed.isValid()
        ? gps.speed.kmph()
        : 0.0;

    s.heading =
      gps.course.isValid()
        ? gps.course.deg()
        : 0.0;

    s.altitude =
      gps.altitude.isValid()
        ? gps.altitude.meters()
        : 0.0;
  }

  readImu(s);

  s.throttle = throttlePercent();

  return s;
}

/*
 VBO coordinates are stored as degrees converted to minutes.
 Example:
 52.123456 degrees -> 3127.40736 minutes
*/
String coordinateMinutes(double degrees) {
  double sign = degrees < 0 ? -1.0 : 1.0;
  double a = fabs(degrees);
  int wholeDegrees = (int)a;
  double minutes = (a - wholeDegrees) * 60.0;
  double result = (wholeDegrees * 60.0 + minutes) * sign;

  char b[32];
  snprintf(b, sizeof(b), "%.5f", result);
  return String(b);
}

String vboTime() {
  if (!gps.time.isValid())
    return "000000.00";

  char b[32];

  snprintf(
    b,
    sizeof(b),
    "%02d%02d%02d.%02lu",
    gps.time.hour(),
    gps.time.minute(),
    gps.time.second(),
    (unsigned long)((millis() % 1000) / 10)
  );

  return String(b);
}

String number(double value, int decimals) {
  char b[40];
  snprintf(b, sizeof(b), "%.*f", decimals, value);
  return String(b);
}

bool writeHeader() {
  const char *header =
    "[header]\n"
    "satellites\n"
    "time\n"
    "latitude\n"
    "longitude\n"
    "velocity kmh\n"
    "heading\n"
    "height\n"
    "accelerometer x g\n"
    "accelerometer y g\n"
    "accelerometer z g\n"
    "gyro x deg/s\n"
    "gyro y deg/s\n"
    "gyro z deg/s\n"
    "throttle %\n"
    "\n"
    "[channel units]\n"
    "satellites number\n"
    "time hhmmss.ss\n"
    "latitude minutes\n"
    "longitude minutes\n"
    "velocity kmh\n"
    "heading degrees\n"
    "height metres\n"
    "accelerometer x g\n"
    "accelerometer y g\n"
    "accelerometer z g\n"
    "gyro x deg/s\n"
    "gyro y deg/s\n"
    "gyro z deg/s\n"
    "throttle percent\n"
    "\n"
    "[comments]\n"
    "RaceSync V1\n"
    "Honda CB500 motorcycle telemetry\n"
    "GPS IMU throttle\n"
    "\n"
    "[column names]\n"
    "sats time lat long velocity heading height accel-x accel-y accel-z gyro-x gyro-y gyro-z throttle\n"
    "\n"
    "[data]\n";

  return vboFile.print(header) == strlen(header);
}

String makeVboLine(const Sample &s) {
  String line;
  line.reserve(220);

  line += String(s.satellites);
  line += " " + vboTime();
  line += " " + coordinateMinutes(s.lat);
  line += " " + coordinateMinutes(s.lon);
  line += " " + number(s.speedKph, 3);
  line += " " + number(s.heading, 2);
  line += " " + number(s.altitude, 2);
  line += " " + number(s.ax, 4);
  line += " " + number(s.ay, 4);
  line += " " + number(s.az, 4);
  line += " " + number(s.gx, 3);
  line += " " + number(s.gy, 3);
  line += " " + number(s.gz, 3);
  line += " " + number(s.throttle, 1);
  line += "\n";

  return line;
}

uint32_t findNextSession() {
  uint32_t n = 1;

  while (SD.exists(
      "/LOGS/SESSION_" + String(n) + ".VBO")) {
    ++n;
  }

  return n;
}

bool startVbo() {
  sessionNumber = findNextSession();

  currentFile =
    "/LOGS/SESSION_" +
    String(sessionNumber) +
    ".VBO";

  vboFile = SD.open(currentFile, FILE_WRITE);

  if (!vboFile)
    return false;

  if (!writeHeader()) {
    vboFile.close();
    return false;
  }

  vboFile.flush();

  return true;
}

bool appendSample(const Sample &s) {
  if (!vboFile)
    return false;

  String line = makeVboLine(s);

  return vboFile.print(line) == line.length();
}

void closeVbo() {
  if (vboFile) {
    vboFile.flush();
    vboFile.close();
  }
}

void sendJson(JsonDocument &doc, int code = 200) {
  String response;
  serializeJson(doc, response);
  server.send(code, "application/json", response);
}

void setupApi() {
  server.on("/api/status", HTTP_GET, [] {
    JsonDocument doc;

    doc["product"] = "RaceSync";
    doc["firmware"] = "V1";
    doc["state"] = stateName();

    JsonObject gpsObj = doc["gps"].to<JsonObject>();
    gpsObj["fix"] = gps.location.isValid();
    gpsObj["satellites"] =
      gps.satellites.isValid()
        ? gps.satellites.value()
        : 0;
    gpsObj["speedKph"] =
      gps.speed.isValid()
        ? gps.speed.kmph()
        : 0;

    JsonObject hw = doc["hardware"].to<JsonObject>();
    hw["sd"] = sdOk;
    hw["imu"] = imuOk;
    hw["throttle"] = true;

    doc["session"] =
      state == State::LOGGING
        ? sessionNumber
        : 0;

    doc["file"] = currentFile;

    sendJson(doc);
  });

  server.on("/api/config", HTTP_GET, [] {
    JsonDocument doc;

    doc["startSpeedKph"] = cfg.startSpeedKph;
    doc["stopSpeedKph"] = cfg.stopSpeedKph;
    doc["stopDelaySeconds"] = cfg.stopDelaySeconds;
    doc["imuRateHz"] = cfg.imuRateHz;
    doc["sdFlushMs"] = cfg.sdFlushMs;
    doc["wifiTimeoutSeconds"] =
      cfg.wifiTimeoutSeconds;

    sendJson(doc);
  });

  server.on("/api/config", HTTP_PUT, [] {
    if (state != State::IDLE) {
      server.send(
        409,
        "text/plain",
        "Configuration only available while idle"
      );
      return;
    }

    if (!server.hasArg("plain")) {
      server.send(
        400,
        "text/plain",
        "JSON body required"
      );
      return;
    }

    JsonDocument doc;

    if (deserializeJson(
          doc,
          server.arg("plain"))) {
      server.send(
        400,
        "text/plain",
        "Invalid JSON"
      );
      return;
    }

    if (doc["startSpeedKph"].is<float>())
      cfg.startSpeedKph =
        doc["startSpeedKph"].as<float>();

    if (doc["stopSpeedKph"].is<float>())
      cfg.stopSpeedKph =
        doc["stopSpeedKph"].as<float>();

    if (doc["stopDelaySeconds"].is<uint32_t>())
      cfg.stopDelaySeconds =
        doc["stopDelaySeconds"].as<uint32_t>();

    if (doc["imuRateHz"].is<uint32_t>())
      cfg.imuRateHz =
        doc["imuRateHz"].as<uint32_t>();

    if (doc["sdFlushMs"].is<uint32_t>())
      cfg.sdFlushMs =
        doc["sdFlushMs"].as<uint32_t>();

    if (cfg.stopSpeedKph >= cfg.startSpeedKph) {
      server.send(
        400,
        "text/plain",
        "Stop speed must be below start speed"
      );
      return;
    }

    saveConfig();

    JsonDocument result;
    result["saved"] = true;

    sendJson(result);
  });

  server.on("/api/sessions", HTTP_GET, [] {
    JsonDocument doc;
    JsonArray sessions =
      doc["sessions"].to<JsonArray>();

    File dir = SD.open("/LOGS");

    if (dir) {
      File file = dir.openNextFile();

      while (file) {
        if (!file.isDirectory()) {
          JsonObject item =
            sessions.add<JsonObject>();

          item["name"] =
            String(file.name());

          item["size"] =
            (uint32_t)file.size();
        }

        file.close();
        file = dir.openNextFile();
      }

      dir.close();
    }

    sendJson(doc);
  });

  server.onNotFound([] {
    server.send(
      404,
      "text/plain",
      "RaceSync endpoint not found"
    );
  });

  server.begin();
}

void startWifi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(
    cfg.ssid,
    cfg.password
  );

  setupApi();

  wifiStartedMs = millis();

  Serial.print("RaceSync AP: ");
  Serial.println(WiFi.softAPIP());
}

void stopWifi() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

void enterLogging() {
  if (!sdOk)
    return;

  if (!startVbo()) {
    state = State::ERROR;
    return;
  }

  stopWifi();

  state = State::LOGGING;

  lastSampleMs = millis();
  lastFlushMs = millis();
  stationarySinceMs = 0;

  Serial.print("RaceSync logging: ");
  Serial.println(currentFile);
}

void finishLogging() {
  closeVbo();

  Serial.print("RaceSync session closed: ");
  Serial.println(currentFile);

  state = State::IDLE;

  startWifi();
}

void updateLeds() {
  digitalWrite(
    Pin::LED_POWER,
    HIGH
  );

  uint32_t now = millis();

  if (state == State::ERROR) {
    digitalWrite(
      Pin::LED_GPS,
      (now / 100) % 2
    );
    return;
  }

  if (state == State::LOGGING) {
    digitalWrite(
      Pin::LED_GPS,
      (now / 150) % 2
    );
    return;
  }

  if (gps.location.isValid()) {
    digitalWrite(
      Pin::LED_GPS,
      (now / 500) % 2
    );
  } else {
    digitalWrite(
      Pin::LED_GPS,
      (now / 1000) % 2
    );
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(Pin::LED_POWER, OUTPUT);
  pinMode(Pin::LED_GPS, OUTPUT);

  loadConfig();

  Wire.begin(
    Pin::I2C_SDA,
    Pin::I2C_SCL
  );

  imuOk = imu.begin();

  if (imuOk) {
    imu.setAccelerometerRange(
      MPU6050_RANGE_8_G
    );

    imu.setGyroRange(
      MPU6050_RANGE_500_DEG
    );

    imu.setFilterBandwidth(
      MPU6050_BAND_44_HZ
    );
  }

  analogReadResolution(12);
  pinMode(Pin::TPS_ADC, INPUT);

  gpsSerial.begin(
    115200,
    SERIAL_8N1,
    Pin::GPS_RX,
    Pin::GPS_TX
  );

  SPI.begin(
    Pin::SD_SCK,
    Pin::SD_MISO,
    Pin::SD_MOSI,
    Pin::SD_CS
  );

  sdOk = SD.begin(
    Pin::SD_CS,
    SPI,
    20000000
  );

  if (sdOk && !SD.exists("/LOGS"))
    SD.mkdir("/LOGS");

  if (!sdOk) {
    state = State::ERROR;
    return;
  }

  /*
   Watchdog is deliberately conservative.
   If the acquisition loop gets stuck for 5 seconds,
   reset the ESP32.
  */
  esp_task_wdt_config_t wdtConfig = {
    .timeout_ms = 5000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };

  esp_task_wdt_init(&wdtConfig);
  esp_task_wdt_add(nullptr);

  state = State::IDLE;

  startWifi();

  Serial.println();
  Serial.println("================================");
  Serial.println("       RaceSync V1");
  Serial.println("================================");
  Serial.println("State: IDLE");
}

void loop() {
  esp_task_wdt_reset();

  serviceGps();
  updateLeds();

  if (state == State::ERROR) {
    delay(10);
    return;
  }

  if (state == State::IDLE) {
    server.handleClient();

    /*
     Wi-Fi is deliberately disabled after the
     configured idle period. GPS monitoring continues.
     If the bike subsequently moves, logging starts.
    */
    if (
      cfg.wifiTimeoutSeconds > 0 &&
      millis() - wifiStartedMs >
        cfg.wifiTimeoutSeconds * 1000UL
    ) {
      stopWifi();
    }

    if (
      gps.location.isValid() &&
      gps.speed.isValid() &&
      gps.speed.kmph() >= cfg.startSpeedKph
    ) {
      enterLogging();
    }

    delay(1);
    return;
  }

  if (state == State::LOGGING) {
    uint32_t now = millis();

    const uint32_t period =
      max(
        1UL,
        1000UL / max(1UL, cfg.imuRateHz)
      );

    if (now - lastSampleMs >= period) {
      lastSampleMs += period;

      Sample sample = makeSample();

      if (!appendSample(sample)) {
        /*
         Never silently continue after a storage error.
         The error state leaves the file intact as far
         as the SD filesystem has committed it.
        */
        closeVbo();
        state = State::ERROR;
        return;
      }
    }

    if (
      now - lastFlushMs >=
      cfg.sdFlushMs
    ) {
      lastFlushMs = now;

      if (vboFile)
        vboFile.flush();
    }

    bool stationary =
      gps.location.isValid() &&
      gps.speed.isValid() &&
      gps.speed.kmph() <= cfg.stopSpeedKph;

    if (stationary) {
      if (stationarySinceMs == 0)
        stationarySinceMs = now;

      if (
        now - stationarySinceMs >=
        cfg.stopDelaySeconds * 1000UL
      ) {
        finishLogging();
      }
    } else {
      stationarySinceMs = 0;
    }

    delay(0);
  }
}
