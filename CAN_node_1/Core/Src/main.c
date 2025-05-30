/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdlib.h"
#include "string.h"
#include "kalman_filter.h"
#include "ssd1306_udf.h"
#include "ssd1306_graphic.h"
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
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

CAN_HandleTypeDef hcan;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

osThreadId displayHandle;
osThreadId send_messageHandle;
osThreadId dashboardHandle;
osMessageQId q_steering_controlHandle;
osMessageQId q_throttle_controlHandle;
osMessageQId q_turn_signalHandle;
osMessageQId q_trim_steeringHandle;
osMessageQId q_disp_steeringHandle;
osMessageQId q_disp_turn_signalHandle;
osMessageQId q_disp_throttleHandle;
osMessageQId q_throttle_fbHandle;
/* USER CODE BEGIN PV */

CAN_TxHeaderTypeDef   tx_header;
uint8_t               tx_data[8];
uint32_t              tx_mailbox;

CAN_RxHeaderTypeDef   rx_header;
uint8_t               rx_data[8];

uint16_t	adc_value[2];
uint16_t 	speed_ppr_fb;
uint16_t 	speed_rpm_fb;
kalman_t 	*kalman_1, *kalman_2;

uint8_t 	status_motor = 0;		// 0 - stop, 1 - start, 2 - error

uint8_t 	trim_button = 0;
uint8_t 	light_sw = 0;
enum light_mode_t light_mode = stop_light;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_CAN_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_ADC1_Init(void);
void StartDefaultTask(void const * argument);
void StartTask01(void const * argument);
void StartTask02(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void clear_buff (uint8_t * buff_clear, size_t num)
{
	if (buff_clear == NULL) return;

	memset(buff_clear, 0, num);
}

uint8_t check_status_motor (uint16_t throttle_control, uint16_t speed_fb, uint32_t timeout_ms)
{
	static uint32_t curr_tick_cnt = 0;
	static uint32_t pre_tick_cnt = 0;
	static uint16_t pre_throttle_control;
	static uint8_t 	ret = 1;

	throttle_control = throttle_control*144/4096;

	curr_tick_cnt = osKernelSysTick();

	if (curr_tick_cnt - pre_tick_cnt > timeout_ms)
	{
		if ((pre_throttle_control - speed_fb) > 30)
		{
			ret = 2;
		}
		pre_throttle_control = throttle_control;
		pre_tick_cnt = curr_tick_cnt;
	}
	return ret;
}

void disp_status_motor (uint8_t _status_motor)
{
	if (_status_motor == 2)
	{
	static uint32_t curr_tick_cnt = 0;
	static uint32_t pre_tick_cnt = 0;

	curr_tick_cnt = osKernelSysTick();
		if (curr_tick_cnt - pre_tick_cnt > 1000)
		{
			disp_toggle_turn_signal(error_blinker);
		}
	}
}

void prepare_buff_send ()
{
	static uint16_t pre_throttle = 0;
	static uint16_t pre_steering = 0;
	static uint16_t pre_light = 0;

	uint16_t current_value;
	osEvent q_message;
	uint8_t * p_buff_ptr = (uint8_t *)&current_value;

	tx_header.IDE = CAN_ID_STD;

	/* get element from throttle queue to send motor node control speed motor */
	q_message = osMessageGet(q_throttle_controlHandle, 0);
	status_motor = check_status_motor((uint16_t)q_message.value.v, speed_ppr_fb, 100);

	/* get a message in q_throttle_control if it is not empty */
	if (q_message.status == osEventMessage)
	{
		/* check if the value has changed, then send a message */
		current_value = (uint16_t)q_message.value.v;
		if (abs(current_value - pre_throttle) > 10)
		{
			pre_throttle = current_value;
			tx_header.StdId = 0x100;
			tx_header.RTR = CAN_RTR_DATA;
			tx_header.DLC = 2;

			tx_data[0] = *p_buff_ptr;
			p_buff_ptr++;
			tx_data[1] = *p_buff_ptr;
			if (HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
			{
			   Error_Handler ();
			}
			clear_buff(&tx_data, sizeof(uint8_t)*8);
			p_buff_ptr = &current_value;
		}
	}

	/* get element from steering queue to send motor node control angle servo */
	q_message = osMessageGet(q_steering_controlHandle, 0);

	/* get a message in q_steering_control if it is not empty */
	if (q_message.status == osEventMessage)
	{
		/* check if the value has changed, then send a message */
		current_value = (uint16_t)q_message.value.v;
		if (abs(current_value - pre_steering) > 10)
		{
			pre_steering = current_value;
			tx_header.StdId = 0x110;
			tx_header.RTR = CAN_RTR_DATA;
			tx_header.DLC = 2;

			tx_data[0] = *p_buff_ptr;
			p_buff_ptr++;
			tx_data[1] = *p_buff_ptr;
			if (HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
			{
			   Error_Handler ();
			}
			clear_buff(&tx_data, sizeof(uint8_t)*8);
			p_buff_ptr = &current_value;
		}
	}

	/* get element from light queue to send light node control light */
	q_message = osMessageGet(q_turn_signalHandle, 0);

	/* get a message in q_light_control if it is not empty */
	if (q_message.status == osEventMessage)
	{
		current_value = (uint16_t)q_message.value.v;
		if (current_value != pre_light)
		{
			pre_light = current_value;
			tx_header.StdId = 0x210;
			tx_header.RTR = CAN_RTR_DATA;
			tx_header.DLC = 1;

			tx_data[0] = current_value;
			if (HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
			{
			   Error_Handler ();
			}
			clear_buff(&tx_data, sizeof(uint8_t)*8);

		}
	}
}

void emergency_buff_send ()
{
	tx_header.IDE = CAN_ID_STD;

	/* stop motor emergency */
	if (status_motor == 0)
	{
		tx_header.StdId = 0x102;
		tx_header.RTR = CAN_RTR_DATA;
		tx_header.DLC = 1;

		if (HAL_CAN_AddTxMessage(&hcan, &tx_header, 0, &tx_mailbox) != HAL_OK)
		{
		   Error_Handler ();
		}
	}
}

void dashboard_button ()
{
	static uint8_t pre_sw_button = 0;
	tx_header.IDE = CAN_ID_STD;

	/* check if the value has changed, then send a message */
	read_trim_button();
	if (trim_button != pre_sw_button)
	{
		pre_sw_button = trim_button;
		tx_header.StdId = 0x111;
		tx_header.RTR = CAN_RTR_DATA;
		tx_header.DLC = 1;

		if (HAL_CAN_AddTxMessage(&hcan, &tx_header, &trim_button, &tx_mailbox) != HAL_OK)
		{
		   Error_Handler ();
		}
		clear_buff(&tx_data, sizeof(uint8_t)*8);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	  adc_value[0] = update_kalman(kalman_1, adc_value[0]);
	  adc_value[1] = update_kalman(kalman_2, adc_value[1]);
}

void read_light_sw ()
{
	/* Set bit 0  */
	light_sw |= (1 << 0);

	/* ON/OFF hazard mode */
	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET)
	{
		light_sw |= (1 << 1);
	}
	else
	{
		light_sw &= ~(1 << 1);
	}

	/* ON/OFF blinker left light */
	if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
	{
		light_sw |= (1 << 2);
	}
	else
	{
		light_sw &= ~(1 << 2);
	}

	/* ON/OFF blinker right light */
	if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_14) == GPIO_PIN_RESET)
	{
		light_sw |= (1 << 3);
	}
	else
	{
		light_sw &= ~(1 << 3);
	}
}

void read_trim_button ()
{
	/* trimming down steering */
	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET)
	{
		trim_button |= (1 << 0);
	}
	else
	{
		trim_button &= ~(1 << 0);
	}

	/* trimming up steering */
	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
	{
		trim_button |= (1 << 1);
	}
	else
	{
		trim_button &= ~(1 << 1);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	static uint8_t 	period_read_ADC_cnt = 0;
	if (htim->Instance == htim2.Instance)
	{
		if (period_read_ADC_cnt == 1)
		{
			HAL_ADC_Start_DMA(&hadc1, &adc_value, 2);
		}
		else if (period_read_ADC_cnt > 1)
		{
			HAL_ADC_Stop_DMA(&hadc1);
			period_read_ADC_cnt = 0;
		}
		period_read_ADC_cnt++;
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
	  {
	    Error_Handler();
	  }
	  if (rx_header.StdId == 0x211)
	  {
		  displ_turn_signal(rx_data[0]);
		  clear_buff(&rx_data, sizeof(uint8_t)*8);
	  }
	  if (rx_header.StdId == 0x101)
	  {
		  speed_ppr_fb = (uint16_t)(rx_data[0] | (rx_data[1] << 8));
		  speed_rpm_fb = speed_ppr_fb * 1.4;
		  clear_buff(&rx_data, sizeof(uint8_t)*8);
	  }
}

void displ_turn_signal (uint8_t feedback_turn_signal)
{
	if ((feedback_turn_signal >> 1) & 0x01)
	{
		disp_toggle_turn_signal(hazard_blinker);
	}
	else if ((feedback_turn_signal >> 2) & 0x01)
	{
		disp_toggle_turn_signal(left_blinker);
	}
	else if ((feedback_turn_signal >> 3) & 0x01)
	{
		disp_toggle_turn_signal(right_blinker);
	}
	else
	{
		disp_off_turn_signal();
	}
	ssd1306_UpdateScreen();
}

void blinker_led ()
{
	static uint32_t curr_tick_cnt = 0;
	static uint32_t pre_tick_cnt = 0;

	curr_tick_cnt = osKernelSysTick();
	if (curr_tick_cnt - pre_tick_cnt > 1000)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET);
	}
	if (curr_tick_cnt - pre_tick_cnt > 1030)
	{
		pre_tick_cnt = curr_tick_cnt;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);
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
  MX_DMA_Init();
  MX_CAN_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  HAL_CAN_Start(&hcan);
  HAL_TIM_Base_Start_IT(&htim2);
  HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
  kalman_1 = create_kalman(50, 50, 0.01);
  kalman_2 = create_kalman(50, 50, 0.01);
  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of q_steering_control */
  osMessageQDef(q_steering_control, 32, uint16_t);
  q_steering_controlHandle = osMessageCreate(osMessageQ(q_steering_control), NULL);

  /* definition and creation of q_throttle_control */
  osMessageQDef(q_throttle_control, 32, uint16_t);
  q_throttle_controlHandle = osMessageCreate(osMessageQ(q_throttle_control), NULL);

  /* definition and creation of q_turn_signal */
  osMessageQDef(q_turn_signal, 16, uint8_t);
  q_turn_signalHandle = osMessageCreate(osMessageQ(q_turn_signal), NULL);

  /* definition and creation of q_trim_steering */
  osMessageQDef(q_trim_steering, 32, uint8_t);
  q_trim_steeringHandle = osMessageCreate(osMessageQ(q_trim_steering), NULL);

  /* definition and creation of q_disp_steering */
  osMessageQDef(q_disp_steering, 16, uint16_t);
  q_disp_steeringHandle = osMessageCreate(osMessageQ(q_disp_steering), NULL);

  /* definition and creation of q_disp_turn_signal */
  osMessageQDef(q_disp_turn_signal, 16, uint8_t);
  q_disp_turn_signalHandle = osMessageCreate(osMessageQ(q_disp_turn_signal), NULL);

  /* definition and creation of q_disp_throttle */
  osMessageQDef(q_disp_throttle, 16, uint16_t);
  q_disp_throttleHandle = osMessageCreate(osMessageQ(q_disp_throttle), NULL);

  /* definition and creation of q_throttle_fb */
  osMessageQDef(q_throttle_fb, 32, uint16_t);
  q_throttle_fbHandle = osMessageCreate(osMessageQ(q_throttle_fb), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of display */
  osThreadDef(display, StartDefaultTask, osPriorityNormal, 0, 128);
  displayHandle = osThreadCreate(osThread(display), NULL);

  /* definition and creation of send_message */
  osThreadDef(send_message, StartTask01, osPriorityNormal, 0, 128);
  send_messageHandle = osThreadCreate(osThread(send_message), NULL);

  /* definition and creation of dashboard */
  osThreadDef(dashboard, StartTask02, osPriorityNormal, 0, 128);
  dashboardHandle = osThreadCreate(osThread(dashboard), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 9;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_3TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = DISABLE;
  hcan.Init.AutoWakeUp = DISABLE;
  hcan.Init.AutoRetransmission = DISABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */
  CAN_FilterTypeDef canfilterconfig;

  canfilterconfig.FilterActivation = ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterIdHigh = 0;
  canfilterconfig.FilterIdLow = 0;
  canfilterconfig.FilterMaskIdHigh = 0;
  canfilterconfig.FilterMaskIdLow = 0;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.SlaveStartFilterBank = 10;

  HAL_CAN_ConfigFilter(&hcan, &canfilterconfig);

  /* USER CODE END CAN_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 72-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 10000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the display thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
				/* this task is displays information */
  /* Infinite loop */
	disp_draw_dashboard();
	for(;;)
	{
		blinker_led();
		disp_status_motor(status_motor);
		disp_msg_uint(speed_ppr_fb, 95, 40);
		disp_needle_speed(speed_ppr_fb, speed_rpm_fb);
	}
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartTask01 */
/**
* @brief Function implementing the send_message thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask01 */
void StartTask01(void const * argument)
{
  /* USER CODE BEGIN StartTask01 */
				/* this task is prepares message and send message */
  /* Infinite loop */
	for(;;)
	{
		emergency_buff_send();
		prepare_buff_send();
		osDelay(1);
	}
  /* USER CODE END StartTask01 */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the dashboard thread.
* @param argument: Not used
* @retval None
*/

/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
				/* this task is get data from dashboard */
  /* Infinite loop */
	status_motor = 1;
	for(;;)
	{
		osMessagePut(q_throttle_controlHandle, adc_value[0], 0);
		osMessagePut(q_steering_controlHandle, adc_value[1], 0);
		osMessagePut(q_disp_throttleHandle, adc_value[1], 0);

		read_light_sw();
		osMessagePut(q_turn_signalHandle, light_sw, 0);
		osMessagePut(q_disp_turn_signalHandle, light_sw, 0);

		read_trim_button();
		osMessagePut(q_trim_steeringHandle, trim_button, 0);
		osDelay(1);
	}
  /* USER CODE END StartTask02 */
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

#ifdef  USE_FULL_ASSERT
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
