#pragma once
#include <stdint.h>

/* SWD debugger writes request only, while the CPU keeps running.
 * Main owns the other fields. One outstanding request at a time.
 * Completion: completed increments, then request becomes zero.
 * HAL_OK means MCU transmission succeeded, not LCD acknowledgement. */
typedef struct {
    uint32_t request;
    uint32_t result;
    uint32_t completed;
    uint32_t last_command;
} DebugControl;
extern volatile DebugControl lcd_debug;
void debug_control_poll(void);
