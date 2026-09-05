#pragma once

#include <Arduino.h>

// Persistent ON/OFF control for the onboard SSD1306. The display hardware is
// initialized by display_manager; this module only controls panel power state.
void initDisplayControl(bool displayDetected);
bool displayControlAvailable();
bool displayControlEnabled();
bool setDisplayControlEnabled(bool enabled);
String displayControlJson();
