#pragma once

namespace Board {
    constexpr const char* NAME = "XIAO ESP32-S3 Plus";
}

namespace Pin {
    constexpr int GPS_RX = 44;
    constexpr int GPS_TX = 43;
    constexpr int I2C_SDA = 5;
    constexpr int I2C_SCL = 6;
    constexpr int SD_CS = 3;
    constexpr int SD_SCK = 7;
    constexpr int SD_MISO = 8;
    constexpr int SD_MOSI = 9;
    constexpr int TPS_ADC = 1;
    constexpr int RPM_INPUT = 4; // Isolated, 3.3 V tachometer input only.
}

namespace Board { constexpr int STATUS_LED = 21; }
