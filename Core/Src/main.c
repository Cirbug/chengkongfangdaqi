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
#include "dac.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DAC_VREF_MV             3300U
#define DAC_MAX_CODE            4095U

#define CONTROL_VOLTAGE_MIN_MV  1800U
#define CONTROL_VOLTAGE_MAX_MV  3200U
#define CONTROL_VOLTAGE_STEP_MV  100U
#define CONTROL_VOLTAGE_INIT_MV  2500U

#define BUTTON_DEBOUNCE_MS       30U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState stable_state;
  GPIO_PinState last_sample;
  uint32_t sample_changed_tick;
} ButtonState;

static uint16_t control_voltage_mv = CONTROL_VOLTAGE_INIT_MV;

static ButtonState key_up =
{
  KEY_UP_GPIO_Port,
  KEY_UP_Pin,
  GPIO_PIN_RESET,
  GPIO_PIN_RESET,
  0U
};

static ButtonState key_down =
{
  KEY_0_GPIO_Port,
  KEY_0_Pin,
  GPIO_PIN_RESET,
  GPIO_PIN_RESET,
  0U
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t Button_WasPressed(ButtonState *button);
static void ControlVoltage_Set(uint16_t voltage_mv);
static void Display_Init(void);
static void Display_UpdateVoltage(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint8_t Button_WasPressed(ButtonState *button)
{
  GPIO_PinState sample = HAL_GPIO_ReadPin(button->port, button->pin);
  uint32_t now = HAL_GetTick();

  if (sample != button->last_sample)
  {
    button->last_sample = sample;
    button->sample_changed_tick = now;
  }

  if ((sample != button->stable_state) &&
      ((now - button->sample_changed_tick) >= BUTTON_DEBOUNCE_MS))
  {
    button->stable_state = sample;
    if (sample == GPIO_PIN_SET)
    {
      return 1U;
    }
  }

  return 0U;
}

static void ControlVoltage_Set(uint16_t voltage_mv)
{
  uint32_t dac_code;

  if (voltage_mv < CONTROL_VOLTAGE_MIN_MV)
  {
    voltage_mv = CONTROL_VOLTAGE_MIN_MV;
  }
  else if (voltage_mv > CONTROL_VOLTAGE_MAX_MV)
  {
    voltage_mv = CONTROL_VOLTAGE_MAX_MV;
  }

  control_voltage_mv = voltage_mv;
  dac_code = ((uint32_t)voltage_mv * DAC_MAX_CODE + (DAC_VREF_MV / 2U)) /
             DAC_VREF_MV;

  if (HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_code) != HAL_OK)
  {
    Error_Handler();
  }
}

static void Display_Init(void)
{
  LCD_Init();
  LCD_Display_Dir(1);
  BACK_COLOR = WHITE;
  LCD_Clear(WHITE);

  POINT_COLOR = RED;
  LCD_ShowString(28, 18, 280, 28, 24, (uint8_t *)"Programmable Amplifier");

  POINT_COLOR = BLACK;
  LCD_ShowString(72, 62, 190, 20, 16, (uint8_t *)"Control Voltage");

  POINT_COLOR = GRAY;
  LCD_ShowString(58, 145, 220, 20, 16, (uint8_t *)"KEY_UP : +0.1V");
  LCD_ShowString(58, 171, 220, 20, 16, (uint8_t *)"KEY_0  : -0.1V");

  POINT_COLOR = BLUE;
  LCD_ShowString(80, 207, 180, 20, 16, (uint8_t *)"Range: 1.8-3.2V");

  Display_UpdateVoltage();
}

static void Display_UpdateVoltage(void)
{
  uint32_t integer_part = control_voltage_mv / 1000U;
  uint32_t decimal_part = (control_voltage_mv % 1000U) / 100U;

  LCD_Fill(92, 92, 228, 125, WHITE);
  POINT_COLOR = GREEN;
  LCD_ShowNum(112, 96, integer_part, 1, 24);
  LCD_ShowString(124, 96, 16, 28, 24, (uint8_t *)".");
  LCD_ShowNum(136, 96, decimal_part, 1, 24);
  LCD_ShowString(156, 96, 40, 28, 24, (uint8_t *)"V");
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
  MX_DAC_Init();
  MX_FSMC_Init();
  /* USER CODE BEGIN 2 */
  if (HAL_DAC_Start(&hdac, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  ControlVoltage_Set(CONTROL_VOLTAGE_INIT_MV);
  Display_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (Button_WasPressed(&key_up))
    {
      if (control_voltage_mv < CONTROL_VOLTAGE_MAX_MV)
      {
        ControlVoltage_Set(control_voltage_mv + CONTROL_VOLTAGE_STEP_MV);
        Display_UpdateVoltage();
      }
    }

    if (Button_WasPressed(&key_down))
    {
      if (control_voltage_mv > CONTROL_VOLTAGE_MIN_MV)
      {
        ControlVoltage_Set(control_voltage_mv - CONTROL_VOLTAGE_STEP_MV);
        Display_UpdateVoltage();
      }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
