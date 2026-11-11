#pragma once
#include "stm32g4xx_hal.h"
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim3;
void board_delay_us(uint32_t us);
void Error_Handler(void);

/* Actual user wiring: PB0 -> LCD pin4, PB1 -> LCD pin5. */
#define LCD_DISP_PIN GPIO_PIN_1
#define LCD_EXTCOM_PIN GPIO_PIN_0
