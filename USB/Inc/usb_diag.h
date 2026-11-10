#pragma once
#include "stm32g4xx_hal.h"
#include <stdbool.h>
#define USB_COMMAND_SIZE 48U
HAL_StatusTypeDef usb_diag_init(void);
/* Main context only; nonblocking, drops reports if USB is absent or busy. */
void usb_diag_poll(void);
bool usb_diag_take_command(char command[USB_COMMAND_SIZE]);
bool usb_diag_reply_available(void);
bool usb_diag_reply(const char *text);
void usb_diag_reply_status(void);
extern volatile uint32_t diag_rx_dropped;
extern volatile uint32_t diag_last_hal_status;
extern volatile uint32_t diag_usb_status;
extern volatile uint32_t diag_reset_flags;
