#pragma once
#include <Arduino.h>

void initNetwork();
void serviceNetwork();
bool networkConnected();
String networkIp();
String networkSsid();
String networkModeName();
bool networkProvisioningActive();
String networkProvisioningSsid();
void startNetworkProvisioning(bool manual = true);
void stopNetworkProvisioning();
