#include <stdbool.h>
#include "board.h"
#include "sharp_ls044q7dh01.h"

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim3;
volatile uint32_t test_stage;
volatile uint32_t test_line;
volatile HAL_StatusTypeDef last_hal_status;

static void check(HAL_StatusTypeDef s)
{
    last_hal_status = s;
    if (s != HAL_OK) Error_Handler();
}

void board_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { }
}

void board_wait_extcom_slot(void)
{
    if ((TIM3->CR1 & TIM_CR1_CEN) == 0U) return;
    /* LD-2025315 6-6-5: a rising EXTCOMIN while SCS is HIGH defers COM
     * inversion until SCS falls. Start only 2..300 ms after a rising edge.
     * PWM2 rises at CNT=500; next rise is at CNT=500 in the next period.
     * This leaves >=700 ms before the next rising edge, above a complete
     * ~81 ms pixel transfer at 1 MHz plus software overhead.
     * Main-context-only blocking function; TIM3 must be running. */
    uint32_t count;
    do {
        count = __HAL_TIM_GET_COUNTER(&htim3);
    } while (count < 502U || count > 800U);
}

static void gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* Set output latches BEFORE changing GPIO modes. Reset-state pins are
     * high impedance: external pulldowns are needed for safety during reset. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOB, &g);
}

void board_wait_extcom_line_slot(void)
{
    /* A 44-byte single-line packet is 352 us at 1 MHz. Avoid only the
     * 5 ms region around EXTCOMIN's CNT=500 rising edge, rather than the
     * broad full-frame window. This keeps the line sweep nearly uniform. */
    if ((TIM3->CR1 & TIM_CR1_CEN) == 0U) return;
    uint32_t count;
    do {
        count = __HAL_TIM_GET_COUNTER(&htim3);
    } while (count >= 497U && count < 502U);
}

void board_wait_next_extcom_rise(void)
{
    /* Startup only: wait for a rising edge AFTER DISP was enabled, then
     * 2 ms of TCOM initialization. PWM2 rises at CNT=500 every period. */
    while (__HAL_TIM_GET_COUNTER(&htim3) >= 500U) { }
    while (__HAL_TIM_GET_COUNTER(&htim3) < 502U) { }
}

static void clock_init(void)
{
    /* No board-dependent crystal. HSI16 -> SYSCLK, AHB/APB1/APB2 all 16 MHz. */
    RCC_OscInitTypeDef o = {0};
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.PLL.PLLState = RCC_PLL_NONE;
    check(HAL_RCC_OscConfig(&o));
    RCC_ClkInitTypeDef c = {0};
    c.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    c.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
    check(HAL_RCC_ClockConfig(&c, FLASH_LATENCY_0));
}

static void spi_init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    /* Normal full-duplex master, but receive data is unused. PA6/MISO is
     * left at reset analog mode and is not connected to the LCD. HAL's
     * blocking transmit clears the expected RX overrun at transfer end.
     * Unlike 1LINE, this mode does not disable/re-enable SPI per HAL call. */
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT; /* PA4 is GPIO: active HIGH. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16; /* 1 MHz. */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_LSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    check(HAL_SPI_Init(&hspi1));
    /* Enable the peripheral before routing its idle outputs to the pins.
     * PA5/PA7 stay AF5 for normal operation; only SCS is toggled per packet.
     * CPOL=0 holds SCLK LOW. Every packet ends with zero dummy data, so
     * MOSI's final transmitted bit is LOW and remains LOW between packets. */
    __HAL_SPI_ENABLE(&hspi1);
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
}

static void timer_init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 15999; /* 16 MHz / 16000 = 1 kHz. */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999; /* 1 Hz, 500 ms LOW + 500 ms HIGH. */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    check(HAL_TIM_PWM_Init(&htim3));
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM2; /* Starts LOW at CNT=0. */
    oc.Pulse = 500;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    check(HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_4));
    /* Keep hardware inversion running during debugger halts. */
    __HAL_DBGMCU_UNFREEZE_TIM3();
}

HAL_StatusTypeDef board_extcom_start(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_1; /* Confirmed wiring: PB1 -> EXTCOMIN, TIM3_CH4. */
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_GPIO_Init(GPIOB, &g);
    return HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

/* This is the entire application: no USB, test-mode selection or UI. */
int main(void)
{
    HAL_Init();
    gpio_init();
    clock_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    spi_init();
    timer_init();
    check(board_extcom_start()); /* 1 Hz starts BEFORE All Clear. */
    test_stage = 1;
    check(sharp_init());         /* All Clear, then DISP HIGH. */

    uint8_t row[SHARP_LINE_BYTES]; /* Only one row, not a framebuffer. */
    for (;;) {
        test_stage = 20;
        test_line = 0;
        check(sharp_clear());
        HAL_Delay(1000);

        for (unsigned i = 0; i < sizeof row; ++i) row[i] = 0x00;
        test_stage = 21;
        for (uint16_t line = 1; line <= SHARP_HEIGHT; ++line) {
            test_line = line;
            check(sharp_write_line(line, row));
            HAL_Delay(40); /* About 10 seconds for the entire black sweep. */
        }
        test_stage = 22;
        HAL_Delay(2000);

        for (unsigned i = 0; i < sizeof row; ++i) row[i] = 0xFF;
        test_stage = 23;
        for (uint16_t line = 1; line <= SHARP_HEIGHT; ++line) {
            test_line = line;
            check(sharp_write_line(line, row));
            HAL_Delay(40);
        }
        test_stage = 24;
        HAL_Delay(2000);
    }
}

void SysTick_Handler(void) { HAL_IncTick(); }
/* ST Reset_Handler supplies startup; no generic CRT startup or syscalls.
 * __libc_init_array still invokes constructors and this empty _init hook. */
void _init(void) { }
void _fini(void) { }
void Error_Handler(void)
{
    test_stage = 0xFFFFFFFFU;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); /* DISP. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    /* Fatal-error exception only: stop SPI and force physical idle LOW.
     * There is no GPIO/AF switching on the normal transaction path. */
    __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_7, GPIO_PIN_RESET);
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);
    /* Do not stop TIM3: inversion remains available after runtime errors. */
    for (;;) {
    }
}
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
