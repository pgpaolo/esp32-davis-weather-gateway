#pragma once
#include "station_state.h"

bool initPressureSensor();
void servicePressureSensor(StationState &station);
bool pressureSensorAvailable();
