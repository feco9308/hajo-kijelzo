#pragma once
#include "stm32g4xx_hal.h"
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim3;
void board_delay_us(uint32_t us);
void Error_Handler(void);
