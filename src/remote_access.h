#pragma once

#include <Arduino.h>

struct RemoteAccessConfig {
  bool enabled = false;
  String relayUrl;
  String deviceId;
  String token;
  String caCertificate;
  uint16_t heartbeatSec = 60;
  bool allowRemoteAdmin = false;
};

struct RemoteAccessStatus {
  bool initialized = false;
  bool configured = false;
  bool transportActive = false;
  String state;
  String lastError;
};

void initRemoteAccess();
RemoteAccessConfig getRemoteAccessConfig();
const RemoteAccessStatus &getRemoteAccessStatus();
bool saveRemoteAccessConfig(const RemoteAccessConfig &cfg, bool replaceToken, bool replaceCa);
bool resetRemoteAccessConfig();
String remoteAccessConfigJson();
String remoteAccessStatusJson();
String remoteDefaultDeviceId();
