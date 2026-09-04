#include "display_manager.h"

#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <math.h>

#include "board_config.h"
#include "config.h"
#include "davis_radio.h"
#include "lightning_manager.h"
#include "mqtt_publisher.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "runtime_config.h"

namespace {
#if OLED_ENABLE
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
#endif

bool available = false;
uint32_t lastRefreshMs = 0;
uint32_t pageEpochMs = 0;
uint32_t searchPhaseEpochMs = 0;
uint8_t page = 0;
uint8_t searchPhase = 0;

constexpr uint32_t REFRESH_MS = 750UL;
constexpr uint32_t PAGE_INTERVAL_MS = 6500UL;
constexpr uint32_t SEARCH_PHASE_MS = 2500UL;
constexpr uint8_t PAGE_COUNT = 6U;
constexpr uint32_t STALE_PACKET_MS = 12000UL;

void header(const char *title, bool wifiOk, bool mqttOk, bool rfOk) {
#if OLED_ENABLE
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 9, title);
  oled.drawStr(96, 9, wifiOk ? "W" : "-");
  oled.drawStr(108, 9, mqttOk ? "M" : "-");
  oled.drawStr(120, 9, rfOk ? "R" : "-");
  oled.drawHLine(0, 12, 128);
#else
  (void)title; (void)wifiOk; (void)mqttOk; (void)rfOk;
#endif
}

void line(const char *text, uint8_t y) {
#if OLED_ENABLE
  oled.drawStr(0, y, text);
#else
  (void)text; (void)y;
#endif
}

void valueOrDash(char *buf, size_t len, float value, uint8_t decimals, const char *suffix) {
  if (!isfinite(value)) {
    snprintf(buf, len, "--%s", suffix);
    return;
  }
  if (decimals == 0U) snprintf(buf, len, "%.0f%s", value, suffix);
  else snprintf(buf, len, "%.1f%s", value, suffix);
}

void formatRaw5(char *buf, size_t len, const uint8_t *raw) {
  snprintf(buf, len, "%02X %02X %02X %02X %02X",
           raw[0], raw[1], raw[2], raw[3], raw[4]);
}

void renderSearch(const StationState &s) {
#if OLED_ENABLE
  const DavisRadioStatus &rf = getDavisRadioStatus();
  const MqttRuntimeStatus &mq = getMqttStatus();
  const bool wifiOk = networkConnected();
  header(rf.initialized ? "DAVIS SEARCH" : "DAVIS RF ERROR", wifiOk, mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf);
  char b[56];

  if (!rf.initialized) {
    snprintf(b, sizeof(b), "Radio init KO rc=%d", (int)rf.lastRadioError); line(b, 24);
    snprintf(b, sizeof(b), "Board %s", BOARD_NAME); line(b, 34);
    line("Controllare SX1276/pin", 44);
    snprintf(b, sizeof(b), "WEB %s", networkIp().c_str()); line(b, 54);
    return;
  }

  snprintf(b, sizeof(b), "CH%u  %.6f MHz", (unsigned)(rf.channel + 1U), rf.frequencyMhz); line(b, 23);
  if (runtimeConfig.issId == 0) snprintf(b, sizeof(b), "ISS AUTO   RSSI %s", isfinite(rf.lastRawRssi) ? String(rf.lastRawRssi,1).c_str() : "--");
  else snprintf(b, sizeof(b), "ISS ID %u  RSSI %s", (unsigned)runtimeConfig.issId, isfinite(rf.lastRawRssi) ? String(rf.lastRawRssi,1).c_str() : "--");
  line(b, 33);
  snprintf(b, sizeof(b), "RAW %lu OK %lu CRC %lu", (unsigned long)rf.rawPackets, (unsigned long)s.packetsOk, (unsigned long)s.crcErrors); line(b, 43);
  const LightningState ls = getLightningState();
  snprintf(b, sizeof(b), "MISS %lu BME %s AS %s", (unsigned long)s.packetsMissed, pressureSensorAvailable() ? "OK" : "scan", ls.detected ? "OK" : (ls.enabled ? "scan" : "off")); line(b, 53);
  if (networkProvisioningActive()) snprintf(b, sizeof(b), "AP %s", networkIp().c_str());
  else if (wifiOk) snprintf(b, sizeof(b), "WEB %s", networkIp().c_str());
  else snprintf(b, sizeof(b), "WiFi ricerca/recovery");
  line(b, 63);
#endif
}

void renderRawSearch() {
#if OLED_ENABLE
  const DavisRadioStatus &rf = getDavisRadioStatus();
  const MqttRuntimeStatus &mq = getMqttStatus();
  header("DAVIS RX RAW", networkConnected(), mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf);
  char b[56];

  snprintf(b, sizeof(b), "CH%u %.6f MHz", (unsigned)(rf.channel + 1U), rf.frequencyMhz); line(b, 23);
  if (!rf.haveRawPacket) {
    line("Attesa frame RF...", 34);
    line("Sync CB89 / 10 byte", 44);
    snprintf(b, sizeof(b), "RAW 0  rc %d", (int)rf.lastRadioError); line(b, 54);
    line("Ascolto Davis 868", 63);
    return;
  }

  const uint32_t ageSec = (millis() - rf.lastRawMs) / 1000UL;
  snprintf(b, sizeof(b), "RX#%lu CRC %s RSSI %.0f", (unsigned long)rf.rawPackets, rf.lastRawCrcOk ? "OK" : "KO", isfinite(rf.lastRawRssi) ? rf.lastRawRssi : 0.0f); line(b, 33);
  formatRaw5(b, sizeof(b), &rf.lastRaw[0]); line(b, 43);
  formatRaw5(b, sizeof(b), &rf.lastRaw[5]); line(b, 53);
  snprintf(b, sizeof(b), "age %lus  rc %d", (unsigned long)ageSec, (int)rf.lastRadioError); line(b, 63);
#endif
}

void renderEnvironment(const StationState &s) {
#if OLED_ENABLE
  const MqttRuntimeStatus &mq = getMqttStatus();
  const DavisRadioStatus &rf = getDavisRadioStatus();
  header("DAVIS METEO", networkConnected(), mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf);
  char b[56], t[16], h[16];
  valueOrDash(t,sizeof(t),s.outTempC,1,"C"); valueOrDash(h,sizeof(h),s.outHumidity,0,"%");
  snprintf(b,sizeof(b),"T %s   H %s",t,h); line(b,23);
  const float dew = calcDewPointC(s.outTempC,s.outHumidity);
  valueOrDash(t,sizeof(t),dew,1,"C"); snprintf(b,sizeof(b),"Dew %s",t); line(b,33);
  if (isfinite(s.uv) || isfinite(s.solarWm2)) snprintf(b,sizeof(b),"UV %.1f  Solar %.0f W/m2",isfinite(s.uv)?s.uv:0.0f,isfinite(s.solarWm2)?s.solarWm2:0.0f);
  else snprintf(b,sizeof(b),"UV --  Solar --"); line(b,43);
  if (s.locked) snprintf(b,sizeof(b),"ISS %u  BAT %s",(unsigned)(s.stationId+1U),s.batteryLow?"LOW":"OK");
  else snprintf(b,sizeof(b),"ISS --  BAT --");
  line(b,53);
  snprintf(b,sizeof(b),"Pkt %s  age %lus",davisPacketTypeName(s.lastPacketType),(unsigned long)(s.lastPacketMs?((millis()-s.lastPacketMs)/1000UL):0UL)); line(b,63);
#endif
}

void renderWindRain(const StationState &s) {
#if OLED_ENABLE
  const MqttRuntimeStatus &mq = getMqttStatus(); const DavisRadioStatus &rf = getDavisRadioStatus();
  header("VENTO / PIOGGIA", networkConnected(), mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf); char b[56];
  if (isfinite(s.windKmh)) snprintf(b,sizeof(b),"Wind %.1f Gust %.1f km/h",s.windKmh,isfinite(s.windGustKmh)?s.windGustKmh:0.0f); else snprintf(b,sizeof(b),"Wind -- Gust --"); line(b,23);
  if (isfinite(s.windDirDeg)) snprintf(b,sizeof(b),"Dir %.0f deg",s.windDirDeg); else snprintf(b,sizeof(b),"Dir --"); line(b,33);
  snprintf(b,sizeof(b),"Rain %.1f mm %.1f mm/h",s.rainDayMm,s.rainRateMmH); line(b,43);
  snprintf(b,sizeof(b),"Month %.1f Year %.1f",s.rainMonthMm,s.rainYearMm); line(b,53);
  snprintf(b,sizeof(b),"Max wind %.1f gust %.1f",isfinite(s.windDayMaxKmh)?s.windDayMaxKmh:0.0f,isfinite(s.gustDayMaxKmh)?s.gustDayMaxKmh:0.0f); line(b,63);
#endif
}

void renderPressure(const StationState &s) {
#if OLED_ENABLE
  const MqttRuntimeStatus &mq = getMqttStatus(); const DavisRadioStatus &rf = getDavisRadioStatus();
  header("BAROMETRO BME", networkConnected(), mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf); char b[56];
  const PressureSensorStatus &ps = getPressureSensorStatus();
  if (!ps.detected) {
    snprintf(b,sizeof(b),"BME280 ricerca %u",(unsigned)ps.detectFailures); line(b,24);
    line("I2C 0x76 / 0x77",34);
    snprintf(b,sizeof(b),"Errori %lu",(unsigned long)ps.readErrors); line(b,44);
    line("Retry non bloccante",54); return;
  }
  if (isfinite(s.pressureHpa)) snprintf(b,sizeof(b),"Sea %.1f hPa",s.pressureHpa); else snprintf(b,sizeof(b),"Sea -- hPa"); line(b,23);
  if (isfinite(s.pressureAbsoluteHpa)) snprintf(b,sizeof(b),"Abs %.1f hPa @0x%02X",s.pressureAbsoluteHpa,(unsigned)ps.address); else snprintf(b,sizeof(b),"Abs -- hPa @0x%02X",(unsigned)ps.address); line(b,33);
  if (s.pressureTrendValid) snprintf(b,sizeof(b),"Trend %+.1f/3h %s",s.pressureTrendHpa3h,pressureTrendName(s)); else snprintf(b,sizeof(b),"Trend acquisizione"); line(b,43);
  snprintf(b,sizeof(b),"%s",pressureForecastName(s)); line(b,53);
  if (isfinite(s.indoorTempC) && isfinite(s.indoorHumidity)) snprintf(b,sizeof(b),"Tin %.1fC Hin %.0f%%",s.indoorTempC,s.indoorHumidity); else snprintf(b,sizeof(b),"Tin --  Hin --"); line(b,63);
#endif
}

void renderRf(const StationState &s) {
#if OLED_ENABLE
  const MqttRuntimeStatus &mq = getMqttStatus(); const DavisRadioStatus &rf = getDavisRadioStatus();
  header("RF DAVIS FHSS", networkConnected(), mq.connected, rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf); char b[56];
  snprintf(b,sizeof(b),"%s CH%u %.6f",rf.synchronized?"SYNC":"SCAN",(unsigned)(rf.channel+1U),rf.frequencyMhz); line(b,23);
  if (isfinite(s.rssi)) snprintf(b,sizeof(b),"RSSI %.1f dBm ID %u",s.rssi,(unsigned)(s.stationId+1U)); else snprintf(b,sizeof(b),"RSSI -- ID --"); line(b,33);
  snprintf(b,sizeof(b),"OK %lu CRC %lu MISS %lu",(unsigned long)s.packetsOk,(unsigned long)s.crcErrors,(unsigned long)s.packetsMissed); line(b,43);
  snprintf(b,sizeof(b),"RAW %lu last %s",(unsigned long)rf.rawPackets,rf.haveRawPacket?(rf.lastRawCrcOk?"CRCOK":"CRCKO"):"--"); line(b,53);
  snprintf(b,sizeof(b),"%s RS%lu rc%d",davisPacketTypeName(s.lastPacketType),(unsigned long)s.resyncs,(int)rf.lastRadioError); line(b,63);
#endif
}

void renderLightning() {
#if OLED_ENABLE
  const LightningState ls=getLightningState(); const LightningConfig lc=getLightningConfig();
  const MqttRuntimeStatus &mq=getMqttStatus(); const DavisRadioStatus &rf=getDavisRadioStatus();
  header("AS3935 FULMINI",networkConnected(),mq.connected,rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf); char b[56];
  if(!ls.enabled){line("Sensore disabilitato",24);line("Config Web per attivare",34);return;}
  snprintf(b,sizeof(b),"Sens %s IRQ %s CAL %s",ls.detected?"OK":"KO",ls.irqOk?"OK":"KO",ls.calibrationOk?"OK":"CHK"); line(b,23);
  if(!ls.lastLightningMs) snprintf(b,sizeof(b),"Ultimo fulmine --"); else if(ls.distanceOutOfRange) snprintf(b,sizeof(b),"Ultimo >40 km E%lu",(unsigned long)ls.lastEnergy); else snprintf(b,sizeof(b),"Ultimo %u km E%lu",(unsigned)ls.lastDistanceKm,(unsigned long)ls.lastEnergy); line(b,33);
  snprintf(b,sizeof(b),"L%lu N%lu D%lu IRQ%lu",(unsigned long)ls.lightningTotal,(unsigned long)ls.noiseTotal,(unsigned long)ls.disturberTotal,(unsigned long)ls.irqTotal); line(b,43);
  snprintf(b,sizeof(b),"%s NF%u WD%u SP%u",lc.indoor?"IN":"OUT",(unsigned)lc.noiseFloor,(unsigned)lc.watchdogThreshold,(unsigned)lc.spikeRejection); line(b,53);
  snprintf(b,sizeof(b),"I2C 0x%02X GPIO %d",(unsigned)lc.i2cAddress,(int)lc.irqPin); line(b,63);
#endif
}

void renderSystem() {
#if OLED_ENABLE
  const MqttRuntimeStatus &mq=getMqttStatus(); const DavisRadioStatus &rf=getDavisRadioStatus(); const LightningState ls=getLightningState();
  header("GATEWAY STATUS",networkConnected(),mq.connected,rf.synchronized);
  oled.setFont(u8g2_font_5x8_tf); char b[56];
  if(networkProvisioningActive()) snprintf(b,sizeof(b),"AP %s",networkIp().c_str()); else if(networkConnected()) snprintf(b,sizeof(b),"IP %s",networkIp().c_str()); else snprintf(b,sizeof(b),"WiFi offline"); line(b,23);
  snprintf(b,sizeof(b),"MQTT %s pub %lu",mq.connected?"ON":(getMqttConfig().enabled?"WAIT":"OFF"),(unsigned long)mq.publishes); line(b,33);
  snprintf(b,sizeof(b),"BME %s AS3935 %s",pressureSensorAvailable()?"OK":"SCAN",ls.detected?"OK":(ls.enabled?"SCAN":"OFF")); line(b,43);
  snprintf(b,sizeof(b),"Heap %lu KB",(unsigned long)(ESP.getFreeHeap()/1024UL)); line(b,53);
  snprintf(b,sizeof(b),"Uptime %lu min FW %s",(unsigned long)(millis()/60000UL),FIRMWARE_VERSION); line(b,63);
#endif
}

bool staleOrSearching(const StationState &s) {
  const DavisRadioStatus &rf=getDavisRadioStatus();
  if(!rf.initialized || !rf.synchronized || !s.locked) return true;
  if(!s.lastPacketMs) return true;
  return (uint32_t)(millis()-s.lastPacketMs) > STALE_PACKET_MS;
}
}

bool initDisplay() {
#if OLED_ENABLE
  Wire.begin(I2C_SDA_PIN,I2C_SCL_PIN);
  Wire.setClock(100000UL);
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(80);
#endif
  Wire.beginTransmission(OLED_ADDRESS);
  const uint8_t probe=Wire.endTransmission();
  if(probe!=0){available=false;Serial.print(F("[OLED] non rilevato @0x"));Serial.println(OLED_ADDRESS,HEX);return false;}
  oled.setI2CAddress((uint8_t)(OLED_ADDRESS<<1));
  oled.setBusClock(100000UL);
  oled.begin();
  oled.setContrast(255);
  available=true;
  pageEpochMs=millis();
  searchPhaseEpochMs=pageEpochMs;
  displayBootMessage("ESP32 Davis Gateway","Avvio servizi...");
  Serial.print(F("[OLED] SSD1306 128x64 @0x"));Serial.println(OLED_ADDRESS,HEX);
  return true;
#else
  return false;
#endif
}

void displayBootMessage(const char *line1,const char *line2){
#if OLED_ENABLE
  if(!available)return;
  oled.clearBuffer();oled.setFont(u8g2_font_6x10_tf);oled.drawStr(0,11,"ESP32 DAVIS 868");oled.drawHLine(0,14,128);
  oled.setFont(u8g2_font_5x8_tf);if(line1)oled.drawStr(0,31,line1);if(line2)oled.drawStr(0,43,line2);
  char b[40];snprintf(b,sizeof(b),"FW %s",FIRMWARE_VERSION);oled.drawStr(0,58,b);oled.sendBuffer();
#else
  (void)line1;(void)line2;
#endif
}

void updateDisplay(const StationState &station){
#if OLED_ENABLE
  if(!available)return;
  const uint32_t now=millis();if((uint32_t)(now-lastRefreshMs)<REFRESH_MS)return;lastRefreshMs=now;
  oled.clearBuffer();
  if(staleOrSearching(station)){
    page=0;pageEpochMs=now;
    if((uint32_t)(now-searchPhaseEpochMs)>=SEARCH_PHASE_MS){searchPhaseEpochMs=now;searchPhase^=1U;}
    if(searchPhase==0U)renderSearch(station);else renderRawSearch();
  }else{
    searchPhase=0;searchPhaseEpochMs=now;
    if((uint32_t)(now-pageEpochMs)>=PAGE_INTERVAL_MS){pageEpochMs=now;page=(uint8_t)((page+1U)%PAGE_COUNT);}
    switch(page){case 0:renderEnvironment(station);break;case 1:renderWindRain(station);break;case 2:renderPressure(station);break;case 3:renderRf(station);break;case 4:renderLightning();break;default:renderSystem();break;}
  }
  oled.sendBuffer();
#else
  (void)station;
#endif
}

bool displayAvailable(){return available;}
uint8_t displayCurrentPage(){return page;}
