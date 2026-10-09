/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ads1292r.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t ads1292r_id = 0U;
ADS1292R_Handle ads;
ADS1292R_Frame ads_frame;
uint32_t sample_index = 0U;
uint32_t frame_error_count = 0U;
uint32_t reported_error_count = 0U;
uint32_t last_bad_status = 0U;
uint32_t last_diag_tick = 0U;
uint8_t ads_ready = 0U;
uint8_t ads_drdy_armed = 1U;
char uart_line[64];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  /* A reset of the STM32 does not power-cycle the ADS1292R.  Force the
     ADS1292R PWDN/RESET pin through a complete reset sequence so that a
     push-button reset starts from the same state as a power-on reset. */
  HAL_Delay(500);                                      /* wait for battery/USB rails */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET); /* START low */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);    /* CS inactive */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);  /* PWDN/RESET low */
  HAL_Delay(20);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);    /* wake/reset ADS */
  HAL_Delay(500);
  uint8_t regs[5] = {0U};
  int n = 0;

  ads_ready = 0U;

  ADS1292R_Attach(
      &ads,
      &hspi1,
      ADS_CS_GPIO_Port, ADS_CS_Pin,
      ADS_RESET_GPIO_Port, ADS_RESET_Pin,
      ADS_START_GPIO_Port, ADS_START_Pin
  );

  if (ADS1292R_Begin(
          &ads,
          ADS1292R_MODE_ECG,
          &ads1292r_id
      ) == HAL_OK)
  {
    n = snprintf(
        uart_line,
        sizeof(uart_line),
        "ADS1292R ID=0x%02X OK\r\n",
        ads1292r_id
    );
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_line, n, 100U);

    if (ADS1292R_ReadRegisters(
            &ads,
            ADS1292R_REG_CONFIG1,
            regs,
            5U
        ) == HAL_OK)
    {
      n = snprintf(
          uart_line,
          sizeof(uart_line),
          "REG=%02X,%02X,%02X,%02X,%02X\r\n",
          regs[0], regs[1], regs[2], regs[3], regs[4]
      );
      HAL_UART_Transmit(&huart1, (uint8_t *)uart_line, n, 100U);

      if ((regs[0] == 0x01U) &&
          (regs[1] == 0xA0U) &&
          (regs[3] == 0x00U) &&
          (regs[4] == 0x00U) &&
          (ADS1292R_Start(&ads) == HAL_OK))
      {
        ads_ready = 1U;
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"ADS START OK\r\n",
            14U,
            100U
        );
      }
      else
      {
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"ADS CONFIG/START FAILED\r\n",
            25U,
            100U
        );
      }
    }
    else
    {
      HAL_UART_Transmit(
          &huart1,
          (uint8_t *)"ADS REG READ FAILED\r\n",
          21U,
          100U
      );
    }
  }
  else
  {
    n = snprintf(
        uart_line,
        sizeof(uart_line),
        "ADS1292R init failed, ID=0x%02X\r\n",
        ads1292r_id
    );
    HAL_UART_Transmit(&huart1, (uint8_t *)uart_line, n, 100U);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (ads_ready == 0U)
    {
      HAL_Delay(100U);
    }
    else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_SET)
    {
      /* A new falling edge may be accepted only after DRDY returned high. */
      ads_drdy_armed = 1U;
    }
    else if (ads_drdy_armed != 0U)
    {
      ads_drdy_armed = 0U;

      if (ADS1292R_ReadFrame(&ads, &ads_frame) == HAL_OK)
      {
        if ((ads_frame.status & 0xF00000UL) == 0xC00000UL)
        {
          uint32_t current_sample = sample_index++;

          /* Read every 250-Hz sample, transmit one channel at 125 lines/s. */
          if ((current_sample % 2U) == 0U)
          {
            n = snprintf(
                uart_line,
                sizeof(uart_line),
                "%lu,%ld\r\n",
                (unsigned long)current_sample,
                (long)ads_frame.channel2
            );
            HAL_UART_Transmit(
                &huart1,
                (uint8_t *)uart_line,
                n,
                20U
            );
          }

          if ((current_sample % 125U) == 0U)
          {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
          }
        }
        else
        {
          ++frame_error_count;
          last_bad_status = ads_frame.status;
        }
      }
      else
      {
        ads_ready = 0U;
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"ADS SPI READ ERROR\r\n",
            20U,
            100U
        );
      }
    }

    /* Never print a message for every bad frame: that much blocking UART
       traffic can consume a complete 4-ms sample interval.  Report only
       once per second and only when the cumulative count has changed. */
    if ((HAL_GetTick() - last_diag_tick) >= 1000U)
    {
      last_diag_tick = HAL_GetTick();
      if (frame_error_count != reported_error_count)
      {
        reported_error_count = frame_error_count;
        n = snprintf(
            uart_line,
            sizeof(uart_line),
            "ADS ERRORS=%lu LAST=%06lX\r\n",
            (unsigned long)frame_error_count,
            (unsigned long)(last_bad_status & 0xFFFFFFUL)
        );
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)uart_line,
            n,
            20U
        );
      }
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */

}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
