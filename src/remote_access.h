#pragma once

#include <Arduino.h>

struct RemoteAccessConfig {
  // The only installer-supplied value. Empty means remote access is disabled.
  String portalUrl;
};

struct RemoteAccessStatus {
  bool initialized = false;
  bool configured = false;
  bool approved = false;
  bool transportActive = false;
  String state;
  String deviceId;
  uint32_t enrollAttempts = 0;
  int lastEnrollHttpCode = 0;
  uint32_t wsAttempts = 0;
  uint32_t wsConnects = 0;
  uint32_t wsDisconnects = 0;
  uint32_t wsHandshakeFailures = 0;
  uint32_t requests = 0;
  uint32_t responses = 0;
  uint32_t heartbeatsSent = 0;
  uint32_t lastActivityMs = 0;
  uint32_t lastWsAttemptMs = 0;
  String wsHost;
  String wsPath;
  String lastWsEvent;
  String lastError;
};

void initRemoteAccess();
RemoteAccessConfig getRemoteAccessConfig();
RemoteAccessStatus getRemoteAccessStatus();

// Save only the generic HTTPS portal base URL. Device identity and token are
// generated/managed by firmware and are never installer-editable.
bool saveRemoteAccessPortalUrl(const String &portalUrl);

// Disable/forget the portal URL, but deliberately preserve the per-device
// token so identity remains stable if the same device is configured again.
bool resetRemoteAccessConfig();

// Force a new enrollment/reconnect attempt without changing identity.
void retryRemoteAccessNow();

String remoteAccessConfigJson();
String remoteAccessStatusJson();
String remoteDefaultDeviceId();
