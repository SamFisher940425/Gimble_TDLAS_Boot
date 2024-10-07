/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "can.h"
#include "dma.h"
#include "iwdg.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "flash.h"
#include "ota.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef void (*pFunction)(void); /*!< Function pointer definition */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern enum OTA_STATUS g_ota_status;
extern uint64_t last_cmd_time;

uint8_t g_nvm_data[NVM_DATA_CNT] = {0};
uint8_t g_gimble_id = 1;

volatile uint32_t g_ota_flag = 0;
volatile uint32_t g_fw_size = 0;

volatile uint8_t g_rs485_c1_state = 0; // 0 idle 1 receiving 2 decoding 3 sending
volatile uint8_t g_rs485_c1_tx_buf[RS485_C1_TX_DATA_LENGTH] = {0};
volatile uint8_t g_rs485_c1_rx_buf[RS485_C1_RX_DATA_LENGTH] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
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
  __HAL_DBGMCU_FREEZE_IWDG();
  __HAL_DBGMCU_FREEZE_WWDG();

  // valid firmware or blank
  g_ota_flag = Read_OTA_Flag();
  g_fw_size = Read_OTA_Size();
  // Read_Nvm_Data(g_nvm_data);
  // g_gimble_id = g_nvm_data[0];
  if (g_gimble_id < 1)
  {
    g_gimble_id = 1;
  }
  if (g_gimble_id > 254)
  {
    g_gimble_id = 254;
  }
  g_nvm_data[0] = g_gimble_id;
  Save_Nvm_Data(g_nvm_data);

  // g_ota_flag = OTA_GOTO_BOOT_FLAG;
  if (((g_ota_flag == OTA_GOTO_JUMP_FLAG) || (g_ota_flag == 0xFFFFFFFF)) && ((*(uint32_t *)(MAIN_PROGRAM_ADDR)) != 0xFFFFFFFF))
  {
    Jump_To_Main_Application();
  }
  if (g_ota_flag == OTA_GOTO_BOOT_FLAG)
  {
    g_ota_status = OTA_STATUS_NOT_STARTED_IN_BOOT;
    Erase_All_Page();
  }

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_RTC_Init();
  MX_IWDG_Init();
  MX_CAN_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  RS485_Status_Set(RS485_CH1, RS485_READ);
  HAL_UART_Receive_IT(&huart3, (uint8_t *)g_rs485_c1_rx_buf, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    HAL_IWDG_Refresh(&hiwdg);

    OTA_CMD_Parse();

    if (g_ota_status == OTA_STATUS_NOT_STARTED_IN_BOOT)
    {
      HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
    }
    else if (g_ota_status == OTA_STATUS_UPDATING)
    {
      if (HAL_GetTick() - last_cmd_time < 1000)
      {
        HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
      }
      else
      {
        HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
      }
    }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void Jump_To_Main_Application(void)
{
  uint32_t JumpAddress = *(__IO uint32_t *)(MAIN_PROGRAM_ADDR + 4);
  pFunction Jump_To_Application;

  // disable systick
  SysTick->CTRL = 0;

  __set_MSP(*(__IO uint32_t *)MAIN_PROGRAM_ADDR);
  // SCB->VTOR = MAIN_PROGRAM_ADDR;
  Jump_To_Application = (pFunction)JumpAddress;
  Jump_To_Application();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    HAL_GPIO_TogglePin(LED_3_GPIO_Port, LED_3_Pin);
    OTA_Data_Frame_Receive(g_rs485_c1_rx_buf[0]);
    HAL_UART_Receive_IT(&huart3, (uint8_t *)g_rs485_c1_rx_buf, 1);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    RS485_Status_Set(RS485_CH1, RS485_READ);
  }
}

void RS485_Status_Set(RS485_Channel ch, RS485_Status status)
{
  if (status == RS485_READ)
  {
    switch (ch)
    {
    case RS485_CH1:
      HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(RS485_1_RE_GPIO_Port, RS485_1_RE_Pin, GPIO_PIN_RESET);
      break;
    case RS485_CH2:
      HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_RESET);
      HAL_GPIO_WritePin(RS485_2_RE_GPIO_Port, RS485_2_RE_Pin, GPIO_PIN_RESET);
      break;

    default:
      break;
    }
  }
  else if (status == RS485_WRITE)
  {
    switch (ch)
    {
    case RS485_CH1:
      HAL_GPIO_WritePin(RS485_1_DE_GPIO_Port, RS485_1_DE_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(RS485_1_RE_GPIO_Port, RS485_1_RE_Pin, GPIO_PIN_SET);
      break;
    case RS485_CH2:
      HAL_GPIO_WritePin(RS485_2_DE_GPIO_Port, RS485_2_DE_Pin, GPIO_PIN_SET);
      HAL_GPIO_WritePin(RS485_2_RE_GPIO_Port, RS485_2_RE_Pin, GPIO_PIN_SET);
      break;

    default:
      break;
    }
  }
  else
  {
    return;
  }
}
/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM2 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
