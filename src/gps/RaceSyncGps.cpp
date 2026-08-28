#include "RaceSyncGps.h"
#include "../config/RaceSyncConfig.h"

bool RaceSyncGps::begin()
{
    _serial.begin(
        RaceSyncConfig::GPS_BAUD,
        SERIAL_8N1,
        RaceSyncConfig::GPS_RX_PIN,
        RaceSyncConfig::GPS_TX_PIN
    );

    Serial.println("[GPS] UART started at 115200");
    return true;
}

void RaceSyncGps::update(Telemetry& telemetry)
{
    _newSample = false;

    while (_serial.available())
    {
        uint8_t value = _serial.read();
        _bytesReceived++;
        captureDiagnosticByte(value);
        parseByte(value, telemetry);
    }

    printDiagnosticIfNeeded();
}

bool RaceSyncGps::connected() const
{
    return
        _lastPacketMs != 0 &&
        millis() - _lastPacketMs < RaceSyncConfig::GPS_STALE_MS;
}

uint32_t RaceSyncGps::lastPacketAgeMs() const
{
    return _lastPacketMs == 0
        ? UINT32_MAX
        : millis() - _lastPacketMs;
}

uint64_t RaceSyncGps::bytesReceived() const
{
    return _bytesReceived;
}

uint64_t RaceSyncGps::packetCount() const
{
    return _packetCount;
}

uint64_t RaceSyncGps::checksumErrors() const
{
    return _checksumErrors;
}

void RaceSyncGps::captureDiagnosticByte(uint8_t data)
{
    _diagnostic[_diagnosticWrite] = data;
    _diagnosticWrite = (_diagnosticWrite + 1) % DIAGNOSTIC_SIZE;
    if (_diagnosticCount < DIAGNOSTIC_SIZE)
        _diagnosticCount++;
}

void RaceSyncGps::printDiagnosticIfNeeded()
{
    // Temporary diagnostic: while no NAV-PVT packets have been decoded,
    // print the latest raw UART sample every five seconds.
    if (_packetCount != 0 || _diagnosticCount == 0)
        return;

    uint32_t now = millis();
    if (_lastDiagnosticPrintMs != 0 && now - _lastDiagnosticPrintMs < 5000)
        return;

    _lastDiagnosticPrintMs = now;

    Serial.printf("[GPS-DIAG] bytes=%llu packets=%llu checksumErrors=%llu\n",
                  (unsigned long long)_bytesReceived,
                  (unsigned long long)_packetCount,
                  (unsigned long long)_checksumErrors);
    Serial.print("[GPS-DIAG] HEX: ");
    Serial.println(diagnosticHex());
    Serial.print("[GPS-DIAG] ASCII: ");
    Serial.println(diagnosticAscii());
}

String RaceSyncGps::diagnosticHex() const
{
    String result;
    result.reserve(_diagnosticCount * 3);

    size_t start = (_diagnosticWrite + DIAGNOSTIC_SIZE - _diagnosticCount) % DIAGNOSTIC_SIZE;
    static const char hex[] = "0123456789ABCDEF";

    for (size_t i = 0; i < _diagnosticCount; ++i)
    {
        uint8_t value = _diagnostic[(start + i) % DIAGNOSTIC_SIZE];
        if (i) result += ' ';
        result += hex[(value >> 4) & 0x0F];
        result += hex[value & 0x0F];
    }

    return result;
}

String RaceSyncGps::diagnosticAscii() const
{
    String result;
    result.reserve(_diagnosticCount);

    size_t start = (_diagnosticWrite + DIAGNOSTIC_SIZE - _diagnosticCount) % DIAGNOSTIC_SIZE;

    for (size_t i = 0; i < _diagnosticCount; ++i)
    {
        uint8_t value = _diagnostic[(start + i) % DIAGNOSTIC_SIZE];
        if (value >= 32 && value <= 126)
            result += static_cast<char>(value);
        else if (value == '\r' || value == '\n')
            result += ' ';
        else
            result += '.';
    }

    return result;
}

uint16_t RaceSyncGps::readU16(const uint8_t* p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

uint32_t RaceSyncGps::readU32(const uint8_t* p)
{
    return
        ((uint32_t)p[0]) |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

int32_t RaceSyncGps::readI32(const uint8_t* p)
{
    return (int32_t)readU32(p);
}

double RaceSyncGps::createRawLatitude(double decimal)
{
    return decimal * 60.0;
}

double RaceSyncGps::createRawLongitude(double decimal)
{
    return decimal * -60.0;
}

void RaceSyncGps::processNavPvt(Telemetry& telemetry)
{
    if (_ubxLength < 92) return;

    uint32_t iTow = readU32(&_payload[0]);

    telemetry.year = readU16(&_payload[4]);
    telemetry.month = _payload[6];
    telemetry.day = _payload[7];

    telemetry.hour = _payload[8];
    telemetry.minute = _payload[9];
    telemetry.second = _payload[10];

    uint8_t validFlags = _payload[11];

    telemetry.timeValid =
        (validFlags & 0x01) &&
        (validFlags & 0x02);

    int32_t nano = readI32(&_payload[16]);

    telemetry.rawTime =
        telemetry.hour * 10000.0 +
        telemetry.minute * 100.0 +
        telemetry.second +
        nano / 1000000000.0;

    telemetry.solutionType = _payload[20];

    uint8_t flags = _payload[21];
    bool fixOK = flags & 0x01;

    telemetry.satellites = _payload[23];

    telemetry.longitude =
        readI32(&_payload[24]) /
        10000000.0;

    telemetry.latitude =
        readI32(&_payload[28]) /
        10000000.0;

    telemetry.rawLatitude =
        createRawLatitude(
            telemetry.latitude
        );

    telemetry.rawLongitude =
        createRawLongitude(
            telemetry.longitude
        );

    telemetry.height =
        readI32(&_payload[36]) /
        1000.0;

    telemetry.verticalVelocityMs =
        -(readI32(&_payload[56]) / 1000.0);

    telemetry.velocityKmh =
        (readI32(&_payload[60]) / 1000.0) *
        3.6;

    telemetry.heading =
        readI32(&_payload[64]) /
        100000.0;

    if (
        _previousITow != 0 &&
        iTow > _previousITow
    )
    {
        uint32_t difference =
            iTow -
            _previousITow;

        if (
            difference >= 20 &&
            difference <= 1000
        )
        {
            telemetry.samplePeriod =
                difference /
                1000.0;
        }
    }

    _previousITow = iTow;

    telemetry.valid =
        fixOK &&
        telemetry.solutionType >= 2;

    telemetry.sampleIndex++;

    _lastPacketMs = millis();
    _newSample = true;
}

void RaceSyncGps::parseByte(
    uint8_t data,
    Telemetry& telemetry)
{
    switch (_state)
    {
        case UbxState::SYNC1:
            if (data == 0xB5)
                _state = UbxState::SYNC2;
            break;

        case UbxState::SYNC2:
            if (data == 0x62)
            {
                _ckA = 0;
                _ckB = 0;
                _state = UbxState::CLASS;
            }
            else
            {
                _state = UbxState::SYNC1;
            }
            break;

        case UbxState::CLASS:
            _ubxClass = data;
            _ckA += data;
            _ckB += _ckA;
            _state = UbxState::ID;
            break;

        case UbxState::ID:
            _ubxId = data;
            _ckA += data;
            _ckB += _ckA;
            _state = UbxState::LENGTH1;
            break;

        case UbxState::LENGTH1:
            _ubxLength = data;
            _ckA += data;
            _ckB += _ckA;
            _state = UbxState::LENGTH2;
            break;

        case UbxState::LENGTH2:
            _ubxLength |= ((uint16_t)data << 8);
            _ckA += data;
            _ckB += _ckA;
            _ubxIndex = 0;

            if (_ubxLength > sizeof(_payload))
            {
                _state = UbxState::SYNC1;
            }
            else
            {
                _state = _ubxLength == 0
                    ? UbxState::CK_A
                    : UbxState::PAYLOAD;
            }
            break;

        case UbxState::PAYLOAD:
            _payload[_ubxIndex++] = data;
            _ckA += data;
            _ckB += _ckA;

            if (_ubxIndex >= _ubxLength)
                _state = UbxState::CK_A;
            break;

        case UbxState::CK_A:
            _receivedCkA = data;
            _state = UbxState::CK_B;
            break;

        case UbxState::CK_B:
            if (
                _receivedCkA == _ckA &&
                data == _ckB
            )
            {
                if (
                    _ubxClass == 0x01 &&
                    _ubxId == 0x07
                )
                {
                    _packetCount++;
                    processNavPvt(telemetry);
                }
            }
            else
            {
                _checksumErrors++;
            }

            _state = UbxState::SYNC1;
            break;
    }
}
