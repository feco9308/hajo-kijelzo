#pragma once
#include "stm32g4xx_hal.h"
#include <stdbool.h>

/* Integer units keep the SWD interface independent of floating point layout. */
typedef struct {
    int32_t awa_degrees;
    uint32_t aws_tenths;
    uint32_t tws_tenths;
    uint32_t sog_tenths;
    uint32_t cog_degrees;
    int32_t xte_hundredths;
    uint32_t distance_tenths;
    uint32_t waypoint;
} BoatDemoData;

BoatDemoData boat_demo_get_data(void);
bool boat_demo_data_valid(const BoatDemoData *data);
HAL_StatusTypeDef boat_demo_set_data(const BoatDemoData *data);
HAL_StatusTypeDef boat_demo_show(void);
