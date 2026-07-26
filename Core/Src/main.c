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
#include "ad9954.h"
#include "lcd.h"
#include "touch.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  EDIT_NONE = 0,
  EDIT_FREQUENCY,
  EDIT_AMPLITUDE
} EditField;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DAC_VREF_MV             3300U
#define DAC_MAX_CODE            4095U

#define CONTROL_VOLTAGE_MIN_MV  1800U
#define CONTROL_VOLTAGE_MAX_MV  3200U
#define CONTROL_VOLTAGE_STEP_MV   10U
#define CONTROL_VOLTAGE_INIT_MV  2500U

#define BUTTON_DEBOUNCE_MS       30U

#define DDS_FREQUENCY_MIN_HZ       1UL
#define DDS_FREQUENCY_MAX_HZ       10000000UL
#define DDS_FREQUENCY_INIT_HZ      1000UL
#define DDS_AMPLITUDE_MAX_MVPP     AD9954_FULL_SCALE_MVPP
#define DDS_AMPLITUDE_INIT_MVPP    100U

#define TOUCH_SCAN_PERIOD_MS       10U
#define EDIT_BUFFER_CAPACITY       10U

#define KEYPAD_LEFT                8U
#define KEYPAD_TOP                 74U
#define KEYPAD_KEY_WIDTH           96U
#define KEYPAD_KEY_HEIGHT          34U
#define KEYPAD_COLUMN_PITCH        104U
#define KEYPAD_ROW_PITCH           39U

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
static uint32_t dds_frequency_hz = DDS_FREQUENCY_INIT_HZ;
static uint16_t dds_amplitude_mvpp = DDS_AMPLITUDE_INIT_MVPP;
static uint8_t dds_ready = 0U;
static uint8_t dds_enabled = 1U;
static uint8_t dds_apply_ok = 0U;

static TouchState touch_state = {0U, 0U, 0U, 0U, 0U};
static uint8_t touch_was_pressed = 0U;
static uint32_t touch_last_scan_tick = 0U;

static EditField edit_field = EDIT_NONE;
static char edit_buffer[EDIT_BUFFER_CAPACITY] = {0};
static uint8_t edit_length = 0U;
static uint8_t edit_invalid = 0U;

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
static void ControlVoltage_Adjust(int32_t delta_mv);
static void Dds_Apply(void);
static void Ui_Init(void);
static void Ui_DrawMain(void);
static void Ui_UpdateDacValue(void);
static void Ui_DrawButton(uint16_t left, uint16_t top,
                          uint16_t right, uint16_t bottom,
                          const char *label, uint16_t color);
static void Ui_BeginEdit(EditField field);
static void Ui_DrawKeypad(void);
static void Ui_DrawEditValue(void);
static void Ui_HandleTouch(uint16_t x, uint16_t y);
static void Ui_HandleKeypadTouch(uint16_t x, uint16_t y);
static void Ui_AppendKey(char key);
static uint32_t Ui_ParseEditValue(void);
static void Touch_Process(void);

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

static void ControlVoltage_Adjust(int32_t delta_mv)
{
  int32_t new_voltage = (int32_t)control_voltage_mv + delta_mv;

  if (new_voltage < (int32_t)CONTROL_VOLTAGE_MIN_MV)
  {
    new_voltage = (int32_t)CONTROL_VOLTAGE_MIN_MV;
  }
  else if (new_voltage > (int32_t)CONTROL_VOLTAGE_MAX_MV)
  {
    new_voltage = (int32_t)CONTROL_VOLTAGE_MAX_MV;
  }

  if ((uint16_t)new_voltage != control_voltage_mv)
  {
    ControlVoltage_Set((uint16_t)new_voltage);
    if (edit_field == EDIT_NONE)
    {
      Ui_UpdateDacValue();
    }
  }
}

static void Dds_Apply(void)
{
  if (dds_ready == 0U)
  {
    dds_apply_ok = 0U;
    return;
  }

  if (dds_enabled != 0U)
  {
    dds_apply_ok = Ad9954_SetOutputMv(dds_frequency_hz,
                                      dds_amplitude_mvpp);
  }
  else
  {
    dds_apply_ok = Ad9954_SetOutputMv(dds_frequency_hz, 0U);
  }
}

static void Ui_DrawButton(uint16_t left, uint16_t top,
                          uint16_t right, uint16_t bottom,
                          const char *label, uint16_t color)
{
  uint16_t length = 0U;
  uint16_t text_width;
  uint16_t x;
  uint16_t y;

  while (label[length] != '\0')
  {
    ++length;
  }

  text_width = length * 8U;
  x = left + (((right - left + 1U) > text_width) ?
              ((right - left + 1U - text_width) / 2U) : 0U);
  y = top + (((bottom - top + 1U) > 16U) ?
             ((bottom - top + 1U - 16U) / 2U) : 0U);

  POINT_COLOR = color;
  LCD_DrawRectangle(left, top, right, bottom);
  LCD_ShowString(x, y, text_width + 2U, 16, 16, (uint8_t *)label);
}

static void Ui_Init(void)
{
  LCD_Init();
  LCD_Display_Dir(1);
  BACK_COLOR = WHITE;
  Touch_Init();

  dds_ready = Ad9954_Init();
  Dds_Apply();
  Ui_DrawMain();
}

static void Ui_DrawMain(void)
{
  LCD_Clear(WHITE);
  BACK_COLOR = WHITE;

  POINT_COLOR = RED;
  LCD_ShowString(58, 5, 210, 24, 24, (uint8_t *)"DAC + DDS CONTROL");

  POINT_COLOR = BLACK;
  LCD_ShowString(8, 47, 48, 16, 16, (uint8_t *)"DAC:");
  LCD_DrawRectangle(62, 38, 170, 71);
  Ui_DrawButton(190, 38, 242, 71, "-", BLUE);
  Ui_DrawButton(252, 38, 304, 71, "+", BLUE);

  POINT_COLOR = BLACK;
  LCD_ShowString(8, 87, 48, 16, 16, (uint8_t *)"FREQ:");
  POINT_COLOR = BLUE;
  LCD_DrawRectangle(62, 79, 310, 109);
  POINT_COLOR = BLACK;
  LCD_ShowNum(74, 87, dds_frequency_hz, 8, 16);
  LCD_ShowString(143, 87, 24, 16, 16, (uint8_t *)"Hz");
  POINT_COLOR = GRAY;
  LCD_ShowString(221, 87, 72, 16, 16, (uint8_t *)"TAP SET");

  POINT_COLOR = BLACK;
  LCD_ShowString(8, 125, 48, 16, 16, (uint8_t *)"AMP:");
  POINT_COLOR = BLUE;
  LCD_DrawRectangle(62, 117, 310, 147);
  POINT_COLOR = BLACK;
  LCD_ShowNum(74, 125, dds_amplitude_mvpp, 3, 16);
  LCD_ShowString(105, 125, 48, 16, 16, (uint8_t *)"mVpp");
  POINT_COLOR = GRAY;
  LCD_ShowString(221, 125, 72, 16, 16, (uint8_t *)"TAP SET");

  if (dds_ready == 0U)
  {
    POINT_COLOR = RED;
    LCD_ShowString(105, 156, 112, 16, 16, (uint8_t *)"DDS INIT ERROR");
  }
  else if (dds_apply_ok == 0U)
  {
    POINT_COLOR = RED;
    LCD_ShowString(101, 156, 120, 16, 16, (uint8_t *)"DDS APPLY ERROR");
  }
  else
  {
    POINT_COLOR = GREEN;
    LCD_ShowString(120, 156, 80, 16, 16, (uint8_t *)"DDS READY");
  }

  Ui_DrawButton(75, 178, 245, 212,
                (dds_enabled != 0U) ? "DDS OUTPUT ON" : "DDS OUTPUT OFF",
                (dds_enabled != 0U) ? GREEN : RED);

  POINT_COLOR = BLUE;
  LCD_ShowString(78, 220, 180, 12, 12,
                 (uint8_t *)"F:1-10MHz A:0-500mVpp");

  Ui_UpdateDacValue();
}

static void Ui_UpdateDacValue(void)
{
  uint32_t integer_part = control_voltage_mv / 1000U;
  uint32_t decimal_part = (control_voltage_mv % 1000U) / 10U;

  LCD_Fill(64, 40, 168, 69, WHITE);
  POINT_COLOR = GREEN;
  LCD_ShowNum(75, 44, integer_part, 1, 24);
  LCD_ShowString(87, 44, 14, 24, 24, (uint8_t *)".");
  LCD_ShowxNum(99, 44, decimal_part, 2, 24, 0x80U);
  LCD_ShowString(127, 44, 18, 24, 24, (uint8_t *)"V");
}

static void Ui_BeginEdit(EditField field)
{
  edit_field = field;
  edit_length = 0U;
  edit_buffer[0] = '\0';
  edit_invalid = 0U;
  Ui_DrawKeypad();
}

static void Ui_DrawEditValue(void)
{
  LCD_Fill(10, 43, 268, 67, WHITE);
  POINT_COLOR = BLACK;

  if (edit_length != 0U)
  {
    LCD_ShowString(18, 48, 120, 16, 16, (uint8_t *)edit_buffer);
  }
  else if (edit_field == EDIT_FREQUENCY)
  {
    LCD_ShowNum(18, 48, dds_frequency_hz, 8, 16);
  }
  else
  {
    LCD_ShowNum(18, 48, dds_amplitude_mvpp, 3, 16);
  }

  if (edit_invalid != 0U)
  {
    POINT_COLOR = RED;
    LCD_ShowString(180, 48, 64, 16, 16, (uint8_t *)"INVALID");
  }
}

static void Ui_DrawKeypad(void)
{
  static const char keys[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'<', '0', 'O'}
  };

  LCD_Clear(WHITE);
  BACK_COLOR = WHITE;
  POINT_COLOR = RED;
  LCD_ShowString(8, 7, 190, 24, 24,
                 (uint8_t *)((edit_field == EDIT_FREQUENCY) ?
                             "SET FREQUENCY" : "SET AMPLITUDE"));

  POINT_COLOR = GRAY;
  LCD_ShowString(202, 12, 78, 12, 12,
                 (uint8_t *)((edit_field == EDIT_FREQUENCY) ?
                             "1-10000000Hz" : "0-500mVpp"));

  POINT_COLOR = BLUE;
  LCD_DrawRectangle(8, 41, 270, 69);
  Ui_DrawButton(280, 38, 315, 70, "X", RED);

  for (uint8_t row = 0U; row < 4U; ++row)
  {
    for (uint8_t col = 0U; col < 3U; ++col)
    {
      uint16_t left = KEYPAD_LEFT + (uint16_t)col * KEYPAD_COLUMN_PITCH;
      uint16_t top = KEYPAD_TOP + (uint16_t)row * KEYPAD_ROW_PITCH;
      char label[3] = {keys[row][col], '\0', '\0'};

      if (keys[row][col] == 'O')
      {
        label[0] = 'O';
        label[1] = 'K';
      }

      Ui_DrawButton(left, top,
                    left + KEYPAD_KEY_WIDTH - 1U,
                    top + KEYPAD_KEY_HEIGHT - 1U,
                    label, BLUE);
    }
  }

  Ui_DrawEditValue();
}

static void Ui_AppendKey(char key)
{
  if (key == '<')
  {
    if (edit_length > 0U)
    {
      --edit_length;
      edit_buffer[edit_length] = '\0';
    }
  }
  else if ((key >= '0') && (key <= '9') &&
           (edit_length < (EDIT_BUFFER_CAPACITY - 1U)))
  {
    edit_buffer[edit_length] = key;
    ++edit_length;
    edit_buffer[edit_length] = '\0';
  }

  edit_invalid = 0U;
}

static uint32_t Ui_ParseEditValue(void)
{
  uint32_t value = 0U;

  for (uint8_t i = 0U; i < edit_length; ++i)
  {
    value = value * 10U + (uint32_t)(edit_buffer[i] - '0');
  }

  return value;
}

static void Ui_HandleKeypadTouch(uint16_t x, uint16_t y)
{
  static const char keys[4][3] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'<', '0', 'O'}
  };

  if ((x >= 280U) && (x <= 319U) && (y >= 34U) && (y <= 72U))
  {
    edit_field = EDIT_NONE;
    Ui_DrawMain();
    return;
  }

  for (uint8_t row = 0U; row < 4U; ++row)
  {
    for (uint8_t col = 0U; col < 3U; ++col)
    {
      uint16_t left = KEYPAD_LEFT + (uint16_t)col * KEYPAD_COLUMN_PITCH;
      uint16_t top = KEYPAD_TOP + (uint16_t)row * KEYPAD_ROW_PITCH;

      if ((x >= left) && (x < (left + KEYPAD_KEY_WIDTH)) &&
          (y >= top) && (y < (top + KEYPAD_KEY_HEIGHT)))
      {
        char key = keys[row][col];

        if (key == 'O')
        {
          uint32_t value = (edit_length != 0U) ? Ui_ParseEditValue() :
                           ((edit_field == EDIT_FREQUENCY) ?
                            dds_frequency_hz : dds_amplitude_mvpp);
          uint8_t valid = 0U;

          if (edit_field == EDIT_FREQUENCY)
          {
            if ((value >= DDS_FREQUENCY_MIN_HZ) &&
                (value <= DDS_FREQUENCY_MAX_HZ))
            {
              dds_frequency_hz = value;
              valid = 1U;
            }
          }
          else if (value <= DDS_AMPLITUDE_MAX_MVPP)
          {
            dds_amplitude_mvpp = (uint16_t)value;
            valid = 1U;
          }

          if (valid != 0U)
          {
            Dds_Apply();
            edit_field = EDIT_NONE;
            Ui_DrawMain();
          }
          else
          {
            edit_invalid = 1U;
            Ui_DrawEditValue();
          }
        }
        else
        {
          Ui_AppendKey(key);
          Ui_DrawEditValue();
        }
        return;
      }
    }
  }
}

static void Ui_HandleTouch(uint16_t x, uint16_t y)
{
  if (edit_field != EDIT_NONE)
  {
    Ui_HandleKeypadTouch(x, y);
    return;
  }

  if ((x >= 190U) && (x <= 242U) && (y >= 38U) && (y <= 71U))
  {
    ControlVoltage_Adjust(-(int32_t)CONTROL_VOLTAGE_STEP_MV);
  }
  else if ((x >= 252U) && (x <= 304U) &&
           (y >= 38U) && (y <= 71U))
  {
    ControlVoltage_Adjust((int32_t)CONTROL_VOLTAGE_STEP_MV);
  }
  else if ((x >= 62U) && (x <= 310U) &&
           (y >= 79U) && (y <= 109U))
  {
    Ui_BeginEdit(EDIT_FREQUENCY);
  }
  else if ((x >= 62U) && (x <= 310U) &&
           (y >= 117U) && (y <= 147U))
  {
    Ui_BeginEdit(EDIT_AMPLITUDE);
  }
  else if ((x >= 75U) && (x <= 245U) &&
           (y >= 178U) && (y <= 212U))
  {
    dds_enabled = (dds_enabled == 0U) ? 1U : 0U;
    Dds_Apply();
    Ui_DrawMain();
  }
}

static void Touch_Process(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - touch_last_scan_tick) < TOUCH_SCAN_PERIOD_MS)
  {
    return;
  }
  touch_last_scan_tick = now;

  (void)Touch_Scan(&touch_state);
  if ((touch_state.pressed != 0U) && (touch_was_pressed == 0U))
  {
    Ui_HandleTouch(touch_state.x, touch_state.y);
  }
  touch_was_pressed = touch_state.pressed;
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
  Ui_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (Button_WasPressed(&key_up))
    {
      ControlVoltage_Adjust((int32_t)CONTROL_VOLTAGE_STEP_MV);
    }

    if (Button_WasPressed(&key_down))
    {
      ControlVoltage_Adjust(-(int32_t)CONTROL_VOLTAGE_STEP_MV);
    }

    Touch_Process();
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
