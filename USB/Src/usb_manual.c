#include "usb_manual.h"
#include "usb_diag.h"
#include "board.h"
#include "sharp_ls044q7dh01.h"
#include <string.h>

extern volatile uint32_t test_stage;
extern volatile uint32_t test_line;
extern volatile HAL_StatusTypeDef last_hal_status;
volatile uint8_t usb_auto_running;
static uint8_t auto_phase;
static uint32_t auto_due;

void usb_auto_poll(void)
{
    if (!usb_auto_running || (int32_t)(HAL_GetTick() - auto_due) < 0) return;
    /* Full white, hold for 2 seconds, full black, hold for 2 seconds.
     * DISP stays enabled and the independent hardware COM timer runs.
     * A manual image command or stop disables automatic writes. */
    bool white = (auto_phase == 0U);
    HAL_StatusTypeDef s = sharp_fill(white);
    test_stage = white ? 12U : 13U;
    auto_phase = white ? 1U : 0U;
    auto_due = HAL_GetTick() + 2000U;
    last_hal_status = s;
    diag_last_hal_status = s;
    if (s != HAL_OK) {
        usb_auto_running = 0;
        test_stage = 0xFFFFFFFFU;
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET);
        usb_diag_reply("ERR auto SPI/HAL; DISP forced LOW\r\n");
    }
}

void usb_manual_poll(void)
{
    if (!usb_diag_reply_available()) return;
    char command[USB_COMMAND_SIZE];
    if (!usb_diag_take_command(command)) return;
    /* Trim spaces; RX already normalizes ASCII case and CR/LF. */
    char *c = command;
    while (*c == ' ') ++c;
    size_t n = strlen(c);
    while (n && c[n - 1] == ' ') c[--n] = 0;
    if (n == 0) return;
    if (!strcmp(c, "help") || !strcmp(c, "?")) {
        usb_diag_reply("COMMANDS: clear black white pattern invert checkerboard off on auto stop status help\r\n");
        return;
    }
    if (!strcmp(c, "status")) { usb_diag_reply_status(); return; }
    if (!strcmp(c, "!overflow")) {
        usb_diag_reply("ERR line-too-long (maximum 47 ASCII characters)\r\n");
        return;
    }
    if (!strcmp(c, "stop")) {
        usb_auto_running = 0;
        usb_diag_reply("OK stop (current image held)\r\n");
        return;
    }
    if (!strcmp(c, "auto")) {
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_SET);
        HAL_Delay(1); board_wait_next_extcom_rise();
        auto_phase = 0; auto_due = HAL_GetTick(); usb_auto_running = 1;
        usb_diag_reply("OK auto (white 2s, black 2s; stop holds image)\r\n");
        return;
    }

    HAL_StatusTypeDef s = HAL_OK;
    const char *reply;
    if (!strcmp(c, "clear") || !strcmp(c, "torles")) {
        usb_auto_running = 0;
        s = sharp_clear(); test_stage = 11; reply = "OK clear HAL=0\r\n";
    } else if (!strcmp(c, "black") || !strcmp(c, "fekete")) {
        usb_auto_running = 0;
        s = sharp_fill(false); test_stage = 13; reply = "OK black HAL=0\r\n";
    } else if (!strcmp(c, "white") || !strcmp(c, "feher")) {
        usb_auto_running = 0;
        s = sharp_fill(true); test_stage = 12; reply = "OK white HAL=0\r\n";
    } else if (!strcmp(c, "pattern")) {
        usb_auto_running = 0;
        s = sharp_test_pattern(); test_stage = 14; reply = "OK pattern HAL=0\r\n";
    } else if (!strcmp(c, "invert")) {
        usb_auto_running = 0;
        s = sharp_test_pattern_inverted(); test_stage = 16; reply = "OK invert HAL=0\r\n";
    } else if (!strcmp(c, "checkerboard") || !strcmp(c, "sakk")) {
        usb_auto_running = 0;
        s = sharp_checkerboard(); test_stage = 17; reply = "OK checkerboard HAL=0\r\n";
    } else if (!strcmp(c, "off")) {
        usb_auto_running = 0;
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET);
        test_stage = 10; reply = "OK off DISP=0 (external COM timer continues)\r\n";
    } else if (!strcmp(c, "on")) {
        usb_auto_running = 0;
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_SET);
        HAL_Delay(1); board_wait_next_extcom_rise();
        test_stage = 15; reply = "OK on DISP=1 (image memory unchanged)\r\n";
    } else {
        usb_diag_reply("ERR unknown-command; type help\r\n");
        return;
    }
    last_hal_status = s;
    diag_last_hal_status = s;
    if (s != HAL_OK) {
        HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET);
        test_stage = 0xFFFFFFFFU;
        usb_diag_reply("ERR SPI/HAL; DISP forced LOW; type status\r\n");
    } else {
        usb_diag_reply(reply);
    }
}
