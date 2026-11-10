#pragma once
#include "stm32g4xx_hal.h"
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim3;
void board_delay_us(uint32_t us);
void board_wait_extcom_slot(void);
HAL_StatusTypeDef board_extcom_start(void);
void Error_Handler(void);
