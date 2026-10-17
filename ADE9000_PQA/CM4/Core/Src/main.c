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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../../CM7/Drivers/ade9000_drivers/ade9000.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
/* To comment when developping/debugging on a single core                     */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
struct ade9000_dev *ade_dev = NULL;

uint8_t tx_data[8] = {0xAA, 0x55, 0x12, 0x34, 0xAB, 0xCD, 0x00, 0xFF};
uint8_t rx_data[8];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_DMA_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void SPI_Loopback_Test()
{
    HAL_StatusTypeDef status;

    status = HAL_SPI_TransmitReceive(&hspi1,
                                     tx_data,
                                     rx_data,
                                     sizeof(tx_data),
                                     HAL_MAX_DELAY);

    if(status != HAL_OK)
    {
        printf("SPI Error!\r\n");
        return;
    }

    printf("TX   RX\r\n");
    for(int i = 0; i < sizeof(tx_data); i++)
    {
        printf("0x%02X 0x%02X\r\n", tx_data[i], rx_data[i]);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /*HW semaphore Clock enable*/
  __HAL_RCC_HSEM_CLK_ENABLE();
  /* Activate HSEM notification for Cortex-M4*/
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  /*
  Domain D2 goes to STOP mode (Cortex-M4 in deep-sleep) waiting for Cortex-M7 to
  perform system initialization (system clock config, external memory configuration.. )
  */
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  /* Clear HSEM flag */
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));

#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	HAL_Delay(1000);
	// Power-up sequence
	printf("PERFORMING POWERUP SEQUENCE\r\n");
	HAL_GPIO_WritePin(ADE9000_PM1_GPIO_Port, ADE9000_PM1_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(ADE9000_RESET_GPIO_Port, ADE9000_RESET_Pin, GPIO_PIN_RESET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(ADE9000_RESET_GPIO_Port, ADE9000_RESET_Pin, GPIO_PIN_SET);
	HAL_Delay(1000);

	// ADE9000 Init parameters
	struct ade9000_init_param ade_init = {
		.hspi = &hspi1,
		.cs_port = ADE9000_SPI1_CS_GPIO_Port,
		.cs_pin = ADE9000_SPI1_CS_Pin,
		.temp_en = true
	};

	int ret = ade9000_init(&ade_dev, ade_init);
	if (ret == 0)
	{
	  printf("ADE9000 INIT SUCCESS\r\n");
	}
	else
	{
	  printf("ADE9000 INIT FAILED: %d\r\n", ret);
	}


  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

//	  printf("CM4 running\r\n");
//	  SPI_Loopback_Test();

	  uint32_t version = 0;

	  if (ade_dev)
	  {
	      if (ade9000_read(ade_dev, ADE9000_REG_CONFIG5, &version) == 0)
	      {
	          printf("CONFIG5 = 0x%02lX\r\n", version);

	          if (version == ADE9000_CHIP_ID)
	              printf("ADE9000 COMMUNICATION OK!\r\n");
	          else
	              printf("Wrong Chip ID!\r\n");
	      }
	      else
	      {
	          printf("SPI Read Failed!\r\n");
	      }
	  }

	  HAL_Delay(2000);


	  HAL_Delay(1000);
  }
  /* USER CODE END 3 */
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

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
