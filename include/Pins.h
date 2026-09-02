#pragma once

namespace Pin
{
    constexpr int GPS_RX = 16;
    constexpr int GPS_TX = 17;

    constexpr int I2C_SDA = 8;
    constexpr int I2C_SCL = 9;

    constexpr int TPS_ADC = 1;

    // Isolated ECU tachometer input. Connect only through the 12 V
    // optocoupler/interface; never connect the ECU signal directly.
    constexpr int RPM_INPUT = 4;

    constexpr int SD_CS = 10;
    constexpr int SD_SCK = 12;
    constexpr int SD_MISO = 13;
    constexpr int SD_MOSI = 11;

    constexpr int LED_POWER = 2;
    constexpr int LED_GPS = 3;
}