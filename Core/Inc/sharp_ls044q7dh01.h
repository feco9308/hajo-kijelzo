#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SHARP_WIDTH 320U
#define SHARP_HEIGHT 240U
#define SHARP_LINE_BYTES 40U
/* Blocking API, main context only. May wait up to ~702 ms for COM-safe phase.
 * No framebuffer, no heap.
 * line: 1..240. data: 40 bytes; bit0 is the first pixel of each byte.
 * 1 = white, 0 = black. Returns HAL_ERROR for invalid arguments.
 */
HAL_StatusTypeDef sharp_init(void);
HAL_StatusTypeDef sharp_clear(void);
HAL_StatusTypeDef sharp_fill(bool white);
HAL_StatusTypeDef sharp_write_line(uint16_t line, const uint8_t *data);
HAL_StatusTypeDef sharp_test_pattern(void);
HAL_StatusTypeDef sharp_test_pattern_inverted(void);
HAL_StatusTypeDef sharp_keepalive(void);
#ifdef __cplusplus
}
#endif
