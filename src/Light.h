// Light.h

#ifndef _LIGHT_h
#define _LIGHT_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

void initLight();
void loopLight();

#endif
