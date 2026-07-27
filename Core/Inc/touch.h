#ifndef TOUCH_H
#define TOUCH_H

#include "main.h"

typedef struct
{
  uint8_t pressed;
  uint16_t x;
  uint16_t y;
  uint16_t raw_x;
  uint16_t raw_y;
} TouchState;

void Touch_Init(void);
uint8_t Touch_Scan(TouchState *state);
uint8_t Touch_IsPressed(void);

#endif /* TOUCH_H */
