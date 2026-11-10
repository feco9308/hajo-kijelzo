/* USB CDC diagnostics. Fixed buffers; no heap, printf, blocking USB writes,
 * or commands that could alter the running LCD test. RX data is discarded. */
#include "usb_diag.h"
#include "board.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"

USBD_HandleTypeDef hUsbDeviceFS;
volatile uint32_t diag_last_hal_status;
volatile uint32_t diag_usb_status;
volatile uint32_t diag_reset_flags;
extern volatile uint32_t test_stage;
static uint8_t rx[64];
static char report[256]; /* Persists until the USB IN transfer completes. */
static uint8_t coding[7] = {0x00, 0xC2, 0x01, 0, 0, 0, 8}; /* 115200 8N1. */
static volatile uint8_t dtr;
static uint32_t last_report;

static int8_t cdc_init(void)
{
    dtr = 0;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)report, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx);
    return USBD_OK;
}
static int8_t cdc_deinit(void) { dtr = 0; return USBD_OK; }
static int8_t cdc_control(uint8_t cmd, uint8_t *p, uint16_t n)
{
    if (cmd == CDC_SET_LINE_CODING && n == sizeof coding) memcpy(coding, p, sizeof coding);
    if (cmd == CDC_GET_LINE_CODING && n == sizeof coding) memcpy(p, coding, sizeof coding);
    if (cmd == CDC_SET_CONTROL_LINE_STATE) {
        /* For no-data requests, ST passes the setup request as pbuf. */
        USBD_SetupReqTypedef *r = (USBD_SetupReqTypedef *)p;
        dtr = (uint8_t)(r->wValue & 1U);
    }
    return USBD_OK;
}
static int8_t cdc_rx(uint8_t *p, uint32_t *n)
{
    (void)p; (void)n;
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}
static int8_t cdc_done(uint8_t *p, uint32_t *n, uint8_t ep)
{ (void)p; (void)n; (void)ep; return USBD_OK; }
static USBD_CDC_ItfTypeDef interface = {cdc_init, cdc_deinit, cdc_control, cdc_rx, cdc_done};

HAL_StatusTypeDef usb_diag_init(void)
{
    RCC_OscInitTypeDef o = {0};
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
    o.HSI48State = RCC_HSI48_ON;
    if (HAL_RCC_OscConfig(&o) != HAL_OK) return HAL_ERROR;
    RCC_PeriphCLKInitTypeDef p = {0};
    p.PeriphClockSelection = RCC_PERIPHCLK_USB;
    p.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&p) != HAL_OK) return HAL_ERROR;
    __HAL_RCC_CRS_CLK_ENABLE();
    RCC_CRSInitTypeDef c = {0};
    c.Prescaler = RCC_CRS_SYNC_DIV1;
    c.Source = RCC_CRS_SYNC_SOURCE_USB;
    c.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    c.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000U, 1000U);
    c.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
    c.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
    HAL_RCCEx_CRSConfig(&c); /* Auto trim 48 MHz from USB SOF. */
    if (USBD_Init(&hUsbDeviceFS, &CDC_Desc, DEVICE_FS) != USBD_OK ||
        USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK ||
        USBD_CDC_RegisterInterface(&hUsbDeviceFS, &interface) != USBD_OK ||
        USBD_Start(&hUsbDeviceFS) != USBD_OK) return HAL_ERROR;
    return HAL_OK;
}

static char *append(char *p, const char *s)
{ while (*s) *p++ = *s++; return p; }
static char *number(char *p, uint32_t v)
{
    char reverse[10];
    unsigned n = 0;
    do { reverse[n++] = (char)('0' + v % 10U); v /= 10U; } while (v);
    while (n) *p++ = reverse[--n];
    return p;
}
void usb_diag_poll(void)
{
    if ((uint32_t)(HAL_GetTick() - last_report) < 1000U) return;
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || !dtr) return;
    USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
    if (cdc == NULL || cdc->TxState != 0U) return;
    last_report = HAL_GetTick();
    char *p = append(report, "LS044_DIAG ms=");
    p = number(p, last_report);
    p = append(p, " stage="); p = number(p, test_stage);
    p = append(p, " disp="); p = number(p, (GPIOB->IDR >> 1) & 1U);
    p = append(p, " extcom="); p = number(p, GPIOB->IDR & 1U);
    p = append(p, " tim3="); p = number(p, TIM3->CNT);
    p = append(p, " cs="); p = number(p, (GPIOA->IDR >> 4) & 1U);
    p = append(p, " hal="); p = number(p, diag_last_hal_status);
    p = append(p, " spi_err="); p = number(p, HAL_SPI_GetError(&hspi1));
    uint32_t divider = 2U << ((hspi1.Init.BaudRatePrescaler & SPI_CR1_BR) >> SPI_CR1_BR_Pos);
    p = append(p, " spi_hz="); p = number(p, HAL_RCC_GetPCLK2Freq() / divider);
    p = append(p, " reset_flags="); p = number(p, diag_reset_flags);
    p = append(p, "\r\n");
    /* Protect main/IRQ state against bus reset/disconnect during submission. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    cdc = hUsbDeviceFS.pClassData;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && cdc != NULL && cdc->TxState == 0U) {
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)report, (uint32_t)(p - report));
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
    }
    __set_PRIMASK(mask);
}
