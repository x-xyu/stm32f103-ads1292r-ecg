/* SPDX-License-Identifier: MIT */
#ifndef ADS1292R_H
#define ADS1292R_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ADS1292R_EXPECTED_ID       0x73U

/* Commands */
#define ADS1292R_CMD_WAKEUP        0x02U
#define ADS1292R_CMD_STANDBY       0x04U
#define ADS1292R_CMD_RESET         0x06U
#define ADS1292R_CMD_START         0x08U
#define ADS1292R_CMD_STOP          0x0AU
#define ADS1292R_CMD_OFFSETCAL     0x1AU
#define ADS1292R_CMD_RDATAC        0x10U
#define ADS1292R_CMD_SDATAC        0x11U
#define ADS1292R_CMD_RDATA         0x12U
#define ADS1292R_CMD_RREG          0x20U
#define ADS1292R_CMD_WREG          0x40U

/* Register addresses */
#define ADS1292R_REG_ID            0x00U
#define ADS1292R_REG_CONFIG1       0x01U
#define ADS1292R_REG_CONFIG2       0x02U
#define ADS1292R_REG_LOFF          0x03U
#define ADS1292R_REG_CH1SET        0x04U
#define ADS1292R_REG_CH2SET        0x05U
#define ADS1292R_REG_RLD_SENS      0x06U
#define ADS1292R_REG_LOFF_SENS     0x07U
#define ADS1292R_REG_LOFF_STAT     0x08U
#define ADS1292R_REG_RESP1         0x09U
#define ADS1292R_REG_RESP2         0x0AU
#define ADS1292R_REG_GPIO          0x0BU

typedef enum
{
    ADS1292R_MODE_INTERNAL_TEST = 0,
    ADS1292R_MODE_ECG = 1
} ADS1292R_Mode;

typedef struct
{
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;
    GPIO_TypeDef *reset_port;
    uint16_t reset_pin;
    GPIO_TypeDef *start_port;
    uint16_t start_pin;
} ADS1292R_Handle;

typedef struct
{
    uint32_t status;
    int32_t channel1;
    int32_t channel2;
} ADS1292R_Frame;

void ADS1292R_Attach(ADS1292R_Handle *device,
                     SPI_HandleTypeDef *spi,
                     GPIO_TypeDef *cs_port, uint16_t cs_pin,
                     GPIO_TypeDef *reset_port, uint16_t reset_pin,
                     GPIO_TypeDef *start_port, uint16_t start_pin);

HAL_StatusTypeDef ADS1292R_HardwareReset(ADS1292R_Handle *device);
HAL_StatusTypeDef ADS1292R_SendCommand(ADS1292R_Handle *device, uint8_t command);
HAL_StatusTypeDef ADS1292R_ReadRegisters(ADS1292R_Handle *device,
                                        uint8_t start_address,
                                        uint8_t *data,
                                        uint8_t length);
HAL_StatusTypeDef ADS1292R_WriteRegisters(ADS1292R_Handle *device,
                                         uint8_t start_address,
                                         const uint8_t *data,
                                         uint8_t length);
HAL_StatusTypeDef ADS1292R_ReadID(ADS1292R_Handle *device, uint8_t *id);
HAL_StatusTypeDef ADS1292R_Begin(ADS1292R_Handle *device,
                                ADS1292R_Mode mode,
                                uint8_t *id);
HAL_StatusTypeDef ADS1292R_Start(ADS1292R_Handle *device);
HAL_StatusTypeDef ADS1292R_Stop(ADS1292R_Handle *device);
HAL_StatusTypeDef ADS1292R_ReadFrame(ADS1292R_Handle *device,
                                    ADS1292R_Frame *frame);

#ifdef __cplusplus
}
#endif

#endif
