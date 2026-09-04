#pragma once

#include <Arduino.h>
#include "station_state.h"

bool initDisplay();
void updateDisplay(const StationState &station);
void displayBootMessage(const char *line1, const char *line2 = nullptr);
bool displayAvailable();
uint8_t displayCurrentPage();
