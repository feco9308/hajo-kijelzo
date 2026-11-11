#include "sharp_ls044q7dh01.h"
/* USB CDC manual-test transport. RX IRQ only frames bounded ASCII commands;
 * LCD operations and replies are handled in main. Fixed buffers, no heap. */
#include "usb_diag.h"
#include "board.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "sharp_ls044q7dh01.h"
#include "usb_manual.h"

USBD_HandleTypeDef hUsbDeviceFS;
volatile uint32_t diag_last_hal_status;
volatile uint32_t diag_usb_status;
volatile uint32_t diag_reset_flags;
extern volatile uint32_t test_stage;
extern volatile uint32_t test_line;
static uint8_t rx[64];
static char report[256]; /* Persists until the USB IN transfer completes. */
static uint8_t coding[7] = {0x00, 0xC2, 0x01, 0, 0, 0, 8}; /* 115200 8N1. */
static volatile uint8_t dtr;
static volatile uint8_t greeting_pending;
static char response[256];
static uint16_t response_length;
static bool response_pending;
/* Single producer (USB RX IRQ), single consumer (main). Three usable slots. */
static char commands[4][USB_COMMAND_SIZE];
static volatile uint8_t command_head, command_tail;
static char input_line[USB_COMMAND_SIZE];
static uint8_t input_length;
static bool input_overflow;
volatile uint32_t diag_rx_dropped;

static int8_t cdc_init(void)
{
    dtr = 0;
    command_head = command_tail = 0;
    input_length = 0;
    input_overflow = false;
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
        uint8_t next_dtr = (uint8_t)(r->wValue & 1U);
        if (next_dtr && !dtr) greeting_pending = 1;
        dtr = next_dtr;
    }
    return USBD_OK;
}
static int8_t cdc_rx(uint8_t *p, uint32_t *n)
{
    for (uint32_t i = 0; i < *n; ++i) {
        uint8_t c = p[i];
        if (c == '\r' || c == '\n') {
            if (input_length || input_overflow) {
                uint8_t next = (uint8_t)((command_head + 1U) % 4U);
                if (next == command_tail) {
                    ++diag_rx_dropped;
                } else {
                    input_line[input_length] = 0;
                    const char *line = input_overflow ? "!overflow" : input_line;
                    memcpy(commands[command_head], line, strlen(line) + 1U);
                    __DMB();
                    command_head = next;
                }
            }
            input_length = 0;
            input_overflow = false;
        } else if ((c == 8U || c == 127U) && !input_overflow) {
            if (input_length) --input_length;
        } else if (c >= 32U && c <= 126U && !input_overflow) {
            if (input_length >= USB_COMMAND_SIZE - 1U) {
                input_overflow = true; /* Discard entire overlong line. */
            } else {
                if (c >= 'A' && c <= 'Z') c = (uint8_t)(c + ('a' - 'A'));
                input_line[input_length++] = (char)c;
            }
        }
    }
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

bool usb_diag_take_command(char command[USB_COMMAND_SIZE])
{
    /* Also serialize against USB reset, which resets queue indexes in IRQ. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    bool found = command_tail != command_head;
    if (found) {
        memcpy(command, commands[command_tail], USB_COMMAND_SIZE);
        command_tail = (uint8_t)((command_tail + 1U) % 4U);
    }
    __set_PRIMASK(mask);
    return found;
}
bool usb_diag_reply_available(void) { return !response_pending; }
bool usb_diag_reply(const char *text)
{
    if (response_pending) return false;
    size_t n = strlen(text);
    if (n >= sizeof response) return false;
    memcpy(response, text, n);
    response_length = (uint16_t)n;
    response_pending = true;
    return true;
}
void usb_diag_reply_status(void)
{
    if (response_pending) return;
    char *p = append(response, "STATUS ms=");
    p = number(p, HAL_GetTick());
    p = append(p, " stage="); p = number(p, test_stage);
    p = append(p, " line="); p = number(p, test_line);
    p = append(p, " auto="); p = number(p, usb_auto_running);
    p = append(p, " disp="); p = number(p, GPIOB->IDR & 1U);
    p = append(p, " extcom="); p = number(p, (GPIOB->IDR >> 1) & 1U);
    p = append(p, " extmode=0 m1="); p = number(p, sharp_com_phase());
    p = append(p, " com_count="); p = number(p, sharp_com_count);
    p = append(p, " cs="); p = number(p, (GPIOA->IDR >> 4) & 1U);
    p = append(p, " hal="); p = number(p, diag_last_hal_status);
    p = append(p, " spi_err="); p = number(p, HAL_SPI_GetError(&hspi1));
    uint32_t divider = 2U << ((hspi1.Init.BaudRatePrescaler & SPI_CR1_BR) >> SPI_CR1_BR_Pos);
    p = append(p, " spi_hz="); p = number(p, HAL_RCC_GetPCLK2Freq() / divider);
    p = append(p, " reset_flags="); p = number(p, diag_reset_flags);
    p = append(p, " rx_dropped="); p = number(p, diag_rx_dropped);
    p = append(p, "\r\n");
    response_length = (uint16_t)(p - response);
    response_pending = true;
}
void usb_diag_poll(void)
{
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED || !dtr) return;
    USBD_CDC_HandleTypeDef *cdc = hUsbDeviceFS.pClassData;
    if (cdc == NULL || cdc->TxState != 0U) return;
    uint16_t length;
    if (response_pending) {
        length = response_length;
        memcpy(report, response, length);
        response_pending = false;
    } else if (greeting_pending) {
        const char *hello = "READY PB0=DISP PB1=EXTCOMIN_LOW EXTMODE=GND. Commands: clear black white pattern invert off on auto stop status help\r\n";
        length = (uint16_t)strlen(hello);
        memcpy(report, hello, length);
        greeting_pending = 0;
    } else {
        return; /* No periodic SPI, image changes or unsolicited status. */
    }
    /* Protect main/IRQ state against bus reset/disconnect during submission. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    cdc = hUsbDeviceFS.pClassData;
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && cdc != NULL && cdc->TxState == 0U) {
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t *)report, length);
        USBD_CDC_TransmitPacket(&hUsbDeviceFS);
    }
    __set_PRIMASK(mask);
}
