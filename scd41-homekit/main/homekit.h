#pragma once

#include <stdbool.h>

int update_hap_climate(float temperature, float humidity, float co2);
int update_hap_occupancy(int occupancy);
int start_homekit(void);
