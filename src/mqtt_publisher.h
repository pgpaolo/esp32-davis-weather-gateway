#pragma once
#include <Arduino.h>
#include "station_state.h"

enum class MqttTlsMode : uint8_t { Off=0, CaVerified=1, Insecure=2 };

struct MqttRuntimeConfig {
  bool enabled = false;
  String broker;
  uint16_t port = 1883;
  String user;
  String password;
  String clientId = "DavisGateway";
  String baseTopic = "davis-gateway";
  MqttTlsMode tlsMode = MqttTlsMode::Off;
  String caCertificate;
  uint32_t publishIntervalMs = 10000UL;
};

struct MqttRuntimeStatus {
  bool initialized = false;
  bool connected = false;
  uint32_t connectAttempts = 0;
  uint32_t connects = 0;
  uint32_t publishes = 0;
  uint32_t lastConnectMs = 0;
  uint32_t lastPublishMs = 0;
  String lastError;
};

void initMqtt();
void serviceMqtt(const StationState &station);
MqttRuntimeConfig getMqttConfig();
const MqttRuntimeStatus &getMqttStatus();
bool saveMqttConfig(const MqttRuntimeConfig &cfg, bool replacePassword, bool replaceCaCertificate);
bool resetMqttConfig();
String mqttConfigJson();
String mqttStatusJson();
const char *mqttTlsModeName(MqttTlsMode mode);
void mqttPublishLightningState(const String &json);
void mqttPublishLightningEvent(const String &json);
