/* USER CODE BEGIN Header */
//CM7
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
#include "ade9000.h"
#include <math.h>


#define ALPHA 0.2   // smoothing factor
//#include "lwip/apps/httpd.h"

#include "shared_mem.h"

#define ENABLE_LOG 1
#include "redirect.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


#define ADE9000_PCF_FS_CODES 74532013.0f

#define ADE9000_FS_VOLTAGE 1.0f
#define VOLTAGE_TR 1001.0f

#define V_FS (ADE9000_FS_VOLTAGE * VOLTAGE_TR)

#define ADC_FS_RMS_VOLTAGE 0.1765f
#define ROGOWSKI_SENSITIVITY 194e-6f
#define CURRENT_GAIN 1.42f

#define I_FS (ADC_FS_RMS_VOLTAGE / ROGOWSKI_SENSITIVITY)
#define V_CAL  (239.0 / 158.0)   // ≈ 1.51
#define I_CAL  (5.1   / 3.4)     // ≈ 1.50


/* DUAL_CORE_BOOT_SYNC_SEQUENCE: Define for dual core boot synchronization    */
/*                             demonstration code based on hardware semaphore */
/* This define is present in both CM7/CM4 projects                            */
///* To comment when developping/debugging on a single core                     */
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

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;

UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
struct ade9000_dev *ade_dev = NULL;
long int i = 0;
volatile uint8_t ade_data_ready = 0;
volatile uint8_t ade_wfb_ready = 0;
volatile uint8_t waveform_ready = 0;
volatile uint16_t sample_counter = 0;
volatile uint8_t spi_busy = 0; // for waveform DMA
volatile uint8_t uart_tx_busy = 0; // for waveform DMA
volatile uint8_t phase_ready = 0; // for phase regs DMS

uint32_t waveform[WAVEFORM_BUFFER_SIZE];
uint8_t waveform_spi_rx[SPI_RX_BUFFER_SIZE];
uint8_t phase_spi_rx[ADE9000_BURST_PHASE_A_BYTES];
//uint32_t phase_regs[ADE9000_BURST_PHASE_A_REG_COUNT];

#pragma pack(push,1)
typedef struct
{
    uint16_t header;
    uint32_t frame_id;
    uint16_t count;
    int32_t ia[WAVE_SAMPLES];
    int32_t va[WAVE_SAMPLES];
    uint16_t crc;
} wave_packet_t;
#pragma pack(pop)

wave_packet_t pkt;

// spi dma mode selection
typedef enum
{
    SPI_IDLE = 0,
    SPI_WAVEFORM,
    SPI_PHASE
} spi_mode_t;

volatile spi_mode_t spi_mode = SPI_IDLE;



phase_shared_t *phase_shared = PHASE_SHARED_ADDR;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
uint16_t crc16_ccitt(uint8_t *data, uint32_t len);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
//{
//    LOG("ETH RX IRQ\r\n");
//}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == ADE9000_DREADY_INT_Pin)
	{
		sample_counter++;
		if(sample_counter >= 160) {
			sample_counter = 0;
			ade_data_ready = 1;
		}
	}	else if (GPIO_Pin == ADE9000_IRQ0_Pin) {
		ade_wfb_ready = 1;
	}
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART3)
    {
        uart_tx_busy = 0;
        LOG("\r\n\r\nUART3 DMA CallBack Received\r\n\r\n");
    }
}


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        HAL_GPIO_WritePin(ADE9000_SPI1_CS_GPIO_Port,
                          ADE9000_SPI1_CS_Pin,
                          GPIO_PIN_SET);

        if(spi_mode == SPI_WAVEFORM)
        {
//            ade9000_parse_burst_block(waveform_spi_rx, WAVEFORM_BUFFER_SIZE, waveform);
//
////            ade9000_write(ade_dev, ADE9000_REG_STATUS0, ADE9000_STATUS0_PAGE_FULL);
//            uint32_t status0;
//            ade9000_read(ade_dev, ADE9000_REG_STATUS0, &status0);
//
//            // Clear ALL active flags
//            ade9000_write(ade_dev, ADE9000_REG_STATUS0, status0);

            waveform_ready = 1;
        }
        else if(spi_mode == SPI_PHASE)
        {
//            ade9000_parse_burst_block(phase_spi_rx, ADE9000_BURST_PHASE_A_REG_COUNT, phase_shared->phase_regs);
//
//            phase_shared->new_data = 1;
						phase_ready = 1;
        }

        spi_busy = 0;
        spi_mode = SPI_IDLE;
    }
}

void ade_write_debug(struct ade9000_dev *dev, char *reg_name, uint16_t reg_addr, uint32_t reg_data) {
	uint32_t temp;
	ade9000_read(ade_dev, reg_addr, &temp);
	LOG("%s (BEFORE) = 0x%08lX\r\n",reg_name, temp);

	int ret = ade9000_write(dev, reg_addr, reg_data);
	if (ret) {
		LOG("%s write failed\r\n", reg_name);
		return;
	}

	ade9000_read(ade_dev, reg_addr, &temp);
	LOG("%s (AFTER) = 0x%08lX\r\n", reg_name, temp);
}




void compute_parameters(int32_t *va_raw,
                        int32_t *ia_raw,
                        uint16_t N,
                        power_params_t *out)
{
    static double v[WAVE_SAMPLES];
    static double i[WAVE_SAMPLES];

    static double vrms_f = 0;
    static double irms_f = 0;
    static double p_real_f = 0;
    static double pf_f = 0;

    double mean_v = 0, mean_i = 0;

    /* -------- SCALE -------- */
    for(int k = 0; k < N; k++)
    {
				v[k] = ((double)va_raw[k] / ADE9000_PCF_FS_CODES) * V_FS * V_CAL;
				i[k] = ((double)ia_raw[k] / ADE9000_PCF_FS_CODES) * I_FS * CURRENT_GAIN * I_CAL;

        mean_v += v[k];
        mean_i += i[k];
    }

    mean_v /= N;
    mean_i /= N;

    /* -------- REMOVE DC -------- */
    for(int k = 0; k < N; k++)
    {
        v[k] -= mean_v;
        i[k] -= mean_i;
    }

    /* -------- HANN WINDOW (stability boost) -------- */
    for(int k = 0; k < N; k++)
    {
        double w = 0.5 * (1.0 - cos(2.0 * M_PI * k / (N - 1)));
        v[k] *= w;
        i[k] *= w;
    }

    /* -------- RMS + POWER -------- */
    double sum_v2 = 0, sum_i2 = 0, sum_p = 0;
    double vpeak = 0, ipeak = 0;

    for(int k = 0; k < N; k++)
    {
        double vk = v[k];
        double ik = i[k];

        sum_v2 += vk * vk;
        sum_i2 += ik * ik;

        double p = vk * ik;
        sum_p += p;

        double av = fabs(vk);
        double ai = fabs(ik);

        if(av > vpeak) vpeak = av;
        if(ai > ipeak) ipeak = ai;
    }

    double vrms = sqrt(sum_v2 / N);
    double irms = sqrt(sum_i2 / N);
    double p_real = sum_p / N;
    double p_app = vrms * irms;

    double pf = (p_app > 1e-9) ? (p_real / p_app) : 0;
    double p_reactive = sqrt(fmax(p_app*p_app - p_real*p_real, 0));

    /* -------- EXPONENTIAL SMOOTHING -------- */
    vrms_f = ALPHA * vrms + (1.0 - ALPHA) * vrms_f;
    irms_f = ALPHA * irms + (1.0 - ALPHA) * irms_f;
    p_real_f = ALPHA * p_real + (1.0 - ALPHA) * p_real_f;
    pf_f = ALPHA * pf + (1.0 - ALPHA) * pf_f;

    double p_app_f = vrms_f * irms_f;
    double p_reactive_f = sqrt(fmax(p_app_f*p_app_f - p_real_f*p_real_f, 0));

    /* -------- OUTPUT -------- */
    out->vrms = vrms_f;
    out->irms = irms_f;
    out->pf = pf_f;
    out->p_app = p_app_f;
    out->p_real = p_real_f;
    out->p_reactive = p_reactive_f;
    out->vthd = 0;   // still TODO
    out->ithd = 0;
    out->vpeak = vpeak;
    out->ipeak = ipeak;
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
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  int32_t timeout;
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
#endif /* DUAL_CORE_BOOT_SYNC_SEQUENCE */
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */
/* Debug halt */
//__BKPT(0);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  phase_shared->new_data = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  HAL_Delay(2000);
	// Power-up sequence
	LOG("\r\nPERFORMING POWERUP SEQUENCE\r\n");

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
		LOG("ADE9000 INIT SUCCESS\r\n");
	}
	else
	{
		LOG("ADE9000 INIT FAILED: %d\r\n", ret);
	}

	ret = ade9000_setup(ade_dev);
	if(ret == 0)
		LOG("ADE9000 SETUP OK\r\n");
	else
		LOG("SETUP FAILED\r\n");

	HAL_Delay(2000);
//	if(ade9000_read_temp(ade_dev) == 0)
//	{
//	    LOG("Temp = %ld C\r\n", ade_dev->temp_deg);
//	}

/* ------------------------- WAVEFORM BUFFER CONFIG ------------------------- */

	// enable BURST_EN in CONFIG1.
	uint32_t reg = 0;
	ade9000_read(ade_dev, ADE9000_REG_CONFIG1, &reg);

	reg |= ADE9000_BURST_EN;
	ade9000_write(ade_dev, ADE9000_REG_CONFIG1, reg);


	reg  = 0;
	ade_write_debug(ade_dev, "WFB_TRG_CFG", ADE9000_REG_WFB_TRG_CFG, reg);

	/*using Continuous Fill Mode*/
	reg |= ADE9000_WF_CAP_SEL;
	reg |= no_os_field_prep(ADE9000_WF_MODE, ADE9000_MODE_SAVE_EVENT_ADDR);
	ade_write_debug(ade_dev, "WFB_CONFIG", ADE9000_REG_WFB_CFG, reg);

	// Enable PAGE FULL Interrupt
	reg = 0;
	reg |= ADE9000_MASK0_PAGE_FULL;
	ade_write_debug(ade_dev, "MASK0", ADE9000_REG_MASK0, reg);

	// Page 15 full → interrupt
	ade_write_debug(ade_dev, "WFB_PG_IRQEN", ADE9000_REG_WFB_PG_IRQEN, 0x8000);

	reg = 0;
	reg |= ADE9000_WF_CAP_SEL;
	reg |= no_os_field_prep(ADE9000_WF_MODE, ADE9000_MODE_TRIG_EN_EVENTS);
	reg |= no_os_field_prep(ADE9000_WF_SRC, ADE9000_SRC_DSP);
	reg |= no_os_field_prep(ADE9000_BURST_CHAN, ADE9000_BURST_IA_VA);
	reg |= ADE9000_WF_CAP_EN;
	ade_write_debug(ade_dev, "WFB_CONFIG", ADE9000_REG_WFB_CFG, reg);

//	 calibration config
	uint32_t gain;
	ade9000_read(ade_dev, ADE9000_REG_AIGAIN, &gain);
	LOG("AIGAIN = 0x%08lX\r\n", gain);

	uint32_t pga;
	ade9000_read(ade_dev, ADE9000_REG_PGA_GAIN, &pga);
	LOG("PGA_GAIN register = 0x%08lX\r\n", pga);
	HAL_Delay(1000);



  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  	// blink led periodically periodically
  	static uint32_t last_print = 0;
  	if (HAL_GetTick() - last_print > 2000) // every 2 sec
  	{
  	    last_print = HAL_GetTick();

  	    HAL_GPIO_TogglePin(CM7_LD1_GPIO_Port, CM7_LD1_Pin);
  	}




  	/* ---------- Waveform DMA trigger ---------- */

		if(ade_wfb_ready)
		{
				ade_wfb_ready = 0;


				if(!spi_busy)
				{
						spi_busy = 1;
						spi_mode = SPI_WAVEFORM;

						ade9000_wfb_burst_read_DMA(ade_dev,
																			 WAVEFORM_BUFFER_SIZE,
																			 waveform_spi_rx);
				}
		}

		/* ---------- Send waveform ---------- */

		if(waveform_ready)
		{
				waveform_ready = 0;

				// Parse data OUTSIDE ISR
				ade9000_parse_burst_block(waveform_spi_rx,
																	WAVEFORM_BUFFER_SIZE,
																	waveform);

				// Clear ADE interrupt OUTSIDE ISR
				uint32_t status0;
				ade9000_read(ade_dev, ADE9000_REG_STATUS0, &status0);
				ade9000_write(ade_dev, ADE9000_REG_STATUS0, status0);

				pkt.header = 0xAA55;
				pkt.frame_id = i++;
				pkt.count  = WAVE_SAMPLES;

				for(int j = 0; j < WAVE_SAMPLES; j++)
				{
						pkt.ia[j] = (int32_t)waveform[j*2];
						pkt.va[j] = (int32_t)waveform[j*2+1];
				}

				power_params_t params;

				compute_parameters(pkt.va, pkt.ia, WAVE_SAMPLES, &params);
				phase_shared->params_ready = 0;   // clear first
				phase_shared->params = params;    // copy struct
				phase_shared->params_ready = 1;   // signal done



//				LOG("VRMS=%.2f IRMS=%.2f PF=%.3f P=%.2f VA=%.2f Q=%.2f VTHD=%.2f%% ITHD=%.2f%%\r\n",
//				        params.vrms,
//				        params.irms,
//				        params.pf,
//				        params.p_real,
//				        params.p_app,
//				        params.p_reactive,
//				        params.vthd * 100.0f,
//				        params.ithd * 100.0f
//				    );

				pkt.crc = crc16_ccitt((uint8_t*)&pkt, sizeof(pkt) - 2);

				if(!uart_tx_busy)
				{
						uart_tx_busy = 1;
						HAL_UART_Transmit_DMA(&huart3,
																	(uint8_t*)&pkt,
																	sizeof(pkt));
				}
		}

		/* ---------- Phase register read ---------- */

		if(ade_dev && ade_data_ready && !spi_busy)
		{
				ade_data_ready = 0;

				spi_busy = 1;
				spi_mode = SPI_PHASE;

				ade9000_phase_block_read_DMA(ade_dev, phase_spi_rx);
		}
//  	if(ade_data_ready) {
//  		ade_data_ready = 0;
//  		ade9000_read_data_ph(ade_dev, ADE9000_PHASE_A);
//
//  						LOG("%d\t%lu\t%lu\t%lu\t%lu\r\n",
//  								i++,
//  								ade_dev->vrms_raw_adc, ade_dev->vrms_val,
//  								ade_dev->irms_raw_adc, ade_dev->irms_val);
//
//
//  	}


		/* ---------- Process phase data ---------- */

		if(phase_ready)
		{
		    phase_ready = 0;

		    ade9000_parse_burst_block(phase_spi_rx,
		                              ADE9000_BURST_PHASE_A_REG_COUNT,
		                              phase_shared->phase_regs);

		    phase_shared->new_data = 1;
		}

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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 50;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

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

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CM7_LD1_GPIO_Port, CM7_LD1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, ADE9000_SPI1_CS_Pin|ADE9000_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ADE9000_PM1_GPIO_Port, ADE9000_PM1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : CM7_LD1_Pin */
  GPIO_InitStruct.Pin = CM7_LD1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CM7_LD1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ADE9000_DREADY_INT_Pin */
  GPIO_InitStruct.Pin = ADE9000_DREADY_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(ADE9000_DREADY_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ADE9000_SPI1_CS_Pin ADE9000_RESET_Pin */
  GPIO_InitStruct.Pin = ADE9000_SPI1_CS_Pin|ADE9000_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ADE9000_PM1_Pin */
  GPIO_InitStruct.Pin = ADE9000_PM1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ADE9000_PM1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PG13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : ADE9000_IRQ0_Pin */
  GPIO_InitStruct.Pin = ADE9000_IRQ0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ADE9000_IRQ0_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(ADE9000_IRQ0_EXTI_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(ADE9000_IRQ0_EXTI_IRQn);

  HAL_NVIC_SetPriority(ADE9000_DREADY_INT_EXTI_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(ADE9000_DREADY_INT_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
//  HAL_NVIC_DisableIRQ(ADE9000_IRQ0_EXTI_IRQn);
//  HAL_NVIC_DisableIRQ(ADE9000_DREADY_INT_EXTI_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
uint16_t crc16_ccitt(uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    for(uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;

        for(uint8_t j = 0; j < 8; j++)
        {
            if(crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }

    return crc;
}


/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

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
     ex: LOG("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
