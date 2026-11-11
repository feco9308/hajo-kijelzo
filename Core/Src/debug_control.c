#include "debug_control.h"
#include "board.h"
#include "sharp_ls044q7dh01.h"
#include "usb_diag.h"
#include "usb_manual.h"

volatile DebugControl lcd_debug;
extern volatile uint32_t test_stage;
extern volatile HAL_StatusTypeDef last_hal_status;

void debug_control_poll(void)
{
    uint32_t command = lcd_debug.request;
    if (command == 0U) return;
    lcd_debug.result = HAL_BUSY;
    usb_auto_running = 0; /* Manual SWD requests always stop automatic writes. */
    HAL_StatusTypeDef s = HAL_OK;
    switch (command) {
    case 1: s = sharp_clear(); test_stage = 11; break;
    case 2: s = sharp_fill(true); test_stage = 12; break;
    case 3: s = sharp_fill(false); test_stage = 13; break;
    case 4: s = sharp_test_pattern(); test_stage = 14; break;
    case 5: s = sharp_test_pattern_inverted(); test_stage = 16; break;
    case 6:
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_SET);
        HAL_Delay(1);
        board_wait_next_extcom_rise();
        test_stage = 15;
        break;
    case 7:
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET);
        test_stage = 10;
        break;
    case 8: break; /* Stop: leave pixels and DISP unchanged. */
    default: s = HAL_ERROR; break;
    }
    last_hal_status = s;
    diag_last_hal_status = s;
    if (s != HAL_OK) {
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET);
        test_stage = 0xFFFFFFFFU;
    }
    lcd_debug.result = (uint32_t)s;
    lcd_debug.last_command = command;
    ++lcd_debug.completed;
    __DMB();
    lcd_debug.request = 0;
}
