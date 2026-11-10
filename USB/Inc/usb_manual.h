#pragma once
#include <stdint.h>
/* Service one queued command in MAIN context. Never run from an IRQ. */
void usb_manual_poll(void);
void usb_auto_poll(void);
extern volatile uint8_t usb_auto_running;
