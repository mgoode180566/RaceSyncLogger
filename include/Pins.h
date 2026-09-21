#pragma once

// Central hardware pin map for the RaceSync XIAO ESP32-S3 Plus board build.
// Application/peripheral code must use these names rather than hard-coded GPIOs.
namespace Pin
{
    // MG-902 GPS UART
    constexpr int GPS_RX = 44; // XIAO D7: connect MG-902 TX here
    constexpr int GPS_TX = 43; // XIAO D6: connect MG-902 RX here

    // Reserved expansion bus
    constexpr int I2C_SDA = 5; // XIAO D4
    constexpr int I2C_SCL = 6; // XIAO D5

    // 3.3 V potentiometric throttle-position input
    constexpr int TPS_ADC = 1; // XIAO D0 / ADC

    // Isolated ECU tachometer input. Connect only through the 12 V
    // optocoupler/interface; never connect the ECU signal directly.
    constexpr int RPM_INPUT = 4; // XIAO D3

    // microSD SPI
    constexpr int SD_CS   = 3; // XIAO D2
    constexpr int SD_SCK  = 7; // XIAO D8
    constexpr int SD_MISO = 8; // XIAO D9
    constexpr int SD_MOSI = 9; // XIAO D10
}
