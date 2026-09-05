#pragma once

#include <Arduino.h>
#include <WebServer.h>

void registerFirmwareUpdateRoutes(WebServer &server);
void serviceFirmwareUpdate();
String firmwareUpdateStatusJson();

bool firmwareRemoteBegin(size_t imageSize, const String &sha256Hex, String &error);
bool firmwareRemoteWrite(uint32_t sequence, const uint8_t *data, size_t len, String &error);
bool firmwareRemoteEnd(String &error);
void firmwareRemoteAbort(const String &reason = String());
bool firmwareUpdateInProgress();
