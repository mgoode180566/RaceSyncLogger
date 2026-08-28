#pragma once
#include <Arduino.h>
#include "../config/RaceSyncTypes.h"

class RaceSyncGps
{
public:
    bool begin();
    void update(Telemetry& telemetry);

    bool connected() const;
    uint32_t lastPacketAgeMs() const;

    uint64_t bytesReceived() const;
    uint64_t packetCount() const;
    uint64_t checksumErrors() const;

    // Temporary GPS UART diagnostics. Keeps the most recent 128 bytes so the
    // actual MG-902 output protocol can be identified without changing wiring.
    String diagnosticHex() const;
    String diagnosticAscii() const;

private:
    enum class UbxState
    {
        SYNC1,
        SYNC2,
        CLASS,
        ID,
        LENGTH1,
        LENGTH2,
        PAYLOAD,
        CK_A,
        CK_B
    };

    HardwareSerial _serial = HardwareSerial(1);

    UbxState _state = UbxState::SYNC1;

    uint8_t _ubxClass = 0;
    uint8_t _ubxId = 0;
    uint16_t _ubxLength = 0;
    uint16_t _ubxIndex = 0;

    uint8_t _payload[128] = {};

    uint8_t _ckA = 0;
    uint8_t _ckB = 0;
    uint8_t _receivedCkA = 0;

    uint32_t _lastPacketMs = 0;
    uint32_t _previousITow = 0;

    uint64_t _bytesReceived = 0;
    uint64_t _packetCount = 0;
    uint64_t _checksumErrors = 0;

    static constexpr size_t DIAGNOSTIC_SIZE = 128;
    uint8_t _diagnostic[DIAGNOSTIC_SIZE] = {};
    size_t _diagnosticWrite = 0;
    size_t _diagnosticCount = 0;

    bool _newSample = false;

    void captureDiagnosticByte(uint8_t data);
    void parseByte(uint8_t data, Telemetry& telemetry);
    void processNavPvt(Telemetry& telemetry);

    static uint16_t readU16(const uint8_t* p);
    static uint32_t readU32(const uint8_t* p);
    static int32_t readI32(const uint8_t* p);

    static double createRawLatitude(double decimal);
    static double createRawLongitude(double decimal);
};
