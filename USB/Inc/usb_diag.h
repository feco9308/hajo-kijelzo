#pragma once
#include "stm32g4xx_hal.h"
HAL_StatusTypeDef usb_diag_init(void);
/* Main context only; nonblocking, drops reports if USB is absent or busy. */
void usb_diag_poll(void);
extern volatile uint32_t diag_last_hal_status;
extern volatile uint32_t diag_usb_status;
extern volatile uint32_t diag_reset_flags;
