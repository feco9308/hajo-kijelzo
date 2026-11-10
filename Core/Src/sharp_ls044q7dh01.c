/* Own driver for LS044Q7DH01, Sharp LD-2025315 (2025-03-25),
 * sections 6-2, 6-3, 6-5, 6-6 and 6-8.
 * SPI mode 0, hardware LSB-first. EXTMODE is wired HIGH, so M1 stays 0.
 * SCS is active HIGH. No other task/interrupt may use SPI1 or these pins.
 */
#include "sharp_ls044q7dh01.h"
#include "board.h"
#include <string.h>

static void begin(void)
{
    /* Always establish inactive SCS and idle clocks before pin muxing.
     * Serialize all calls in main context; no nested transactions. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    board_spi_active(0);
    board_wait_extcom_slot();
    board_delay_us(2); /* SCS LOW >=1 us even between adjacent API calls. */
    board_spi_active(1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    board_delay_us(4); /* SCS setup >= 3 us. */
}

static HAL_StatusTypeDef end(HAL_StatusTypeDef status)
{
    /* HAL blocking transmit waits for BSY to clear before returning.
     * On timeout, stop the SPI clock while CS is still asserted. */
    if (status != HAL_OK) __HAL_SPI_DISABLE(&hspi1);
    board_delay_us(2); /* SCS hold >= 1 us. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    board_spi_active(0); /* Physically force SCLK and SI LOW between packets. */
    board_delay_us(2);
    return status;
}

static HAL_StatusTypeDef tx(const uint8_t *p, uint16_t n)
{
    return HAL_SPI_Transmit(&hspi1, (uint8_t *)p, n, 100);
}

static HAL_StatusTypeDef command(uint8_t cmd)
{
    /* 3 mode bits + >13 transfer clocks: use 24 clocks, not just 16.
     * All Clear M2 is bit2 because the peripheral sends LSB first. */
    uint8_t packet[3] = {cmd, 0, 0};
    begin();
    return end(tx(packet, sizeof packet));
}

HAL_StatusTypeDef sharp_clear(void) { return command(0x04); }
HAL_StatusTypeDef sharp_keepalive(void) { return command(0x00); }

HAL_StatusTypeDef sharp_init(void)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    /* Board has no LCD power switch/power-good input. Both LCD rails must
     * already be stable before entering here. Firmware cannot enforce this. */
    HAL_Delay(100);
    HAL_StatusTypeDef s = sharp_clear();
    if (s != HAL_OK) return s;
    /* LD-2025315: >=one All Clear; TA >=100 us after initialization.
     * Keep a conservative 50 ms guard after the completed command. */
    HAL_Delay(50);
    /* Start the external waveform before enabling DISP, as requested.
     * PWM2 begins LOW; its first rising edge occurs 500 ms after start. */
    s = board_extcom_start();
    if (s != HAL_OK) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        return s;
    }
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    /* T3: DISP -> first EXTCOMIN rising edge ~499 ms (minimum 100 us).
     * T4: wait >=2 ms after that edge before permitting SCS HIGH.
     * Poll actual timer phase, not a SysTick/HSI phase estimate. */
    board_wait_extcom_slot();
    return HAL_OK;
}

HAL_StatusTypeDef sharp_write_line(uint16_t line, const uint8_t *data)
{
    if (line < 1 || line > SHARP_HEIGHT || data == NULL) return HAL_ERROR;
    uint8_t packet[44] = {0x01, (uint8_t)line};
    memcpy(packet + 2, data, SHARP_LINE_BYTES);
    /* Last two bytes supply the required 16 transfer clocks. */
    packet[42] = packet[43] = 0;
    begin();
    return end(tx(packet, sizeof packet));
}

static HAL_StatusTypeDef screen(bool white, bool pattern)
{
    uint8_t row[42]; /* address + 40 pixels bytes + 8 dummy clocks */
    const uint8_t update = 0x01;
    const uint8_t final_dummy = 0;
    begin();
    HAL_StatusTypeDef s = tx(&update, 1);
    for (uint16_t y = 1; y <= SHARP_HEIGHT && s == HAL_OK; ++y) {
        row[0] = (uint8_t)y;
        for (unsigned x = 0; x < SHARP_LINE_BYTES; ++x) {
            /* 16x16 checkerboard, first square white. */
            bool w = pattern ? (((x / 2 + (y - 1) / 16) & 1) == 0) : white;
            row[x + 1] = w ? 0xFF : 0x00;
        }
        row[41] = 0;
        s = tx(row, sizeof row);
        /* Next address provides the other 8 transfer clocks for this line. */
    }
    if (s == HAL_OK) s = tx(&final_dummy, 1);
    return end(s);
}
HAL_StatusTypeDef sharp_fill(bool white) { return screen(white, false); }
HAL_StatusTypeDef sharp_test_pattern(void) { return screen(false, true); }
