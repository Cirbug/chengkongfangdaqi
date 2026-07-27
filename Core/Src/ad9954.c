#include "ad9954.h"

#define AD9954_REG_CFR1  0x00U
#define AD9954_REG_CFR2  0x01U
#define AD9954_REG_ASF   0x02U
#define AD9954_REG_FTW0  0x04U
#define AD9954_REG_POW0  0x05U

/* Pin assignment follows D:\git\2025g. */
#define AD9954_IOSY_PORT GPIOA
#define AD9954_IOSY_PIN  GPIO_PIN_12
#define AD9954_PWR_PORT  GPIOB
#define AD9954_PWR_PIN   GPIO_PIN_10
#define AD9954_RES_PORT  GPIOB
#define AD9954_RES_PIN   GPIO_PIN_6
#define AD9954_CS_PORT   GPIOC
#define AD9954_CS_PIN    GPIO_PIN_6
#define AD9954_PS1_PORT  GPIOC
#define AD9954_PS1_PIN   GPIO_PIN_8
#define AD9954_OSK_PORT  GPIOC
#define AD9954_OSK_PIN   GPIO_PIN_11
#define AD9954_PS0_PORT  GPIOC
#define AD9954_PS0_PIN   GPIO_PIN_12
#define AD9954_SCLK_PORT GPIOD
#define AD9954_SCLK_PIN  GPIO_PIN_6
#define AD9954_UPD_PORT  GPIOG
#define AD9954_UPD_PIN   GPIO_PIN_6
#define AD9954_SDO_PORT  GPIOG
#define AD9954_SDO_PIN   GPIO_PIN_8
#define AD9954_SDIO_PORT GPIOG
#define AD9954_SDIO_PIN  GPIO_PIN_15

static const uint32_t ad9954_system_clock_hz =
  AD9954_REFERENCE_CLOCK_HZ * AD9954_PLL_MULTIPLIER;

static void Ad9954_InitGpio(void);
static void Ad9954_Reset(void);
static void Ad9954_Update(void);
static void Ad9954_SerialDelay(void);
static void Ad9954_WriteByte(uint8_t data);
static void Ad9954_WriteRegister(uint8_t address,
                                 const uint8_t *data,
                                 uint8_t length);

uint8_t Ad9954_Init(void)
{
  const uint8_t cfr1[4] = {0x02U, 0x00U, 0x00U, 0x00U};
  const uint8_t cfr2[3] = {
    0x00U,
    0x00U,
    (uint8_t)((AD9954_PLL_MULTIPLIER << 3U) | 0x07U)
  };

  if ((AD9954_PLL_MULTIPLIER < 4U) ||
      (AD9954_PLL_MULTIPLIER > 20U) ||
      (ad9954_system_clock_hz > 400000000UL))
  {
    return 0U;
  }

  Ad9954_InitGpio();
  Ad9954_Reset();
  HAL_Delay(300U);

  /* Enable manual OSK so the 14-bit amplitude scale register is active. */
  Ad9954_WriteRegister(AD9954_REG_CFR1, cfr1, (uint8_t)sizeof(cfr1));
  Ad9954_WriteRegister(AD9954_REG_CFR2, cfr2, (uint8_t)sizeof(cfr2));
  Ad9954_Update();
  /* Allow the internal PLL to lock before writing FTW/ASF registers. */
  HAL_Delay(10U);
  HAL_GPIO_WritePin(AD9954_OSK_PORT, AD9954_OSK_PIN, GPIO_PIN_SET);
  return 1U;
}

uint8_t Ad9954_SetFrequency(uint32_t frequency_hz)
{
  uint64_t numerator;
  uint32_t tuning_word;
  uint8_t data[4];

  if (frequency_hz >= (ad9954_system_clock_hz / 2U))
  {
    return 0U;
  }

  numerator = ((uint64_t)frequency_hz << 32U) +
              (ad9954_system_clock_hz / 2U);
  tuning_word = (uint32_t)(numerator / ad9954_system_clock_hz);
  data[0] = (uint8_t)(tuning_word >> 24U);
  data[1] = (uint8_t)(tuning_word >> 16U);
  data[2] = (uint8_t)(tuning_word >> 8U);
  data[3] = (uint8_t)tuning_word;

  Ad9954_WriteRegister(AD9954_REG_FTW0, data, (uint8_t)sizeof(data));
  Ad9954_Update();
  return 1U;
}

uint8_t Ad9954_SetAmplitude(uint16_t amplitude_scale)
{
  uint8_t data[2];

  if (amplitude_scale > AD9954_MAX_AMPLITUDE_SCALE)
  {
    return 0U;
  }

  data[0] = (uint8_t)(amplitude_scale >> 8U);
  data[1] = (uint8_t)amplitude_scale;
  Ad9954_WriteRegister(AD9954_REG_ASF, data, (uint8_t)sizeof(data));
  Ad9954_Update();
  return 1U;
}

uint8_t Ad9954_SetAmplitudeMv(uint16_t amplitude_mvpp)
{
  uint32_t amplitude_scale;

  if (amplitude_mvpp > AD9954_FULL_SCALE_MVPP)
  {
    return 0U;
  }

  amplitude_scale = ((uint32_t)amplitude_mvpp *
                     AD9954_MAX_AMPLITUDE_SCALE +
                     (AD9954_FULL_SCALE_MVPP / 2U)) /
                    AD9954_FULL_SCALE_MVPP;
  return Ad9954_SetAmplitude((uint16_t)amplitude_scale);
}

uint8_t Ad9954_SetPhase(uint16_t phase_word)
{
  uint8_t data[2];

  if (phase_word > 0x3FFFU)
  {
    return 0U;
  }

  data[0] = (uint8_t)(phase_word >> 8U);
  data[1] = (uint8_t)phase_word;
  Ad9954_WriteRegister(AD9954_REG_POW0, data, (uint8_t)sizeof(data));
  Ad9954_Update();
  return 1U;
}

uint8_t Ad9954_SetOutputMv(uint32_t frequency_hz,
                           uint16_t amplitude_mvpp)
{
  if ((Ad9954_SetPhase(0U) == 0U) ||
      (Ad9954_SetFrequency(frequency_hz) == 0U) ||
      (Ad9954_SetAmplitudeMv(amplitude_mvpp) == 0U))
  {
    return 0U;
  }

  return 1U;
}

static void Ad9954_InitGpio(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

  HAL_GPIO_WritePin(AD9954_IOSY_PORT, AD9954_IOSY_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_PWR_PORT, AD9954_PWR_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_RES_PORT, AD9954_RES_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_CS_PORT, AD9954_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(AD9954_PS1_PORT, AD9954_PS1_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_OSK_PORT, AD9954_OSK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_PS0_PORT, AD9954_PS0_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_SCLK_PORT, AD9954_SCLK_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_UPD_PORT, AD9954_UPD_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(AD9954_SDIO_PORT, AD9954_SDIO_PIN, GPIO_PIN_RESET);

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_PULLDOWN;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;

  gpio.Pin = AD9954_IOSY_PIN;
  HAL_GPIO_Init(AD9954_IOSY_PORT, &gpio);
  gpio.Pin = AD9954_PWR_PIN | AD9954_RES_PIN;
  HAL_GPIO_Init(GPIOB, &gpio);
  gpio.Pin = AD9954_CS_PIN | AD9954_PS1_PIN |
             AD9954_OSK_PIN | AD9954_PS0_PIN;
  HAL_GPIO_Init(GPIOC, &gpio);
  gpio.Pin = AD9954_SCLK_PIN;
  HAL_GPIO_Init(AD9954_SCLK_PORT, &gpio);
  gpio.Pin = AD9954_UPD_PIN | AD9954_SDIO_PIN;
  HAL_GPIO_Init(GPIOG, &gpio);

  gpio.Pin = AD9954_SDO_PIN;
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(AD9954_SDO_PORT, &gpio);
}

static void Ad9954_Reset(void)
{
  HAL_GPIO_WritePin(AD9954_RES_PORT, AD9954_RES_PIN, GPIO_PIN_SET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(AD9954_RES_PORT, AD9954_RES_PIN, GPIO_PIN_RESET);
  HAL_Delay(1U);
}

static void Ad9954_Update(void)
{
  HAL_GPIO_WritePin(AD9954_UPD_PORT, AD9954_UPD_PIN, GPIO_PIN_SET);
  for (volatile uint32_t delay = 0U; delay < 32U; ++delay)
  {
    __NOP();
  }
  HAL_GPIO_WritePin(AD9954_UPD_PORT, AD9954_UPD_PIN, GPIO_PIN_RESET);
  Ad9954_SerialDelay();
}

static void Ad9954_SerialDelay(void)
{
  for (volatile uint32_t delay = 0U; delay < 8U; ++delay)
  {
    __NOP();
  }
}

static void Ad9954_WriteByte(uint8_t data)
{
  for (uint8_t i = 0U; i < 8U; ++i)
  {
    HAL_GPIO_WritePin(AD9954_SCLK_PORT, AD9954_SCLK_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AD9954_SDIO_PORT, AD9954_SDIO_PIN,
                      ((data & 0x80U) != 0U) ?
                      GPIO_PIN_SET : GPIO_PIN_RESET);
    Ad9954_SerialDelay();
    HAL_GPIO_WritePin(AD9954_SCLK_PORT, AD9954_SCLK_PIN, GPIO_PIN_SET);
    Ad9954_SerialDelay();
    data <<= 1U;
  }

  HAL_GPIO_WritePin(AD9954_SCLK_PORT, AD9954_SCLK_PIN, GPIO_PIN_RESET);
}

static void Ad9954_WriteRegister(uint8_t address,
                                 const uint8_t *data,
                                 uint8_t length)
{
  HAL_GPIO_WritePin(AD9954_CS_PORT, AD9954_CS_PIN, GPIO_PIN_RESET);
  Ad9954_WriteByte(address);

  for (uint8_t i = 0U; i < length; ++i)
  {
    Ad9954_WriteByte(data[i]);
  }

  HAL_GPIO_WritePin(AD9954_CS_PORT, AD9954_CS_PIN, GPIO_PIN_SET);
}
