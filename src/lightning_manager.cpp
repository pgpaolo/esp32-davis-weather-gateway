#include "lightning_manager.h"

#include <AS3935I2C.h>
#include <Preferences.h>
#include <Wire.h>

#include "board_config.h"
#include "mqtt_publisher.h"

namespace {
#if defined(BOARD_T3_S3_SX1276)
constexpr bool DEFAULT_ENABLED=false;
constexpr int8_t DEFAULT_IRQ=-1; // enable only after confirming a free GPIO on the board revision
#else
constexpr bool DEFAULT_ENABLED=true;
constexpr int8_t DEFAULT_IRQ=34;
#endif
constexpr uint32_t IRQ_SETTLE_US=2000UL;
constexpr uint32_t MQTT_STATE_INTERVAL_MS=30000UL;
constexpr uint8_t DISTANCE_OUT_OF_RANGE=0x3FU;

Preferences prefs;
LightningConfig cfg;
LightningState state;
AS3935I2C *sensor=nullptr;
int8_t activeIrq=-1;
volatile bool irqPending=false;
volatile uint32_t irqRaisedUs=0;
uint32_t lastMqttStateMs=0;

void IRAM_ATTR onLightningIrq(){ irqRaisedUs=micros(); irqPending=true; }

LightningConfig defaults(){
  LightningConfig c; c.enabled=DEFAULT_ENABLED; c.irqPin=DEFAULT_IRQ; return c;
}

bool minStrikeValid(uint8_t v){ return v==1||v==5||v==9||v==16; }
uint8_t minStrikeCode(uint8_t v){
  switch(v){case 5:return AS3935MI::AS3935_MNL_5;case 9:return AS3935MI::AS3935_MNL_9;case 16:return AS3935MI::AS3935_MNL_16;default:return AS3935MI::AS3935_MNL_1;}
}

bool pinReserved(int8_t p){
  if(p<0) return false;
  if(p==I2C_SDA_PIN||p==I2C_SCL_PIN||p==RADIO_SCLK_PIN||p==RADIO_MISO_PIN||p==RADIO_MOSI_PIN||p==RADIO_CS_PIN||p==RADIO_DIO0_PIN||p==RADIO_RST_PIN||p==RADIO_DIO1_PIN||p==BOARD_LED_PIN) return true;
#if !defined(BOARD_T3_S3_SX1276)
  if(p>=6&&p<=11) return true;
#endif
  return false;
}

void normalize(LightningConfig &c){
  if(c.i2cAddress<1||c.i2cAddress>3)c.i2cAddress=3;
  if(c.noiseFloor>7)c.noiseFloor=7;
  if(c.watchdogThreshold>15)c.watchdogThreshold=15;
  if(c.spikeRejection>15)c.spikeRejection=15;
  if(!minStrikeValid(c.minStrikes))c.minStrikes=1;
  if(c.tuningCap>15)c.tuningCap=15;
}

void loadConfig(){
  const LightningConfig d=defaults(); cfg=d;
  if(!prefs.begin("as3935cfg",true)){normalize(cfg);return;}
  cfg.enabled=prefs.getBool("enabled",d.enabled); cfg.indoor=prefs.getBool("indoor",d.indoor);
  cfg.i2cAddress=prefs.getUChar("addr",d.i2cAddress); cfg.irqPin=prefs.getChar("irq",d.irqPin);
  cfg.noiseFloor=prefs.getUChar("noise",d.noiseFloor); cfg.watchdogThreshold=prefs.getUChar("watch",d.watchdogThreshold);
  cfg.spikeRejection=prefs.getUChar("spike",d.spikeRejection); cfg.minStrikes=prefs.getUChar("min",d.minStrikes);
  cfg.maskDisturbers=prefs.getBool("mask",d.maskDisturbers); cfg.tuningCap=prefs.getUChar("cap",d.tuningCap); cfg.autoTune=prefs.getBool("auto",d.autoTune);
  prefs.end(); normalize(cfg);
}

void stopSensor(){
  if(activeIrq>=0){detachInterrupt(digitalPinToInterrupt((uint8_t)activeIrq));activeIrq=-1;}
  irqPending=false;
  if(sensor){sensor->writePowerDown(true);delete sensor;sensor=nullptr;}
  state.detected=false; state.irqOk=false;
}

bool validate(const LightningConfig &in){
  LightningConfig n=in; normalize(n);
  if(n.i2cAddress!=in.i2cAddress||n.noiseFloor!=in.noiseFloor||n.watchdogThreshold!=in.watchdogThreshold||n.spikeRejection!=in.spikeRejection||n.minStrikes!=in.minStrikes||n.tuningCap!=in.tuningCap)return false;
  if(n.enabled&&(n.irqPin<0||pinReserved(n.irqPin)))return false;
#if defined(BOARD_T3_S3_SX1276)
  if(n.irqPin>48)return false;
#else
  if(n.irqPin>39)return false;
#endif
  return true;
}

bool configure(){
  stopSensor(); state.enabled=cfg.enabled; state.calibrationOk=false; state.resonanceHz=0;
  if(!cfg.enabled){Serial.println(F("[AS3935] disabled"));return true;}
  if(!validate(cfg)){Serial.println(F("[AS3935] invalid configuration"));return false;}
  pinMode((uint8_t)cfg.irqPin,INPUT);
  sensor=new AS3935I2C(cfg.i2cAddress,(uint8_t)cfg.irqPin);
  if(!sensor||!sensor->begin()||!sensor->checkConnection()){
    Serial.print(F("[AS3935] not detected @0x"));Serial.println(cfg.i2cAddress,HEX);
    if(sensor){delete sensor;sensor=nullptr;} return false;
  }
  state.detected=true; state.irqOk=sensor->checkIRQ();
  int32_t freq=0; bool resonanceOk=false;
  if(cfg.autoTune)resonanceOk=sensor->calibrateResonanceFrequency(freq,AS3935MI::AS3935_DR_16);
  else{sensor->writeAntennaTuning(cfg.tuningCap);resonanceOk=sensor->validateCurrentResonanceFrequency(freq);}
  state.resonanceHz=freq; state.calibrationOk=resonanceOk&&sensor->calibrateRCO();
  sensor->writeAFE(cfg.indoor?AS3935MI::AS3935_INDOORS:AS3935MI::AS3935_OUTDOORS);
  sensor->writeNoiseFloorThreshold(cfg.noiseFloor); sensor->writeWatchdogThreshold(cfg.watchdogThreshold);
  sensor->writeSpikeRejection(cfg.spikeRejection); sensor->writeMinLightnings(minStrikeCode(cfg.minStrikes));
  sensor->writeMaskDisturbers(cfg.maskDisturbers); sensor->clearStatistics();
  attachInterrupt(digitalPinToInterrupt((uint8_t)cfg.irqPin),onLightningIrq,RISING); activeIrq=cfg.irqPin;
  Serial.print(F("[AS3935] ready addr=0x"));Serial.print(cfg.i2cAddress,HEX);Serial.print(F(" irq="));Serial.print(cfg.irqPin);Serial.print(F(" cal="));Serial.println(state.calibrationOk?F("OK"):F("CHECK"));
  return true;
}
} // namespace

void initLightning(){loadConfig();state=LightningState{};state.enabled=cfg.enabled;configure();}

void serviceLightning(){
  const uint32_t now=millis();
  if(cfg.enabled&&sensor&&state.detected&&irqPending&&((uint32_t)(micros()-irqRaisedUs)>=IRQ_SETTLE_US)){
    noInterrupts();irqPending=false;interrupts();
    state.irqTotal++;state.lastEventMs=now;const uint8_t src=sensor->readInterruptSource();state.lastInterruptSource=src;
    if(src==AS3935MI::AS3935_INT_NH)state.noiseTotal++;
    else if(src==AS3935MI::AS3935_INT_D)state.disturberTotal++;
    else if(src==AS3935MI::AS3935_INT_L){
      state.lightningTotal++;state.lastLightningMs=now;const uint8_t d=sensor->readStormDistance();state.distanceOutOfRange=(d==DISTANCE_OUT_OF_RANGE);state.lastDistanceKm=state.distanceOutOfRange?0:d;state.lastEnergy=sensor->readEnergy();
      Serial.print(F("[AS3935] lightning #"));Serial.print(state.lightningTotal);Serial.print(F(" distance="));if(state.distanceOutOfRange)Serial.print(F(">40"));else Serial.print(state.lastDistanceKm);Serial.print(F(" km energy="));Serial.println(state.lastEnergy);
    }
    mqttPublishLightningEvent(lightningStateJson());
  }
  if((uint32_t)(now-lastMqttStateMs)>=MQTT_STATE_INTERVAL_MS){lastMqttStateMs=now;mqttPublishLightningState(lightningStateJson());}
}

LightningConfig getLightningConfig(){return cfg;}
LightningState getLightningState(){return state;}

bool saveLightningConfig(const LightningConfig &input){
  if(!validate(input))return false;LightningConfig n=input;normalize(n);
  if(!prefs.begin("as3935cfg",false))return false;
  prefs.putBool("enabled",n.enabled);prefs.putBool("indoor",n.indoor);prefs.putUChar("addr",n.i2cAddress);prefs.putChar("irq",n.irqPin);
  prefs.putUChar("noise",n.noiseFloor);prefs.putUChar("watch",n.watchdogThreshold);prefs.putUChar("spike",n.spikeRejection);prefs.putUChar("min",n.minStrikes);
  prefs.putBool("mask",n.maskDisturbers);prefs.putUChar("cap",n.tuningCap);prefs.putBool("auto",n.autoTune);prefs.end();cfg=n;return configure();
}

bool resetLightningConfig(){if(!prefs.begin("as3935cfg",false))return false;const bool ok=prefs.clear();prefs.end();if(!ok)return false;cfg=defaults();return configure();}
bool reinitializeLightning(){return configure();}

const char *lightningInterruptName(uint8_t source){switch(source){case AS3935MI::AS3935_INT_NH:return "noise";case AS3935MI::AS3935_INT_D:return "disturber";case AS3935MI::AS3935_INT_L:return "lightning";case AS3935MI::AS3935_INT_DUPDATE:return "distance_update";default:return "unknown";}}

String lightningConfigJson(){
  String o;o.reserve(380);o="{\"enabled\":";o+=cfg.enabled?"true":"false";o+=",\"indoor\":";o+=cfg.indoor?"true":"false";o+=",\"i2c_address\":"+String(cfg.i2cAddress)+",\"irq_pin\":"+String(cfg.irqPin)+",\"noise_floor\":"+String(cfg.noiseFloor)+",\"watchdog_threshold\":"+String(cfg.watchdogThreshold)+",\"spike_rejection\":"+String(cfg.spikeRejection)+",\"min_strikes\":"+String(cfg.minStrikes)+",\"mask_disturbers\":";o+=cfg.maskDisturbers?"true":"false";o+=",\"tuning_cap\":"+String(cfg.tuningCap)+",\"auto_tune\":";o+=cfg.autoTune?"true":"false";o+="}";return o;
}

String lightningStateJson(){
  String o;o.reserve(620);o="{\"enabled\":";o+=state.enabled?"true":"false";o+=",\"detected\":";o+=state.detected?"true":"false";o+=",\"irq_ok\":";o+=state.irqOk?"true":"false";o+=",\"calibration_ok\":";o+=state.calibrationOk?"true":"false";o+=",\"resonance_hz\":"+String(state.resonanceHz)+",\"irq_total\":"+String(state.irqTotal)+",\"noise_total\":"+String(state.noiseTotal)+",\"disturber_total\":"+String(state.disturberTotal)+",\"lightning_total\":"+String(state.lightningTotal)+",\"last_event_ms\":"+String(state.lastEventMs)+",\"last_lightning_ms\":"+String(state.lastLightningMs)+",\"last_type\":\""+String(state.lastEventMs?lightningInterruptName(state.lastInterruptSource):"none")+"\"";if(!state.lastLightningMs||state.distanceOutOfRange)o+=",\"last_distance_km\":null";else o+=",\"last_distance_km\":"+String(state.lastDistanceKm);o+=",\"distance_out_of_range\":";o+=state.distanceOutOfRange?"true":"false";o+=",\"last_energy\":"+String(state.lastEnergy)+"}";return o;
}
