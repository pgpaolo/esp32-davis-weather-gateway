#pragma once
#include <Arduino.h>

void initNetwork();
void serviceNetwork();
bool networkConnected();
String networkIp();
