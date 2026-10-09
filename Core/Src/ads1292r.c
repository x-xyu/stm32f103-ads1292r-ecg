/* SPDX-License-Identifier: MIT */
#include "ads1292r.h"

#define ADS1292R_SPI_TIMEOUT_MS  20U
#define ADS1292R_FRAME_BYTES      9U

static void ads_select(const ADS1292R_Handle *device)
{
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_RESET);
}

static void ads_wait_before_cs_high(void)
{
    /* ADS1292R requires at least four internal-clock periods between the
       final SCLK edge and CS rising.  A short CPU loop leaves ample margin
       at 72 MHz without spending a full 1 ms of every 4 ms sample period. */
    for (volatile uint32_t i = 0U; i < 600U; ++i)
    {
        __NOP();
    }
}

static void ads_deselect(const ADS1292R_Handle *device)
{
    ads_wait_before_cs_high();
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_SET);
}

static int32_t ads_sign_extend_24(const uint8_t *bytes)
{
    uint32_t value = ((uint32_t)bytes[0] << 16) |
                     ((uint32_t)bytes[1] << 8) |
                     (uint32_t)bytes[2];

    if ((value & 0x00800000UL) != 0U)
    {
        value |= 0xFF000000UL;
    }
    return (int32_t)value;
}

void ADS1292R_Attach(ADS1292R_Handle *device,
                     SPI_HandleTypeDef *spi,
                     GPIO_TypeDef *cs_port, uint16_t cs_pin,
                     GPIO_TypeDef *reset_port, uint16_t reset_pin,
                     GPIO_TypeDef *start_port, uint16_t start_pin)
{
    device->spi = spi;
    device->cs_port = cs_port;
    device->cs_pin = cs_pin;
    device->reset_port = reset_port;
    device->reset_pin = reset_pin;
    device->start_port = start_port;
    device->start_pin = start_pin;
}

HAL_StatusTypeDef ADS1292R_HardwareReset(ADS1292R_Handle *device)
{
    HAL_GPIO_WritePin(device->cs_port, device->cs_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(device->start_port, device->start_pin, GPIO_PIN_RESET);

    /* Allow the module power rails and internal clock to settle. */
    HAL_Delay(100U);
    HAL_GPIO_WritePin(device->reset_port, device->reset_pin, GPIO_PIN_RESET);
    HAL_Delay(2U);
    HAL_GPIO_WritePin(device->reset_port, device->reset_pin, GPIO_PIN_SET);
    HAL_Delay(100U);
    return HAL_OK;
}

HAL_StatusTypeDef ADS1292R_SendCommand(ADS1292R_Handle *device, uint8_t command)
{
    HAL_StatusTypeDef status;
    ads_select(device);
    status = HAL_SPI_Transmit(device->spi, &command, 1U, ADS1292R_SPI_TIMEOUT_MS);
    ads_deselect(device);
    HAL_Delay(1U);
    return status;
}

HAL_StatusTypeDef ADS1292R_ReadRegisters(ADS1292R_Handle *device,
                                        uint8_t start_address,
                                        uint8_t *data,
                                        uint8_t length)
{
    if ((data == NULL) || (length == 0U) ||
        ((uint16_t)start_address + length > 12U))
    {
        return HAL_ERROR;
    }

    uint8_t opcode = (uint8_t)(ADS1292R_CMD_RREG | (start_address & 0x1FU));
    uint8_t count = (uint8_t)(length - 1U);
    uint8_t dummy = 0U;
    HAL_StatusTypeDef status;

    ads_select(device);
    status = HAL_SPI_Transmit(device->spi, &opcode, 1U, ADS1292R_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        HAL_Delay(1U);
        status = HAL_SPI_Transmit(device->spi, &count, 1U, ADS1292R_SPI_TIMEOUT_MS);
    }
    if (status == HAL_OK)
    {
        HAL_Delay(1U);
        for (uint8_t i = 0U; i < length; ++i)
        {
            status = HAL_SPI_TransmitReceive(device->spi, &dummy, &data[i], 1U,
                                             ADS1292R_SPI_TIMEOUT_MS);
            if (status != HAL_OK)
            {
                break;
            }
        }
    }
    ads_deselect(device);
    return status;
}

HAL_StatusTypeDef ADS1292R_WriteRegisters(ADS1292R_Handle *device,
                                         uint8_t start_address,
                                         const uint8_t *data,
                                         uint8_t length)
{
    if ((data == NULL) || (length == 0U) ||
        ((uint16_t)start_address + length > 12U))
    {
        return HAL_ERROR;
    }

    uint8_t opcode = (uint8_t)(ADS1292R_CMD_WREG | (start_address & 0x1FU));
    uint8_t count = (uint8_t)(length - 1U);
    HAL_StatusTypeDef status;

    ads_select(device);
    status = HAL_SPI_Transmit(device->spi, &opcode, 1U, ADS1292R_SPI_TIMEOUT_MS);
    if (status == HAL_OK)
    {
        HAL_Delay(1U);
        status = HAL_SPI_Transmit(device->spi, &count, 1U, ADS1292R_SPI_TIMEOUT_MS);
    }
    if (status == HAL_OK)
    {
        HAL_Delay(1U);
        status = HAL_SPI_Transmit(device->spi, (uint8_t *)data, length,
                                  ADS1292R_SPI_TIMEOUT_MS);
    }
    ads_deselect(device);
    return status;
}

HAL_StatusTypeDef ADS1292R_ReadID(ADS1292R_Handle *device, uint8_t *id)
{
    if (id == NULL)
    {
        return HAL_ERROR;
    }
    return ADS1292R_ReadRegisters(device, ADS1292R_REG_ID, id, 1U);
}

HAL_StatusTypeDef ADS1292R_Begin(ADS1292R_Handle *device,
                                ADS1292R_Mode mode,
                                uint8_t *id)
{
    if ((device == NULL) || (device->spi == NULL) || (id == NULL))
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = ADS1292R_HardwareReset(device);
    if (status != HAL_OK) return status;

    status = ADS1292R_SendCommand(device, ADS1292R_CMD_SDATAC);
    if (status != HAL_OK) return status;
    status = ADS1292R_SendCommand(device, ADS1292R_CMD_RESET);
    if (status != HAL_OK) return status;
    HAL_Delay(100U);
    status = ADS1292R_SendCommand(device, ADS1292R_CMD_SDATAC);
    if (status != HAL_OK) return status;

    status = ADS1292R_ReadID(device, id);
    if (status != HAL_OK) return status;
    if (*id != ADS1292R_EXPECTED_ID) return HAL_ERROR;

    /* CONFIG1: continuous conversion, 250 samples/s. */
    const uint8_t config1 = 0x01U;

    /* CONFIG2: internal 2.42 V reference. Test mode also enables the
       internally generated 1 Hz square wave. */
    const uint8_t config2 =
        (mode == ADS1292R_MODE_INTERNAL_TEST) ? 0xA3U : 0xA0U;

    const uint8_t loff = 0x10U;

    /* Gain = 6. MUX = test signal (0x5) or normal electrode input (0x0). */
    const uint8_t channel_setting =
        (mode == ADS1292R_MODE_INTERNAL_TEST) ? 0x05U : 0x00U;

    /* ECG mode enables the RLD buffer and derives RLD from channel 2. */
    const uint8_t rld_sens =
        (mode == ADS1292R_MODE_INTERNAL_TEST) ? 0x00U : 0x2CU;

    const uint8_t loff_sens = 0x00U;
    const uint8_t resp1 = 0x02U;  /* Respiration disabled; bit 1 must be 1. */
    const uint8_t resp2 = 0x03U;  /* Internal RLD reference; bit 0 must be 1. */
    const uint8_t gpio = 0x0CU;

    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_CONFIG1, &config1, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_CONFIG2, &config2, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_LOFF, &loff, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_CH1SET, &channel_setting, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_CH2SET, &channel_setting, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_RLD_SENS, &rld_sens, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_LOFF_SENS, &loff_sens, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_RESP1, &resp1, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_RESP2, &resp2, 1U);
    if (status != HAL_OK) return status;
    status = ADS1292R_WriteRegisters(device, ADS1292R_REG_GPIO, &gpio, 1U);
    return status;
}

HAL_StatusTypeDef ADS1292R_Start(ADS1292R_Handle *device)
{
    /* Continuous mode avoids sending an opcode before every 72-bit frame.
       The caller reads promptly after DRDY, well before the next conversion. */
    HAL_StatusTypeDef status = ADS1292R_SendCommand(device, ADS1292R_CMD_RDATAC);
    if (status != HAL_OK) return status;
    HAL_GPIO_WritePin(device->start_port, device->start_pin, GPIO_PIN_SET);
    HAL_Delay(10U);
    return HAL_OK;
}

HAL_StatusTypeDef ADS1292R_Stop(ADS1292R_Handle *device)
{
    HAL_GPIO_WritePin(device->start_port, device->start_pin, GPIO_PIN_RESET);
    HAL_Delay(2U);
    return ADS1292R_SendCommand(device, ADS1292R_CMD_SDATAC);
}

HAL_StatusTypeDef ADS1292R_ReadFrame(ADS1292R_Handle *device,
                                    ADS1292R_Frame *frame)
{
    if ((device == NULL) || (frame == NULL))
    {
        return HAL_ERROR;
    }

    uint8_t tx[ADS1292R_FRAME_BYTES] = {0U};
    uint8_t rx[ADS1292R_FRAME_BYTES] = {0U};

    ads_select(device);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
        device->spi, tx, rx, ADS1292R_FRAME_BYTES,
        ADS1292R_SPI_TIMEOUT_MS);
    ads_deselect(device);

    if (status != HAL_OK)
    {
        return status;
    }

    frame->status = ((uint32_t)rx[0] << 16) |
                    ((uint32_t)rx[1] << 8) |
                    (uint32_t)rx[2];
    frame->channel1 = ads_sign_extend_24(&rx[3]);
    frame->channel2 = ads_sign_extend_24(&rx[6]);
    return HAL_OK;
}
