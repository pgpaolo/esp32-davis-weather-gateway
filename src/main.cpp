#include <Arduino.h>

#include "board_config.h"
#include "config.h"
#include "davis_radio.h"
#include "meteobridge_client.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "rain_persistence.h"
#include "runtime_config.h"
#include "station_state.h"
#include "web_manager.h"

StationState station;
bool dashboardInitialized=false;

void setup(){
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.print(F(" ESP32 Davis Weather Gateway ")); Serial.println(FIRMWARE_VERSION);
  Serial.print(F(" Board: ")); Serial.println(BOARD_NAME);
  Serial.println(F(" RF: Davis Vantage Pro2 EU 868 MHz FHSS / 2-FSK"));
  Serial.println(F(" Network: captive provisioning + NVS"));
  Serial.println(F("========================================"));

  pinMode(BOARD_LED_PIN,OUTPUT);
  digitalWrite(BOARD_LED_PIN,BOARD_LED_OFF);

  loadRuntimeConfig();
  initRainPersistence(station);
  initNetwork();
  const bool bmeOk=initPressureSensor();
  Serial.print(F("[BME] "));Serial.println(bmeOk?F("ready"):F("not detected"));
  const bool rfOk=initDavisRadio();
  Serial.print(F("[RF] "));Serial.println(rfOk?F("ready"):F("initialization failed"));
}

void loop(){
  // RF is serviced first to minimize packet latency.
  serviceDavisRadio(station,runtimeConfig.issId,runtimeConfig.rainMmPerTip);
  servicePressureSensor(station);
  serviceRainPersistence(station);

  serviceNetwork();

  if(networkConnected() && !networkProvisioningActive() && !dashboardInitialized){
    initWeb(station);
    dashboardInitialized=true;
    Serial.print(F("[WEB] Dashboard: http://"));Serial.println(networkIp());
  }
  if(dashboardInitialized) serviceWeb();

  serviceWeatherUpload(station);

  static uint32_t ledMs=0;
  static bool ledActive=false;
  if(digitalRead(BOARD_LED_PIN)==BOARD_LED_ON && !ledActive){ledMs=millis();ledActive=true;}
  if(ledActive && (uint32_t)(millis()-ledMs)>80UL){digitalWrite(BOARD_LED_PIN,BOARD_LED_OFF);ledActive=false;}

  delay(1);
}
