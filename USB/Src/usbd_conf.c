/* STM32G431 USB FS device glue. Based on the ST CubeG4 CDC example's
 * PCD/USBD interface, with static allocation and no low-power mode.
 * PMA layout reserves 0x00..0x3F for all eight endpoint table entries. */
#include "usbd_core.h"
#include "usbd_cdc.h"

PCD_HandleTypeDef hpcd_USB_FS;
static USBD_StatusTypeDef result(HAL_StatusTypeDef s)
{ return s == HAL_OK ? USBD_OK : (s == HAL_BUSY ? USBD_BUSY : USBD_FAIL); }

void HAL_PCD_MspInit(PCD_HandleTypeDef *p)
{
    if (p->Instance != USB) return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* USB_DM/DP are additional functions, not an AF-number selection on
     * G431. Keep PA11/12 in their reset analog/no-pull state. */
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &g);
    __HAL_RCC_USB_CLK_ENABLE();
    HAL_NVIC_SetPriority(USB_LP_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USB_LP_IRQn);
}
void HAL_PCD_MspDeInit(PCD_HandleTypeDef *p)
{
    if (p->Instance != USB) return;
    HAL_NVIC_DisableIRQ(USB_LP_IRQn);
    __HAL_RCC_USB_CLK_DISABLE();
}
void USB_LP_IRQHandler(void) { HAL_PCD_IRQHandler(&hpcd_USB_FS); }
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *p)
{ USBD_LL_SetupStage(p->pData, (uint8_t *)p->Setup); }
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *p, uint8_t ep)
{ USBD_LL_DataOutStage(p->pData, ep, p->OUT_ep[ep].xfer_buff); }
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *p, uint8_t ep)
{ USBD_LL_DataInStage(p->pData, ep, p->IN_ep[ep].xfer_buff); }
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *p) { USBD_LL_SOF(p->pData); }
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *p)
{
    USBD_LL_SetSpeed(p->pData, USBD_SPEED_FULL);
    USBD_LL_Reset(p->pData);
}
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *p) { USBD_LL_Suspend(p->pData); }
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *p) { USBD_LL_Resume(p->pData); }
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *p) { USBD_LL_DevConnected(p->pData); }
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *p) { USBD_LL_DevDisconnected(p->pData); }
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *p, uint8_t ep)
{ USBD_LL_IsoOUTIncomplete(p->pData, ep); }
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *p, uint8_t ep)
{ USBD_LL_IsoINIncomplete(p->pData, ep); }

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *d)
{
    hpcd_USB_FS.Instance = USB;
    hpcd_USB_FS.pData = d;
    d->pData = &hpcd_USB_FS;
    hpcd_USB_FS.Init.dev_endpoints = 8;
    hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
    hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
    hpcd_USB_FS.Init.Sof_enable = DISABLE;
    hpcd_USB_FS.Init.low_power_enable = DISABLE;
    hpcd_USB_FS.Init.lpm_enable = DISABLE;
    hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
    if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK) return USBD_FAIL;
    /* All buffers are disjoint and fit well inside the G431's 1 KiB PMA. */
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x00, PCD_SNG_BUF, 0x40);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, 0x80, PCD_SNG_BUF, 0x80);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_IN_EP, PCD_SNG_BUF, 0xC0);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_OUT_EP, PCD_SNG_BUF, 0x100);
    HAL_PCDEx_PMAConfig(&hpcd_USB_FS, CDC_CMD_EP, PCD_SNG_BUF, 0x140);
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *d) { return result(HAL_PCD_DeInit(d->pData)); }
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *d) { return result(HAL_PCD_Start(d->pData)); }
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *d) { return result(HAL_PCD_Stop(d->pData)); }
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *d, uint8_t ep, uint8_t type, uint16_t n)
{ return result(HAL_PCD_EP_Open(d->pData, ep, n, type)); }
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *d, uint8_t ep)
{ return result(HAL_PCD_EP_Close(d->pData, ep)); }
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *d, uint8_t ep)
{ return result(HAL_PCD_EP_Flush(d->pData, ep)); }
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *d, uint8_t ep)
{ return result(HAL_PCD_EP_SetStall(d->pData, ep)); }
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *d, uint8_t ep)
{ return result(HAL_PCD_EP_ClrStall(d->pData, ep)); }
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *d, uint8_t ep)
{
    PCD_HandleTypeDef *p = d->pData;
    return (ep & 0x80) ? p->IN_ep[ep & 0x7F].is_stall : p->OUT_ep[ep & 0x7F].is_stall;
}
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *d, uint8_t a)
{ return result(HAL_PCD_SetAddress(d->pData, a)); }
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *d, uint8_t ep, uint8_t *p, uint32_t n)
{ return result(HAL_PCD_EP_Transmit(d->pData, ep, p, n)); }
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *d, uint8_t ep, uint8_t *p, uint32_t n)
{ return result(HAL_PCD_EP_Receive(d->pData, ep, p, n)); }
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *d, uint8_t ep)
{ return HAL_PCD_EP_GetRxCount(d->pData, ep); }
void USBD_LL_Delay(uint32_t ms) { HAL_Delay(ms); }
void *USBD_static_malloc(uint32_t n)
{
    static uint32_t mem[(sizeof(USBD_CDC_HandleTypeDef) + 3U) / 4U];
    return n <= sizeof mem ? mem : NULL;
}
void USBD_static_free(void *p) { (void)p; }
