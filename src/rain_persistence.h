#pragma once
#include "station_state.h"

void initRainPersistence(StationState &station);
void serviceRainPersistence(StationState &station);
void forceSaveRainPersistence(const StationState &station);
