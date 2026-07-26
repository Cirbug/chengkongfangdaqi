#ifndef AD9954_H
#define AD9954_H

#include "main.h"

#define AD9954_REFERENCE_CLOCK_HZ    20000000UL
#define AD9954_PLL_MULTIPLIER        20U
#define AD9954_MAX_AMPLITUDE_SCALE   0x3FFFU
#define AD9954_FULL_SCALE_MVPP       500U

uint8_t Ad9954_Init(void);
uint8_t Ad9954_SetFrequency(uint32_t frequency_hz);
uint8_t Ad9954_SetAmplitude(uint16_t amplitude_scale);
uint8_t Ad9954_SetAmplitudeMv(uint16_t amplitude_mvpp);
uint8_t Ad9954_SetPhase(uint16_t phase_word);
uint8_t Ad9954_SetOutputMv(uint32_t frequency_hz, uint16_t amplitude_mvpp);

#endif /* AD9954_H */
