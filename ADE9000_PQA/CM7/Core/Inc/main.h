/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADE9000_SPI1_SCK_Pin GPIO_PIN_5
#define ADE9000_SPI1_SCK_GPIO_Port GPIOA
#define ADE9000_SPI1_MISO_Pin GPIO_PIN_6
#define ADE9000_SPI1_MISO_GPIO_Port GPIOA
#define CM7_LD1_Pin GPIO_PIN_0
#define CM7_LD1_GPIO_Port GPIOB
#define ADE9000_DREADY_INT_Pin GPIO_PIN_11
#define ADE9000_DREADY_INT_GPIO_Port GPIOE
#define ADE9000_DREADY_INT_EXTI_IRQn EXTI15_10_IRQn
#define ADE9000_SPI1_CS_Pin GPIO_PIN_14
#define ADE9000_SPI1_CS_GPIO_Port GPIOD
#define ADE9000_RESET_Pin GPIO_PIN_15
#define ADE9000_RESET_GPIO_Port GPIOD
#define ADE9000_PM1_Pin GPIO_PIN_9
#define ADE9000_PM1_GPIO_Port GPIOG
#define ADE9000_SPI1_MOSI_Pin GPIO_PIN_5
#define ADE9000_SPI1_MOSI_GPIO_Port GPIOB
#define ADE9000_IRQ0_INT_Pin GPIO_PIN_8
#define ADE9000_IRQ0_INT_GPIO_Port GPIOB
#define ADE9000_IRQ0_INT_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
