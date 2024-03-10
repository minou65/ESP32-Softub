// ntp.h

#ifndef _NTP_h
#define _NTP_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include "ESP32Time.h"

void NTPInit();
void NTPloop();

#endif


