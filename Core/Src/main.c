#include <stdbool.h>
#include "board.h"
#include "sharp_ls044q7dh01.h"
#include "usb_diag.h"
#include "usb_manual.h"
#include "debug_control.h"

SPI_HandleTypeDef hspi1;
TIM_HandleTypeDef htim3;
volatile uint32_t test_stage;
volatile uint32_t test_line;
volatile HAL_StatusTypeDef last_hal_status;

static void check(HAL_StatusTypeDef s)
{
    last_hal_status = s;
    diag_last_hal_status = s;
    if (s != HAL_OK) Error_Handler();
}

void board_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { }
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
    /* Normal full-duplex master, but receive data is unused. PA6/MISO is
     * left at reset analog mode and is not connected to the LCD. HAL's
     * blocking transmit clears the expected RX overrun at transfer end.
     * Unlike 1LINE, this mode does not disable/re-enable SPI per HAL call. */
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT; /* PA4 is GPIO: active HIGH. */
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64; /* 250 kHz diagnostic trial. */
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

/* PB0 AF2 = TIM3_CH3 on STM32G431CBU6. HSI16/APB1=16 MHz. */
HAL_StatusTypeDef board_extcom_start(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 15999; /* 1 kHz counter. */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999; /* 1 Hz. */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_StatusTypeDef s = HAL_TIM_PWM_Init(&htim3);
    if (s != HAL_OK) return s;
    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM2; /* LOW first 500 ms, HIGH next 500 ms. */
    oc.Pulse = 500;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    s = HAL_TIM_PWM_ConfigChannel(&htim3, &oc, TIM_CHANNEL_3);
    if (s != HAL_OK) return s;
    GPIO_InitTypeDef g = {0};
    g.Pin = LCD_EXTCOM_PIN;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF2_TIM3;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_GPIO_Init(GPIOB, &g);
    __HAL_DBGMCU_UNFREEZE_TIM3();
    return HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

void board_wait_extcom_slot(void)
{
    if (!(TIM3->CR1 & TIM_CR1_CEN)) return;
    /* Each transaction now contains at most one row: 44 bytes at 250 kHz
     * takes 1.408 ms. Avoid 3 ms before and 2 ms after the COM rising
     * edge at CNT=500; this leaves SCS LOW at every COM edge. */
    uint32_t count;
    do { count = __HAL_TIM_GET_COUNTER(&htim3); }
    while (count >= 497U && count < 502U);
}

void board_wait_next_extcom_rise(void)
{
    if (!(TIM3->CR1 & TIM_CR1_CEN)) return;
    while (__HAL_TIM_GET_COUNTER(&htim3) >= 500U) { }
    while (__HAL_TIM_GET_COUNTER(&htim3) < 502U) { }
}

int main(void)
{
    HAL_Init();
    gpio_init();
    clock_init();
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    spi_init();
    /* Timer is started by sharp_init after two clears and DISP HIGH. */
    diag_reset_flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();
    diag_usb_status = usb_diag_init();
    test_stage = 1;
    check(sharp_init());         /* All Clear, then DISP HIGH. */

    test_stage = 11; /* Cleared; manual mode, no automatic image writes. */
    test_line = 0;
    for (;;) {
        debug_control_poll();
        usb_diag_poll();
        usb_manual_poll();
        usb_auto_poll();
        HAL_Delay(1);
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
    HAL_GPIO_WritePin(GPIOB, LCD_DISP_PIN, GPIO_PIN_RESET); /* DISP. */
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
    /* Keep the independent hardware COM timer running on errors. */
    for (;;) { usb_diag_poll(); }
}
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
