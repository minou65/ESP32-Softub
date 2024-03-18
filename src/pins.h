// pins.h

#ifndef _PINS_h
#define _PINS_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#define pin_pump GPIO_NUM_2
const int pin_temp[] = { GPIO_NUM_32, GPIO_NUM_33 };

#define pin_red GPIO_NUM_12
#define pin_green GPIO_NUM_13
#define pin_blue GPIO_NUM_14

#define PANEL_TX GPIO_NUM_8
#define PANEL_RX GPIO_NUM_7

#endif

