#include <Arduino.h>

#include "board_config.h"
#include "config.h"
#include "davis_radio.h"
#include "lightning_manager.h"
#include "meteobridge_client.h"
#include "mqtt_publisher.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "rain_persistence.h"
#include "runtime_config.h"
#include "station_state.h"
#include "web_manager.h"

StationState station;
bool dashboardInitialized=false;

void setup(){
  Serial.begin(115200);delay(150);Serial.println();
  Serial.println(F("========================================"));
  Serial.print(F(" ESP32 Davis Weather Gateway "));Serial.println(FIRMWARE_VERSION);
  Serial.print(F(" Board: "));Serial.println(BOARD_NAME);
  Serial.println(F(" RF engine: DAVIS ONLY - Vantage Pro2 EU 868 MHz FHSS / 2-FSK"));
  Serial.println(F(" Services: BME280 + AS3935 + MQTT + Web UI + HTTP upload"));
  Serial.println(F("========================================"));

  pinMode(BOARD_LED_PIN,OUTPUT);digitalWrite(BOARD_LED_PIN,BOARD_LED_OFF);
  loadRuntimeConfig();initRainPersistence(station);initNetwork();
  const bool bmeOk=initPressureSensor();Serial.print(F("[BME] "));Serial.println(bmeOk?F("ready"):F("retry mode"));
  initLightning();initMqtt();
  const bool rfOk=initDavisRadio();Serial.print(F("[RF] "));Serial.println(rfOk?F("ready"):F("initialization failed"));
}

void loop(){
  // Davis RF always has first service priority. Auxiliary services never alter
  // the radio mode, hop table or packet decoder.
  serviceDavisRadio(station,runtimeConfig.issId,runtimeConfig.rainMmPerTip);
  servicePressureSensor(station);serviceRainPersistence(station);serviceLightning();
  serviceNetwork();
  if(networkConnected()&&!networkProvisioningActive()&&!dashboardInitialized){initWeb(station);dashboardInitialized=true;Serial.print(F("[WEB] Dashboard: http://"));Serial.println(networkIp());}
  if(dashboardInitialized)serviceWeb();
  serviceMqtt(station);serviceWeatherUpload(station);

  static uint32_t ledMs=0;static bool ledActive=false;
  if(digitalRead(BOARD_LED_PIN)==BOARD_LED_ON&&!ledActive){ledMs=millis();ledActive=true;}
  if(ledActive&&(uint32_t)(millis()-ledMs)>80UL){digitalWrite(BOARD_LED_PIN,BOARD_LED_OFF);ledActive=false;}
  delay(1);
}
