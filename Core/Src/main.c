#include <stdbool.h>
#include "board.h"
#include "sharp_ls044q7dh01.h"
#include "usb_diag.h"

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim3;
volatile uint32_t test_stage;

static void check(HAL_StatusTypeDef s)
{
    diag_last_hal_status = s;
    if (s != HAL_OK) Error_Handler();
}

void board_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { }
}

void board_spi_active(uint32_t active)
{
    GPIO_InitTypeDef g = {0};
    if (!active) __HAL_SPI_DISABLE(&hspi1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5 | GPIO_PIN_7, GPIO_PIN_RESET);
    g.Pin = GPIO_PIN_5 | GPIO_PIN_7;
    g.Mode = active ? GPIO_MODE_AF_PP : GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
}

void board_wait_extcom_slot(void)
{
    if ((TIM3->CR1 & TIM_CR1_CEN) == 0U) return; /* Initial All Clear. */
    /* LD-2025315 6-6-5: a rising EXTCOMIN while SCS is HIGH defers COM
     * inversion until SCS falls. Start only 2..300 ms after a rising edge.
     * PWM2 rises at CNT=500; next rise is at CNT=500 in the next period.
     * This leaves >=700 ms before the next rising edge, above a complete
     * ~323 ms pixel transfer at diagnostic 250 kHz plus software overhead.
     * Main-context-only blocking function; TIM3 must be running. */
    uint32_t count;
    do {
        usb_diag_poll();
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
    hspi1.Init.Direction = SPI_DIRECTION_1LINE;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT; /* PA4 is GPIO: active HIGH. */
    /* Controlled signal-integrity trial: HSI16 / 64 = 250 kHz.
     * LD-2025315 has no minimum SCLK frequency; 2 MHz is the maximum. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_LSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
    hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    check(HAL_SPI_Init(&hspi1));
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
    check(HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_3));
    /* Keep hardware inversion running during debugger halts. */
    __HAL_DBGMCU_UNFREEZE_TIM3();
}

HAL_StatusTypeDef board_extcom_start(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0;
    g.Mode = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_GPIO_Init(GPIOB, &g);
    return HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

static void hold_image(uint32_t ms)
{
    /* LD-2025315 6-5-3 Note 6-5-2: no SPI keepalive is needed in external
     * COM mode while holding an image. SCS/SCLK/SI remain physically LOW. */
    uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < ms) {
        usb_diag_poll();
        HAL_Delay(1);
    }
}

int main(void)
{
    diag_reset_flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    HAL_Init();
    gpio_init();
    clock_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    spi_init();
    timer_init();
    /* Absence/failure of USB must never stop external COM or the LCD test. */
    diag_usb_status = usb_diag_init();
    test_stage = 1;
    check(sharp_init());
#if defined(SHARP_SIMPLE_BLINK)
    /* Continuous DISP HIGH; no long blanking/clear phases that could be
     * mistaken for a stopped test. Keep SPI timing and COM unchanged. */
    for (;;) {
        test_stage = 12;
        check(sharp_fill(true));
        hold_image(2000);
        test_stage = 13;
        check(sharp_fill(false));
        hold_image(2000);
    }
#elif defined(SHARP_DIAGNOSTIC_LOOP)
    for (;;) {
        /* Separate display blanking (no SPI) from memory writes. Keep the
         * hardware EXTCOMIN waveform running throughout all five phases. */
        test_stage = 10;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        hold_image(3000);

        test_stage = 11;
        check(sharp_clear());
        HAL_Delay(1); /* >=100 us after clear before enabling DISP. */
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
        hold_image(3000);

        test_stage = 12;
        check(sharp_fill(true));
        hold_image(3000);
        test_stage = 13;
        check(sharp_fill(false));
        hold_image(3000);
        test_stage = 14;
        check(sharp_test_pattern());
        hold_image(3000);
    }
#else
    test_stage = 2;
    check(sharp_fill(true));
    hold_image(3000);
    test_stage = 3;
    check(sharp_fill(false));
    hold_image(3000);
    test_stage = 4;
    check(sharp_test_pattern());
    for (;;) hold_image(1000);
#endif
}

void SysTick_Handler(void) { HAL_IncTick(); }
/* ST Reset_Handler supplies startup; no generic CRT startup or syscalls.
 * __libc_init_array still invokes constructors and this empty _init hook. */
void _init(void) { }
void _fini(void) { }
void Error_Handler(void)
{
    test_stage = 0xFFFFFFFFU;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
    board_spi_active(0);
    /* Do not stop TIM3: inversion remains available after runtime errors. */
    for (;;) { usb_diag_poll(); }
}
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
