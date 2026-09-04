#include "davis_radio.h"

#include <RadioLib.h>
#include <SPI.h>
#include <string.h>

#include "board_config.h"
#include "config.h"
#include "davis_decoder.h"

namespace {
// European Davis Vantage Pro2/Vue hop set. The values below are the carrier
// frequencies corresponding to the five documented EU synthesizer words.
constexpr float kEuChannelsMhz[] = {
    868.066711f,
    868.297119f,
    868.527466f,
    868.181885f,
    868.412292f,
};
constexpr uint8_t kChannelCount = sizeof(kEuChannelsMhz) / sizeof(kEuChannelsMhz[0]);
constexpr uint8_t kPacketLength = 10;
constexpr uint8_t kMaxMissedBeforeRescan = 25;

SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
volatile bool packetFlag = false;
volatile bool irqEnabled = true;
DavisRadioStatus status;
uint8_t missStreak = 0;
uint32_t lastRxMs = 0;
uint32_t lastScanRetuneMs = 0;

#if defined(ESP32)
IRAM_ATTR
#endif
void onPacket() {
  if (irqEnabled) packetFlag = true;
}

bool idMatches(uint8_t packetIdZeroBased, uint8_t configuredIssId) {
  if (configuredIssId == 0) return true;
  return (uint8_t)(packetIdZeroBased + 1U) == configuredIssId;
}

bool tune(uint8_t channel) {
  channel %= kChannelCount;
  int16_t rc = radio.standby();
  if (rc != RADIOLIB_ERR_NONE) {
    status.lastRadioError = rc;
    return false;
  }
  rc = radio.setFrequency(kEuChannelsMhz[channel]);
  if (rc != RADIOLIB_ERR_NONE) {
    status.lastRadioError = rc;
    return false;
  }
  status.channel = channel;
  status.frequencyMhz = kEuChannelsMhz[channel];
  status.lastHopMs = millis();
  rc = radio.startReceive();
  status.lastRadioError = rc;
  return rc == RADIOLIB_ERR_NONE;
}

void hop() {
  tune((uint8_t)((status.channel + 1U) % kChannelCount));
}

void loseSyncAndScan() {
  status.synchronized = false;
  missStreak = 0;
  // A Davis EU transmitter returns to every channel every five packets.
  // Staying on channel 0 guarantees a new acquisition opportunity without
  // the phase-lock problem that can occur with a fast deterministic scanner.
  tune(0);
  lastScanRetuneMs = millis();
}

void printPacket(const uint8_t d[10], const DavisDecodeResult &decoded, float rssi) {
  Serial.print(F("[DAVIS] ch=")); Serial.print(status.channel);
  Serial.print(F(" f=")); Serial.print(status.frequencyMhz, 6);
  Serial.print(F("MHz id=")); Serial.print(decoded.stationId + 1U);
  Serial.print(F(" type=")); Serial.print(davisPacketTypeName(decoded.packetType));
  Serial.print(F(" rssi=")); Serial.print(rssi, 1);
  Serial.print(F(" data="));
  for (uint8_t i = 0; i < 10; ++i) {
    if (d[i] < 0x10) Serial.print('0');
    Serial.print(d[i], HEX);
    if (i != 9) Serial.print(' ');
  }
  Serial.println();
}

void printRawPacket(const uint8_t raw[10], bool crcOk, float rssi) {
  Serial.print(F("[DAVIS-RAW] #")); Serial.print(status.rawPackets);
  Serial.print(F(" ch=")); Serial.print(status.channel + 1U);
  Serial.print(F(" rssi=")); Serial.print(rssi, 1);
  Serial.print(F(" crc=")); Serial.print(crcOk ? F("OK") : F("KO"));
  Serial.print(F(" raw="));
  for (uint8_t i = 0; i < kPacketLength; ++i) {
    if (raw[i] < 0x10) Serial.print('0');
    Serial.print(raw[i], HEX);
    if (i + 1U != kPacketLength) Serial.print(' ');
  }
  Serial.println();
}
}

bool initDavisRadio() {
  status = DavisRadioStatus{};
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

  // Davis ISS: 19.2 kbps 2-FSK, 4.8 kHz deviation, Gaussian BT=0.5,
  // four-byte 0xAA preamble, sync 0xCB 0x89, fixed 10-byte payload.
  int16_t rc = radio.beginFSK(kEuChannelsMhz[0], 19.2f, 4.8f, 25.0f, 10, 32, false);
  if (rc != RADIOLIB_ERR_NONE) {
    status.lastRadioError = rc;
    return false;
  }

  rc = radio.setDataShaping(RADIOLIB_SHAPING_0_5);
  if (rc != RADIOLIB_ERR_NONE) { status.lastRadioError = rc; return false; }

  uint8_t syncWord[] = {0xCB, 0x89};
  rc = radio.setSyncWord(syncWord, sizeof(syncWord));
  if (rc != RADIOLIB_ERR_NONE) { status.lastRadioError = rc; return false; }

  rc = radio.fixedPacketLengthMode(kPacketLength);
  if (rc != RADIOLIB_ERR_NONE) { status.lastRadioError = rc; return false; }

  // Davis carries its own CCITT CRC in bytes 6..7. Do not let the SX1276
  // expect or strip the RadioLib default FSK CRC.
  rc = radio.setCRC(false);
  if (rc != RADIOLIB_ERR_NONE) { status.lastRadioError = rc; return false; }

  radio.setAFC(true);
  radio.setAFCBandwidth(50.0f);
  radio.setGain(0);
  radio.setPacketReceivedAction(onPacket);

  status.initialized = true;
  status.channel = 0;
  status.frequencyMhz = kEuChannelsMhz[0];
  status.synchronized = false;
  packetFlag = false;
  irqEnabled = true;
  missStreak = 0;
  lastRxMs = 0;
  lastScanRetuneMs = millis();

  rc = radio.startReceive();
  status.lastRadioError = rc;
  if (rc != RADIOLIB_ERR_NONE) {
    status.initialized = false;
    return false;
  }
  return true;
}

void serviceDavisRadio(StationState &station, uint8_t configuredIssId, float rainMmPerTip) {
  if (!status.initialized) return;

  if (packetFlag) {
    irqEnabled = false;
    packetFlag = false;

    uint8_t raw[kPacketLength] = {0};
    int16_t rc = radio.readData(raw, kPacketLength);
    const float rssi = radio.getRSSI(true, true);
    status.lastRadioError = rc;

    uint8_t normalized[kPacketLength] = {0};
    bool crcOk = false;
    const uint32_t now = millis();

    if (rc == RADIOLIB_ERR_NONE) {
      status.haveRawPacket = true;
      status.rawPackets++;
      status.lastRawMs = now;
      status.lastRawRssi = rssi;
      memcpy(status.lastRaw, raw, kPacketLength);
      crcOk = validateAndNormalizeDavisPacket(raw, kPacketLength, normalized);
      status.lastRawCrcOk = crcOk;
      printRawPacket(raw, crcOk, rssi);
    }

    lastRxMs = now;

    if (crcOk) {
      const uint8_t packetId = normalized[0] & 0x07U;
      if (idMatches(packetId, configuredIssId)) {
        DavisDecodeResult decoded = decodeDavisPacket(normalized, station, rainMmPerTip);
        station.rssi = rssi;
        station.packetsOk++;
        station.lastPacketMs = now;
        station.locked = true;
        status.synchronized = true;
        missStreak = 0;
        printPacket(normalized, decoded, rssi);
        digitalWrite(BOARD_LED_PIN, BOARD_LED_ON);
      } else {
        Serial.print(F("[DAVIS] packet valido ignorato: ID "));
        Serial.print(packetId + 1U);
        Serial.print(F(" (configurato ID "));
        Serial.print(configuredIssId);
        Serial.println(')');
      }
    } else {
      station.crcErrors++;
      Serial.print(F("[DAVIS] CRC/read KO rc=")); Serial.println(rc);
    }

    // A detected Davis-shaped packet consumes one hop in the transmitter
    // sequence even when its CRC is bad; hopping here mirrors console behavior.
    hop();
    irqEnabled = true;
    return;
  }

  const uint32_t now = millis();
  if (status.synchronized && lastRxMs != 0) {
    const uint32_t due = DAVIS_PACKET_INTERVAL_MS + DAVIS_MISS_GRACE_MS;
    if ((uint32_t)(now - lastRxMs) > due) {
      station.packetsMissed++;
      if (missStreak == 0) station.resyncs++;
      missStreak++;
      lastRxMs += DAVIS_PACKET_INTERVAL_MS;
      hop();
      if (missStreak > kMaxMissedBeforeRescan) {
        loseSyncAndScan();
      }
    }
  } else {
    // Re-arm channel 0 occasionally in case the radio was disturbed. We do not
    // cycle channels while unsynchronized: every EU ISS revisits channel 0.
    if ((uint32_t)(now - lastScanRetuneMs) > 15000UL) {
      tune(0);
      lastScanRetuneMs = now;
    }
  }
}

const DavisRadioStatus &getDavisRadioStatus() { return status; }

const char *davisPacketTypeName(uint8_t type) {
  switch (type) {
    case 0x4: return "UV";
    case 0x5: return "RAIN_RATE";
    case 0x6: return "SOLAR";
    case 0x8: return "TEMP";
    case 0x9: return "WIND_GUST";
    case 0xA: return "HUMIDITY";
    case 0xE: return "RAIN";
    default: return "OTHER";
  }
}
