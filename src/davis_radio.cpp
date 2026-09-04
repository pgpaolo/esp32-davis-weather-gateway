#include "davis_radio.h"

#include <RadioLib.h>
#include <SPI.h>
#include <math.h>
#include <string.h>

#include "board_config.h"
#include "config.h"
#include "davis_decoder.h"

namespace {
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
volatile uint32_t irqCounterIsr = 0;
DavisRadioStatus status;
DavisChannelDiagnostics channelDiag[DAVIS_DIAG_CHANNEL_COUNT];
DavisFrameDiagnostic frameHistory[DAVIS_DIAG_FRAME_HISTORY];
uint8_t historyHead = 0;
uint8_t historyCount = 0;
uint8_t missStreak = 0;
uint32_t lastRxMs = 0;
uint32_t lastScanRetuneMs = 0;
uint64_t rawIntervalSum = 0;
uint64_t rawJitterSum = 0;

#if defined(ESP32)
IRAM_ATTR
#endif
void onPacket() {
  irqCounterIsr++;
  if (irqEnabled) packetFlag = true;
}

bool idMatches(uint8_t packetIdZeroBased, uint8_t configuredIssId) {
  if (configuredIssId == 0) return true;
  return (uint8_t)(packetIdZeroBased + 1U) == configuredIssId;
}

void clearDiagnosticArrays() {
  for (uint8_t i = 0; i < DAVIS_DIAG_CHANNEL_COUNT; ++i) channelDiag[i] = DavisChannelDiagnostics{};
  for (uint8_t i = 0; i < DAVIS_DIAG_FRAME_HISTORY; ++i) frameHistory[i] = DavisFrameDiagnostic{};
  historyHead = 0;
  historyCount = 0;
  rawIntervalSum = 0;
  rawJitterSum = 0;
}

void updateRssi(DavisChannelDiagnostics &d, float rssi) {
  if (!isfinite(rssi)) return;
  d.lastRssi = rssi;
  if (!isfinite(d.minRssi) || rssi < d.minRssi) d.minRssi = rssi;
  if (!isfinite(d.maxRssi) || rssi > d.maxRssi) d.maxRssi = rssi;
  d.rssiSum += rssi;
  d.rssiSamples++;
}

String bytesHex(const uint8_t *d) {
  String out;
  out.reserve(30);
  char b[4];
  for (uint8_t i = 0; i < kPacketLength; ++i) {
    snprintf(b, sizeof(b), "%02X", d[i]);
    if (i) out += ' ';
    out += b;
  }
  return out;
}

void recordFrame(uint8_t channel, const uint8_t raw[10], const uint8_t normalized[10],
                 bool crcOk, uint16_t crcReceived, uint16_t crcCalculated,
                 float rssi, int16_t radioError, uint32_t now) {
  if (status.lastRawMs != 0) {
    const uint32_t interval = (uint32_t)(now - status.lastRawMs);
    status.lastRawIntervalMs = interval;
    status.timingSamples++;
    rawIntervalSum += interval;
    status.avgRawIntervalMs = (uint32_t)(rawIntervalSum / status.timingSamples);
    if (status.minRawIntervalMs == 0 || interval < status.minRawIntervalMs) status.minRawIntervalMs = interval;
    if (interval > status.maxRawIntervalMs) status.maxRawIntervalMs = interval;
    const int32_t delta = (int32_t)interval - (int32_t)DAVIS_PACKET_INTERVAL_MS;
    rawJitterSum += (uint32_t)(delta < 0 ? -delta : delta);
    status.avgJitterMs = (uint32_t)(rawJitterSum / status.timingSamples);
  }

  status.haveRawPacket = true;
  status.rawPackets++;
  status.lastRawMs = now;
  status.lastRawRssi = rssi;
  status.lastRawCrcOk = crcOk;
  status.lastCrcReceived = crcReceived;
  status.lastCrcCalculated = crcCalculated;
  memcpy(status.lastRaw, raw, kPacketLength);
  memcpy(status.lastNormalized, normalized, kPacketLength);

  DavisChannelDiagnostics &cd = channelDiag[channel % DAVIS_DIAG_CHANNEL_COUNT];
  cd.rawPackets++;
  cd.lastRxMs = now;
  if (crcOk) cd.crcValid++; else cd.crcErrors++;
  updateRssi(cd, rssi);

  DavisFrameDiagnostic &f = frameHistory[historyHead];
  f = DavisFrameDiagnostic{};
  f.valid = true;
  f.sequence = status.rawPackets;
  f.timestampMs = now;
  f.channel = channel;
  f.frequencyMhz = kEuChannelsMhz[channel % kChannelCount];
  f.rssi = rssi;
  f.radioError = radioError;
  f.crcOk = crcOk;
  f.crcReceived = crcReceived;
  f.crcCalculated = crcCalculated;
  memcpy(f.raw, raw, kPacketLength);
  memcpy(f.normalized, normalized, kPacketLength);
  historyHead = (uint8_t)((historyHead + 1U) % DAVIS_DIAG_FRAME_HISTORY);
  if (historyCount < DAVIS_DIAG_FRAME_HISTORY) historyCount++;
}

bool tune(uint8_t channel) {
  channel %= kChannelCount;
  status.tuneCount++;
  int16_t rc = radio.standby();
  if (rc != RADIOLIB_ERR_NONE) {
    status.lastRadioError = rc;
    status.tuneErrors++;
    return false;
  }
  rc = radio.setFrequency(kEuChannelsMhz[channel]);
  if (rc != RADIOLIB_ERR_NONE) {
    status.lastRadioError = rc;
    status.tuneErrors++;
    return false;
  }
  status.channel = channel;
  status.frequencyMhz = kEuChannelsMhz[channel];
  status.lastHopMs = millis();
  rc = radio.startReceive();
  status.lastRadioError = rc;
  if (rc != RADIOLIB_ERR_NONE) status.startReceiveErrors++;
  return rc == RADIOLIB_ERR_NONE;
}

void hop() {
  status.hopCount++;
  tune((uint8_t)((status.channel + 1U) % kChannelCount));
}

void loseSyncAndScan() {
  status.synchronized = false;
  missStreak = 0;
  status.missStreak = 0;
  tune(0);
  lastScanRetuneMs = millis();
}

void printPacket(const uint8_t d[10], const DavisDecodeResult &decoded, float rssi) {
  Serial.print(F("[DAVIS] ch=")); Serial.print(status.channel + 1U);
  Serial.print(F(" f=")); Serial.print(status.frequencyMhz, 6);
  Serial.print(F("MHz id=")); Serial.print(decoded.stationId + 1U);
  Serial.print(F(" type=")); Serial.print(davisPacketTypeName(decoded.packetType));
  Serial.print(F(" rssi=")); Serial.print(rssi, 1);
  Serial.print(F(" data=")); Serial.println(bytesHex(d));
}

void printRawPacket(const uint8_t raw[10], bool crcOk, uint16_t received, uint16_t calculated, float rssi) {
  Serial.print(F("[DAVIS-RAW] #")); Serial.print(status.rawPackets);
  Serial.print(F(" ch=")); Serial.print(status.channel + 1U);
  Serial.print(F(" rssi=")); Serial.print(rssi, 1);
  Serial.print(F(" crc=")); Serial.print(crcOk ? F("OK") : F("KO"));
  Serial.print(F(" rx=0x")); Serial.print(received, HEX);
  Serial.print(F(" calc=0x")); Serial.print(calculated, HEX);
  Serial.print(F(" raw=")); Serial.println(bytesHex(raw));
}

const char *phaseName() {
  if (!status.initialized) return "ERROR";
  if (status.synchronized) return "SYNC";
  if (status.haveRawPacket) return "CANDIDATE";
  return "SEARCH";
}

String floatJson(float value, uint8_t decimals = 1U) {
  return isfinite(value) ? String(value, (unsigned int)decimals) : String("null");
}

void appendCrcHex(String &j, uint16_t value) {
  char b[7];
  snprintf(b, sizeof(b), "%04X", value);
  j += b;
}
}

bool initDavisRadio() {
  status = DavisRadioStatus{};
  clearDiagnosticArrays();
  irqCounterIsr = 0;
  SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);

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
    status.startReceiveErrors++;
    status.initialized = false;
    return false;
  }
  return true;
}

void serviceDavisRadio(StationState &station, uint8_t configuredIssId, float rainMmPerTip) {
  status.irqCount = irqCounterIsr;
  if (!status.initialized) return;

  if (packetFlag) {
    irqEnabled = false;
    packetFlag = false;

    const uint8_t rxChannel = status.channel;
    uint8_t raw[kPacketLength] = {0};
    const int16_t rc = radio.readData(raw, kPacketLength);
    const float rssi = radio.getRSSI(true, true);
    status.lastRadioError = rc;

    uint8_t normalized[kPacketLength] = {0};
    bool crcOk = false;
    uint16_t crcCalculated = 0;
    uint16_t crcReceived = 0;
    const uint32_t now = millis();

    if (rc == RADIOLIB_ERR_NONE) {
      status.readOk++;
      crcOk = validateAndNormalizeDavisPacket(raw, kPacketLength, normalized);
      crcCalculated = davisCrc16(normalized, 6);
      crcReceived = ((uint16_t)normalized[6] << 8) | normalized[7];
      recordFrame(rxChannel, raw, normalized, crcOk, crcReceived, crcCalculated, rssi, rc, now);
      printRawPacket(raw, crcOk, crcReceived, crcCalculated, rssi);
    } else {
      status.readErrors++;
      channelDiag[rxChannel % DAVIS_DIAG_CHANNEL_COUNT].readErrors++;
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
        status.missStreak = 0;
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
      status.missStreak = missStreak;
      lastRxMs += DAVIS_PACKET_INTERVAL_MS;
      hop();
      if (missStreak > kMaxMissedBeforeRescan) loseSyncAndScan();
    }
  } else if ((uint32_t)(now - lastScanRetuneMs) > 15000UL) {
    tune(0);
    lastScanRetuneMs = now;
  }
}

const DavisRadioStatus &getDavisRadioStatus() { return status; }

const DavisChannelDiagnostics &getDavisChannelDiagnostics(uint8_t channel) {
  static DavisChannelDiagnostics empty;
  return channel < DAVIS_DIAG_CHANNEL_COUNT ? channelDiag[channel] : empty;
}

uint8_t getDavisFrameHistoryCount() { return historyCount; }

bool getDavisFrameHistory(uint8_t newestIndex, DavisFrameDiagnostic &out) {
  if (newestIndex >= historyCount) return false;
  const uint8_t idx = (uint8_t)((historyHead + DAVIS_DIAG_FRAME_HISTORY - 1U - newestIndex) % DAVIS_DIAG_FRAME_HISTORY);
  out = frameHistory[idx];
  return out.valid;
}

void resetDavisDiagnosticWindow() {
  const bool wasRaw = status.haveRawPacket;
  const bool wasCrc = status.lastRawCrcOk;
  uint8_t raw[10];
  uint8_t norm[10];
  memcpy(raw, status.lastRaw, 10);
  memcpy(norm, status.lastNormalized, 10);
  const uint16_t crcRx = status.lastCrcReceived;
  const uint16_t crcCalc = status.lastCrcCalculated;
  const uint32_t rawMs = status.lastRawMs;
  const float rawRssi = status.lastRawRssi;

  clearDiagnosticArrays();
  status.rawPackets = 0;
  status.readOk = 0;
  status.readErrors = 0;
  status.irqCount = irqCounterIsr;
  status.lastRawIntervalMs = 0;
  status.minRawIntervalMs = 0;
  status.maxRawIntervalMs = 0;
  status.avgRawIntervalMs = 0;
  status.avgJitterMs = 0;
  status.timingSamples = 0;
  status.haveRawPacket = wasRaw;
  status.lastRawCrcOk = wasCrc;
  status.lastCrcReceived = crcRx;
  status.lastCrcCalculated = crcCalc;
  status.lastRawMs = rawMs;
  status.lastRawRssi = rawRssi;
  memcpy(status.lastRaw, raw, 10);
  memcpy(status.lastNormalized, norm, 10);
}

String davisRadioDiagnosticsJson(const StationState &station, uint8_t configuredIssId, bool includeHistory) {
  String j;
  j.reserve(includeHistory ? 12500 : 4800);
  j = "{\"initialized\":"; j += status.initialized ? "true" : "false";
  j += ",\"synchronized\":"; j += status.synchronized ? "true" : "false";
  j += ",\"phase\":\"" + String(phaseName()) + "\"";
  j += ",\"channel\":" + String(status.channel + 1U);
  j += ",\"frequency_mhz\":" + String(status.frequencyMhz, 6);
  j += ",\"configured_iss_id\":" + String(configuredIssId);
  j += ",\"station_id\":" + (station.locked ? String(station.stationId + 1U) : String("0"));
  j += ",\"rssi_dbm\":" + floatJson(station.rssi);
  j += ",\"last_raw_rssi_dbm\":" + floatJson(status.lastRawRssi);
  j += ",\"have_raw_packet\":"; j += status.haveRawPacket ? "true" : "false";
  j += ",\"last_raw_crc_ok\":"; j += status.lastRawCrcOk ? "true" : "false";
  j += ",\"last_raw_age_ms\":" + String(status.haveRawPacket ? (uint32_t)(millis() - status.lastRawMs) : 0UL);
  j += ",\"last_raw_hex\":\"" + (status.haveRawPacket ? bytesHex(status.lastRaw) : String()) + "\"";
  j += ",\"last_normalized_hex\":\"" + (status.haveRawPacket ? bytesHex(status.lastNormalized) : String()) + "\"";
  j += ",\"last_crc_received\":\""; appendCrcHex(j, status.lastCrcReceived); j += "\"";
  j += ",\"last_crc_calculated\":\""; appendCrcHex(j, status.lastCrcCalculated); j += "\"";
  j += ",\"raw_packets\":" + String(status.rawPackets);
  j += ",\"packets_ok\":" + String(station.packetsOk);
  j += ",\"crc_errors\":" + String(station.crcErrors);
  j += ",\"missed\":" + String(station.packetsMissed);
  j += ",\"resyncs\":" + String(station.resyncs);
  j += ",\"last_packet_type\":\"" + String(davisPacketTypeName(station.lastPacketType)) + "\"";
  j += ",\"last_radio_error\":" + String(status.lastRadioError);
  j += ",\"irq_count\":" + String(status.irqCount);
  j += ",\"read_ok\":" + String(status.readOk);
  j += ",\"read_errors\":" + String(status.readErrors);
  j += ",\"tune_count\":" + String(status.tuneCount);
  j += ",\"tune_errors\":" + String(status.tuneErrors);
  j += ",\"hop_count\":" + String(status.hopCount);
  j += ",\"start_receive_errors\":" + String(status.startReceiveErrors);
  j += ",\"miss_streak\":" + String(status.missStreak);
  j += ",\"last_hop_age_ms\":" + String((uint32_t)(millis() - status.lastHopMs));
  j += ",\"timing\":{\"expected_ms\":" + String(DAVIS_PACKET_INTERVAL_MS);
  j += ",\"last_ms\":" + String(status.lastRawIntervalMs);
  j += ",\"min_ms\":" + String(status.minRawIntervalMs);
  j += ",\"max_ms\":" + String(status.maxRawIntervalMs);
  j += ",\"avg_ms\":" + String(status.avgRawIntervalMs);
  j += ",\"avg_jitter_ms\":" + String(status.avgJitterMs);
  j += ",\"samples\":" + String(status.timingSamples) + "}";
  j += ",\"radio_profile\":{\"modulation\":\"2-FSK\",\"bitrate_kbps\":19.2,\"deviation_khz\":4.8,\"rx_bw_khz\":25.0,\"preamble_bits\":32,\"sync\":\"CB 89\",\"packet_bytes\":10,\"afc\":true,\"afc_bw_khz\":50.0}";

  j += ",\"channels\":[";
  for (uint8_t i = 0; i < DAVIS_DIAG_CHANNEL_COUNT; ++i) {
    if (i) j += ',';
    const DavisChannelDiagnostics &d = channelDiag[i];
    j += "{\"channel\":" + String(i + 1U) + ",\"frequency_mhz\":" + String(kEuChannelsMhz[i], 6);
    j += ",\"raw\":" + String(d.rawPackets) + ",\"crc_valid\":" + String(d.crcValid) + ",\"crc_errors\":" + String(d.crcErrors) + ",\"read_errors\":" + String(d.readErrors);
    j += ",\"last_rssi_dbm\":" + floatJson(d.lastRssi) + ",\"min_rssi_dbm\":" + floatJson(d.minRssi) + ",\"max_rssi_dbm\":" + floatJson(d.maxRssi);
    j += ",\"avg_rssi_dbm\":" + (d.rssiSamples ? String(d.rssiSum / d.rssiSamples, 1) : String("null"));
    j += ",\"last_rx_age_ms\":" + String(d.lastRxMs ? (uint32_t)(millis() - d.lastRxMs) : 0UL) + "}";
  }
  j += ']';

  if (includeHistory) {
    j += ",\"history\":[";
    DavisFrameDiagnostic f;
    for (uint8_t n = 0; n < historyCount; ++n) {
      if (!getDavisFrameHistory(n, f)) continue;
      if (n) j += ',';
      j += "{\"seq\":" + String(f.sequence) + ",\"age_ms\":" + String((uint32_t)(millis() - f.timestampMs));
      j += ",\"channel\":" + String(f.channel + 1U) + ",\"frequency_mhz\":" + String(f.frequencyMhz, 6) + ",\"rssi_dbm\":" + floatJson(f.rssi);
      j += ",\"crc_ok\":"; j += f.crcOk ? "true" : "false";
      j += ",\"crc_received\":\""; appendCrcHex(j, f.crcReceived); j += "\"";
      j += ",\"crc_calculated\":\""; appendCrcHex(j, f.crcCalculated); j += "\"";
      j += ",\"radio_error\":" + String(f.radioError) + ",\"raw\":\"" + bytesHex(f.raw) + "\",\"normalized\":\"" + bytesHex(f.normalized) + "\"}";
    }
    j += ']';
  }
  j += '}';
  return j;
}

String davisRadioDiagnosticReport(const StationState &station, uint8_t configuredIssId) {
  String r;
  r.reserve(9000);
  r += "ESP32 Davis Weather Gateway - RF diagnostic report\n";
  r += "Firmware: " + String(FIRMWARE_VERSION) + "\nBoard: " + String(BOARD_NAME) + "\n";
  r += "Uptime ms: " + String(millis()) + "\nPhase: " + String(phaseName()) + "\n";
  r += "Configured ISS: " + String(configuredIssId) + "  Received ISS: " + (station.locked ? String(station.stationId + 1U) : String("--")) + "\n";
  r += "Channel: " + String(status.channel + 1U) + "  Frequency: " + String(status.frequencyMhz, 6) + " MHz\n";
  r += "IRQ: " + String(status.irqCount) + "  RAW: " + String(status.rawPackets) + "  Read OK: " + String(status.readOk) + "  Read errors: " + String(status.readErrors) + "\n";
  r += "Davis OK: " + String(station.packetsOk) + "  CRC errors: " + String(station.crcErrors) + "  Missed: " + String(station.packetsMissed) + "  Resync: " + String(station.resyncs) + "\n";
  r += "Timing expected/last/min/max/avg/jitter: " + String(DAVIS_PACKET_INTERVAL_MS) + "/" + String(status.lastRawIntervalMs) + "/" + String(status.minRawIntervalMs) + "/" + String(status.maxRawIntervalMs) + "/" + String(status.avgRawIntervalMs) + "/" + String(status.avgJitterMs) + " ms\n";
  if (status.haveRawPacket) {
    char crc[48]; snprintf(crc, sizeof(crc), "Last CRC received=%04X calculated=%04X %s\n", status.lastCrcReceived, status.lastCrcCalculated, status.lastRawCrcOk ? "OK" : "KO"); r += crc;
    r += "Last RAW:        " + bytesHex(status.lastRaw) + "\nLast normalized: " + bytesHex(status.lastNormalized) + "\n";
  }
  r += "\nPer-channel diagnostics:\n";
  for (uint8_t i = 0; i < DAVIS_DIAG_CHANNEL_COUNT; ++i) {
    const DavisChannelDiagnostics &d = channelDiag[i];
    r += "CH" + String(i + 1U) + " " + String(kEuChannelsMhz[i], 6) + " MHz  RAW=" + String(d.rawPackets) + " CRC_OK=" + String(d.crcValid) + " CRC_KO=" + String(d.crcErrors) + " READ_KO=" + String(d.readErrors);
    r += " RSSI(last/avg/min/max)=" + floatJson(d.lastRssi) + "/" + (d.rssiSamples ? String(d.rssiSum / d.rssiSamples, 1) : String("null")) + "/" + floatJson(d.minRssi) + "/" + floatJson(d.maxRssi) + "\n";
  }
  r += "\nRecent frames (newest first):\n";
  DavisFrameDiagnostic f;
  for (uint8_t n = 0; n < historyCount; ++n) {
    if (!getDavisFrameHistory(n, f)) continue;
    char crc[64]; snprintf(crc, sizeof(crc), "#%lu CH%u RSSI %.1f CRC %s RX=%04X CALC=%04X age=%lums\n", (unsigned long)f.sequence, (unsigned)(f.channel + 1U), f.rssi, f.crcOk ? "OK" : "KO", f.crcReceived, f.crcCalculated, (unsigned long)(millis() - f.timestampMs));
    r += crc;
    r += "  RAW  " + bytesHex(f.raw) + "\n  NORM " + bytesHex(f.normalized) + "\n";
  }
  return r;
}

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
