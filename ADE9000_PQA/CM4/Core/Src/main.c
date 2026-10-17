/* USER CODE BEGIN Header */
//CM4
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
#include "openamp.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "I2C_LCD.h"
#include "I2C_LCD_cfg.h"
#include "ESP8266_STM32.h"
#include "wifi_secrets.h"   /* WIFI_SSID, WIFI_PASSWORD, MQTT_*  (copy wifi_secrets.example.h) */
#define ADE9000_CM4_VIEW_ONLY
#include "ade9000.h"


#include "stm32h7xx_hal.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "shared_mem.h"

#define ENABLE_LOG 1
#include "redirect.h"
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


#define LCD I2C_LCD_1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
char msg[21] = {0};
typedef enum
{
    PAGE_INST = 0,
    PAGE_POWERCOMP,
    PAGE_POWERQUALITY,
    PAGE_DIAGNOSTICS,
    PAGE_COUNT
} page_t;

volatile page_t current_page = PAGE_INST;
volatile uint8_t menu_active = 0;
volatile uint8_t menu_index = 0;
uint8_t menu_top = 0;

const char *menu_items[PAGE_COUNT] =
{
    "Inst Values",
    "Power Comp",
    "Power Quality",
    "Diagnostics"
};

int16_t last_enc = 0;

phase_shared_t *phase_shared = PHASE_SHARED_ADDR;


enum
{
    REG_AV_PCF = 0,
    REG_AI_PCF,
    REG_AIRMS,
    REG_AVRMS,
    REG_AWATT,
    REG_AVA,
    REG_AVAR,
    REG_AFVAR,
    REG_APF,
    REG_AVTHD,
    REG_AITHD,
    REG_AFWATT,
    REG_AFVA,
    REG_AFIRMS,
    REG_AFVRMS,
    REG_AIRMS1012,
    REG_AVRMS1012
};



typedef struct
{
    float vrms;
    float irms;
    float watt;
    float var;
    float va;
    float pf;
    float vthd;
    float ithd;
    float vf;
    float ifund;
} phase_values_t;

phase_values_t phaseA;

//float mcu_temp = 0.0f;
//float vdd = 0.0f;
//
char esp_ip_cm4[32] = "CONNECTING...";

TIM_HandleTypeDef htim4;                  /* rotary encoder, see UI_Encoder_Init() */
volatile uint8_t encoder_sw_pressed = 0;  /* set in the EXTI callback, handled in the main loop */
uint8_t mqtt_ok = 0;                      /* publish only after the broker connection succeeded */

/* ESP WiFi Config */
ESP8266_Status esp_status;
char mqtt_payload[128];


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static void MX_DMA_Init(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_ADC3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void UI_Encoder_Init(void);
static void UI_BootStatus(const char *text);
//void scanI2C(bool flag);
void updatePhaseA_Inst(bool clear);
void updatePhaseA_PowerComp(bool clear);
void updatePhaseA_PowerQuality(bool clear);
void updatePhaseA_Diagnostics(bool clear);
void drawPage(bool clear);
void drawMenu(void);


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Never draw on the LCD from an interrupt: just flag the press,
 * the main loop debounces it and redraws. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == ENCODER_SW_Pin)
    {
        static uint32_t last_press = 0;

        if(HAL_GetTick() - last_press < 200)
            return;

        last_press = HAL_GetTick();
        encoder_sw_pressed = 1;
    }
}
//
//
//float ade_get_vrms(uint32_t raw)
//{
//    /* Convert RMS code to ADC voltage */
//    float v_adc = ((float)raw * ADE9000_FS_VOLTAGE) /
//                  ADE9000_RMS_FS_CODES;
//
//    /* Apply voltage transformer scaling */
//    float v_line = v_adc * ADE9000_VOLTAGE_TR_FCN;
//
//    return v_line / 1000.0f;   // Volts
//}
//
//double ade_get_irms(uint32_t raw)
//{
//	/* Convert AIRMS code → ADC RMS voltage */
//		double v_adc_rms = ((double)raw / ADE9000_RMS_FS_CODES) * ADC_FS_RMS_VOLTAGE;
//
//		/* Convert voltage → current */
//		double current = v_adc_rms / ROGOWSKI_SENSITIVITY_V_PER_A;
//
//		double final = (double)(current);
//
////		if(final > 1.5)
//			return final;
////		else return 0.0f;
//}
//
//double ade_get_power(int32_t raw)
//{
//    double v_fs =
//        ADC_FS_RMS_VOLTAGE *
//        ADE9000_VOLTAGE_TR_FCN;
//
//    double i_fs =
//        ADC_FS_RMS_VOLTAGE / (ROGOWSKI_SENSITIVITY_V_PER_A);
//
//    double power =
//        ((double)raw / ADE9000_WATT_FS_CODES) *
//        v_fs *
//        i_fs;
//
//    if(power < 0)
//    	return 0;
//    else
//    	return power;
//}
//
//float ade_get_pf(int32_t raw)
//{
//    return (float)raw / ADE_PF_SCALE;
//}
//
float ade_get_vthd(uint32_t raw)
{
    return ((float)raw / ADE_THD_SCALE) * 100.0f;
}

float ade_get_ithd(uint32_t raw)
{
    return ((float)raw / ADE_THD_SCALE) * 100.0f;
}

void update_phase_values()
{
    /* ---------- POWER PARAMS (from CM7) ---------- */
    if(phase_shared->params_ready)
    {
        phase_shared->params_ready = 0;

        power_params_t *p = &phase_shared->params;

        phaseA.vrms = p->vrms;
        phaseA.irms = p->irms;
        phaseA.watt = p->p_real;
        phaseA.va   = p->p_app;
        phaseA.var  = p->p_reactive;
        phaseA.pf   = p->pf;
        phaseA.vf    = p->vpeak;
				phaseA.ifund = p->ipeak;


    }

    /* ---------- THD + FUNDAMENTAL (from ADE) ---------- */
    if(phase_shared->new_data)
    {
        phase_shared->new_data = 0;

        uint32_t *r = phase_shared->phase_regs;

        phaseA.vthd  = ade_get_vthd(r[REG_AVTHD]);
        phaseA.ithd  = ade_get_ithd(r[REG_AITHD]);


    }

    if(phase_shared->ip_ready)
    {
        phase_shared->ip_ready = 0;

        strncpy(esp_ip_cm4, phase_shared->esp_ip, sizeof(esp_ip_cm4));
    }
}
//
//void read_temp_vref(float *temp, float *vdd)
//{
//    uint32_t raw_temp = 0;
//    uint32_t raw_vref = 0;
//
//    HAL_ADC_Start(&hadc3);
//
//    HAL_ADC_PollForConversion(&hadc3, HAL_MAX_DELAY);
//    raw_temp = HAL_ADC_GetValue(&hadc3);
//
//    HAL_ADC_PollForConversion(&hadc3, HAL_MAX_DELAY);
//    raw_vref = HAL_ADC_GetValue(&hadc3);
//
//    HAL_ADC_Stop(&hadc3);
//
//    uint16_t vrefint_cal = *VREFINT_CAL_ADDR;
//    uint16_t ts_cal1 = *TEMPSENSOR_CAL1_ADDR;
//    uint16_t ts_cal2 = *TEMPSENSOR_CAL2_ADDR;
//
//    *vdd = 3.3f * vrefint_cal / raw_vref;
//
//    *temp = ((float)(raw_temp - ts_cal1)) *
//            (110.0f - 30.0f) /
//            (ts_cal2 - ts_cal1) + 30.0f;
//}


void ESP_FormatAll(char *buf, size_t len, power_params_t *p)
{
    snprintf(buf, len,
        "%.1f#%.2f#%.2f#%.0f#%.0f#%.0f#%.2f#%.2f#%.0f#%.2f",
        p->vrms,        // V
        p->irms,        // I
        p->pf,          // PF
        p->p_app,       // S
        p->p_real,      // P
        p->p_reactive,  // Q
        p->vthd,        // VTHD
        p->ithd,        // ITHD
        p->vpeak,       // VP
        p->ipeak        // IP
    );
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
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_USART3_UART_Init();
  MX_ADC3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  UI_Encoder_Init();
  last_enc = (int16_t)__HAL_TIM_GET_COUNTER(&htim4) / 4;
//  HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED);

  /* LCD splash; it stays up while the ESP8266 connects */
  I2C_LCD_Init(LCD);
  I2C_LCD_SetCursor(LCD, 3, 1);
  I2C_LCD_WriteString(LCD, "POWER QUALITY");
  I2C_LCD_SetCursor(LCD, 6, 2);
  I2C_LCD_WriteString(LCD, "MONITOR");

  /* ---------------- ESP8266 INIT + WIFI + MQTT SETUP ---------------- */
  UI_BootStatus("ESP8266 INIT...");
  HAL_GPIO_WritePin(ESP01_RST_GPIO_Port, ESP01_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(1000);
  HAL_GPIO_WritePin(ESP01_RST_GPIO_Port, ESP01_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(1000);

  esp_status = ESP_Init();
  if (esp_status != ESP8266_OK)
  {
      LOG("ESP INIT FAILED\r\n");
      UI_BootStatus("ESP8266 NOT FOUND");
  }
  else
  {
      LOG("ESP INIT SUCCESS\r\n");

      /* WIFI: credentials come from wifi_secrets.h (git-ignored) */
      UI_BootStatus("WIFI CONNECTING...");
      esp_status = ESP_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD,
                                   phase_shared->esp_ip,
                                   sizeof(phase_shared->esp_ip));
      if (esp_status != ESP8266_OK)
      {
          LOG("WIFI FAILED\r\n");
          UI_BootStatus("WIFI FAILED");
      }
      else
      {
          LOG("IP: %s\r\n", phase_shared->esp_ip);
          phase_shared->ip_ready = 1;

          /* MQTT */
          UI_BootStatus("MQTT CONNECTING...");
          esp_status = ESP_MQTT_Connect(MQTT_CLIENT_ID, MQTT_BROKER_IP, MQTT_BROKER_PORT);
          if (esp_status != ESP8266_OK)
          {
              LOG("MQTT FAILED\r\n");
              UI_BootStatus("MQTT FAILED");
          }
          else
          {
              LOG("MQTT CONNECTED\r\n");
              UI_BootStatus("MQTT CONNECTED");
              mqtt_ok = 1;
          }
      }
  }
  HAL_Delay(1500);
  drawPage(true);
  /* ---------------------------------------------------- */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  	/* Pull the latest CM7 parameters and ADE9000 THD registers into phaseA */
  	update_phase_values();

  	/* -------- Encoder push button (flag set in the EXTI callback) -------- */
  	if (encoder_sw_pressed)
  	{
  	    encoder_sw_pressed = 0;
  	    if (menu_active == 0)
  	    {
  	        menu_active = 1;
  	        menu_index = current_page;
  	        drawMenu();
  	    }
  	    else
  	    {
  	        current_page = (page_t)menu_index;
  	        menu_active = 0;
  	        drawPage(true);
  	    }
  	}

  	/* -------- Encoder rotation -------- */
  	int16_t enc = (int16_t)__HAL_TIM_GET_COUNTER(&htim4) / 4;
  	int16_t delta = enc - last_enc;
  	if (delta != 0)
  	{
  	    last_enc = enc;
  	    if (menu_active)
  	    {
  	        int idx = (int)menu_index + (delta > 0 ? 1 : -1);
  	        if (idx >= PAGE_COUNT) idx = 0;            /* wrap both ways */
  	        if (idx < 0) idx = PAGE_COUNT - 1;
  	        menu_index = (uint8_t)idx;

  	        /* 4-row screen: 1 header + 3 items */
  	        if (menu_index < menu_top)
  	            menu_top = menu_index;
  	        if (menu_index >= menu_top + 3)
  	            menu_top = menu_index - 2;

  	        drawMenu();
  	    }
  	}

  	/* -------- Refresh the active page every 500 ms -------- */
  	static uint32_t last_update = 0;
  	if (!menu_active && (HAL_GetTick() - last_update > 500))
  	{
  	    drawPage(false);
  	    last_update = HAL_GetTick();
  	}

  	/* -------- MQTT publish, once a second --------
  	 * The AT commands block while the ESP8266 answers, so publishing every
  	 * waveform frame (~32 ms) would freeze the UI. */
  	static uint32_t last_publish = 0;
  	if (mqtt_ok && (HAL_GetTick() - last_publish >= 1000))
  	{
  	    last_publish = HAL_GetTick();

  	    power_params_t p = {
  	        .vrms = phaseA.vrms,   .irms = phaseA.irms,  .pf = phaseA.pf,
  	        .p_app = phaseA.va,    .p_real = phaseA.watt, .p_reactive = phaseA.var,
  	        .vthd = phaseA.vthd,   .ithd = phaseA.ithd,   /* THD from the ADE9000 registers */
  	        .vpeak = phaseA.vf,    .ipeak = phaseA.ifund,
  	    };
  	    ESP_FormatAll(mqtt_payload, sizeof(mqtt_payload), &p);
  	    if (ESP_MQTT_Publish(MQTT_TOPIC, mqtt_payload, 0, 0) != ESP8266_OK)
  	        LOG("MQTT publish FAIL\r\n");
  	    else
  	        LOG("MQTT publish SUCCESS\r\n");
  	}
  }
  /* USER CODE END 3 */
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 2;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc3.Init.OversamplingMode = DISABLE;
  hadc3.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_387CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10C0ECFF;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 921600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 921600;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_EnableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP01_RST_GPIO_Port, ESP01_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : ESP01_RST_Pin */
  GPIO_InitStruct.Pin = ESP01_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP01_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ENCODER_SW_Pin */
  GPIO_InitStruct.Pin = ENCODER_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(ENCODER_SW_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(ENCODER_SW_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(ENCODER_SW_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */


void updatePhaseA_Inst(bool clear) {

	if(clear) {
		I2C_LCD_Clear(LCD);
	}

	char temp[21] = {0};

	// Header
	I2C_LCD_SetCursor(LCD, 0, 0);
	I2C_LCD_WriteString(LCD, "PHASE-A: INST VALUES");

	// VRMS and IRMS
	snprintf(temp,sizeof(temp),
	         "V:%5.1fV  I:%5.2fA",
	         phaseA.vrms,
	         phaseA.irms);
	I2C_LCD_SetCursor(LCD, 0, 1);
	I2C_LCD_WriteString(LCD, temp);

	// Frequency and PF
	snprintf(temp,sizeof(temp),
	         "PF:%4.2f",
	         phaseA.pf);
	I2C_LCD_SetCursor(LCD, 0, 2);
	I2C_LCD_WriteString(LCD, temp);

	// Active Power (full row)
	snprintf(temp,sizeof(temp),
	         "P:%7.1fW",
	         phaseA.watt);
	I2C_LCD_SetCursor(LCD, 0, 3);
	I2C_LCD_WriteString(LCD, temp);

}

void updatePhaseA_PowerComp(bool clear) {

	if(clear) {
		I2C_LCD_Clear(LCD);
	}

	char temp[21] = {0};

	// Header
	I2C_LCD_SetCursor(LCD, 0, 0);
	I2C_LCD_WriteString(LCD, "PHASE-A: POWER COMP");

	// Line 1 → Active Power + PF
	snprintf(temp,sizeof(temp),
	         "P:%7.1f W",
	         phaseA.watt);
	I2C_LCD_SetCursor(LCD, 0, 1);
	I2C_LCD_WriteString(LCD, temp);

	// Line 2 → Reactive Power
	snprintf(temp,sizeof(temp),
	         "Q:%7.1f VAR",
	         phaseA.var);	I2C_LCD_SetCursor(LCD, 0, 2);
	I2C_LCD_WriteString(LCD, temp);

	// Line 3 → Apparent Power
	snprintf(temp,sizeof(temp),
	         "S:%7.1f VA",
	         phaseA.va);	I2C_LCD_SetCursor(LCD, 0, 3);
	I2C_LCD_WriteString(LCD, temp);
}

void updatePhaseA_PowerQuality(bool clear) {

	if(clear) {
		I2C_LCD_Clear(LCD);
	}

	char temp[21] = {0};

	// Header
	I2C_LCD_SetCursor(LCD, 0, 0);
	I2C_LCD_WriteString(LCD, "PHASE-A: PWR QUALITY");

	// THD values
	snprintf(temp,sizeof(temp),
	         "VTHD:%3.1f%% ITHD:%3.1f%%",
	         phaseA.vthd,
	         phaseA.ithd);	I2C_LCD_SetCursor(LCD, 0, 1);
	I2C_LCD_WriteString(LCD, temp);

	// Fundamental Voltage
	snprintf(temp,sizeof(temp),
	         "VPEAK:%6.1fV",
	         phaseA.vf);	I2C_LCD_SetCursor(LCD, 0, 2);
	I2C_LCD_WriteString(LCD, temp);

	// Fundamental Current
	snprintf(temp,sizeof(temp),
	         "IPEAK:%6.2fA",
	         phaseA.ifund);	I2C_LCD_SetCursor(LCD, 0, 3);
	I2C_LCD_WriteString(LCD, temp);
}

void updatePhaseA_Diagnostics(bool clear)
{
    if(clear) {
        I2C_LCD_Clear(LCD);
    }

    char temp[21] = {0};

    // Header
    I2C_LCD_SetCursor(LCD, 0, 0);
    I2C_LCD_WriteString(LCD, "DIAGNOSTICS");

    /* -------- ESP IP DISPLAY -------- */
    if(strlen(esp_ip_cm4) > 0)
    {
        snprintf(temp, sizeof(temp), "IP:%-15s", esp_ip_cm4);
    }
    else
    {
        snprintf(temp, sizeof(temp), "IP:CONNECTING...");
    }

    I2C_LCD_SetCursor(LCD, 0, 3);
    I2C_LCD_WriteString(LCD, temp);
}
//
//
//
//void LCD_WriteCentered(uint8_t row, const char *text)
//{
//    uint8_t col = (20 - strlen(text)) / 2;
//    I2C_LCD_SetCursor(LCD, col, row);
//    I2C_LCD_WriteString(LCD, text);
//
//
//    /*LCD_WriteCentered(1, "POWER QUALITY");
//    LCD_WriteCentered(2, "MONITOR");*/
//}
//
void drawMenu(void)
{
    // Use a header that doesn't clear the whole screen
    I2C_LCD_SetCursor(LCD, 0, 0);
    I2C_LCD_WriteString(LCD, " ---- MAIN MENU ---- "); // Spaces at end clear old characters

    for (int i = 0; i < 3; i++) // 3 slots available on rows 1, 2, 3
    {
        int item_idx = menu_top + i;
        I2C_LCD_SetCursor(LCD, 0, i + 1);

        if (item_idx < PAGE_COUNT)
        {
            // Print selection arrow
            if (item_idx == menu_index)
                I2C_LCD_WriteString(LCD, "> ");
            else
                I2C_LCD_WriteString(LCD, "  ");

            // Print item name
            I2C_LCD_WriteString(LCD, menu_items[item_idx]);

            // Crucial: Clear the rest of the line so old text doesn't linger
            int len = strlen(menu_items[item_idx]) + 2;
            for(int s = 0; s < (20 - len); s++) I2C_LCD_WriteString(LCD, " ");
        }
        else
        {
            // Clear unused rows
            I2C_LCD_WriteString(LCD, "                    ");
        }
    }
}

void drawPage(bool clear)
{
    switch(current_page)
    {
        case PAGE_INST:
            updatePhaseA_Inst(clear);
            break;

        case PAGE_POWERCOMP:
            updatePhaseA_PowerComp(clear);
            break;

        case PAGE_POWERQUALITY:
            updatePhaseA_PowerQuality(clear);
            break;

        case PAGE_DIAGNOSTICS:
            updatePhaseA_Diagnostics(clear);
            break;
    }
}
//
//void scanI2C(bool flag) {
//	if (flag) {
//	  /*-[ I2C Bus Scanning ]-*/
//	    LOG("Starting I2C Scanning: \r\n");
//	    for(int i=1; i<128; i++)
//	    {
//	    		HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(i<<1), 3, 5);
//	        if (ret != HAL_OK) /* No ACK Received At That Address */
//	        {
//	            LOG(" - ");
//	        }
//	        else if(ret == HAL_OK)
//	        {
//	            LOG("0x%X", i);
//	        }
//	    }
//	    LOG("Done! \r\n\r\n");
//	    /*--[ Scanning Done ]--*/
//	}
//}




/* Rotary encoder on TIM4 (PB6 = CH1, PB7 = CH2). The pins are already set to
 * AF2 in MX_GPIO_Init(); TIM4 was dropped from the .ioc during the MQTT tests,
 * so it is initialised here, inside a USER CODE block that CubeMX keeps. */
static void UI_Encoder_Init(void)
{
  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  __HAL_RCC_TIM4_CLK_ENABLE();

  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 8;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 8;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

/* Shows a status line on row 3 while the ESP8266 is being set up. */
static void UI_BootStatus(const char *text)
{
  char line[21];
  snprintf(line, sizeof(line), "%-20s", text);
  I2C_LCD_SetCursor(LCD, 0, 3);
  I2C_LCD_WriteString(LCD, line);
}

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
