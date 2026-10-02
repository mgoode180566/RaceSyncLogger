#pragma once

namespace Board {
    constexpr const char* NAME = "ESP32-S3 DevKitC-1";
}

namespace Pin {
    constexpr int GPS_RX = 16;
    constexpr int GPS_TX = 17;
    constexpr int I2C_SDA = 8;
    constexpr int I2C_SCL = 9;
    constexpr int SD_CS = 10;
    constexpr int SD_SCK = 12;
    constexpr int SD_MISO = 13;
    constexpr int SD_MOSI = 11;
    constexpr int TPS_ADC = 1;
    constexpr int RPM_INPUT = 4; // Isolated, 3.3 V tachometer input only.
}
