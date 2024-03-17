// 
// 
// 

#include "Light.h"
#include "common.h"
#include "pins.h"
#include "neotimer.h"

#define channel_red 0
#define channel_green 1
#define channel_blue 2

#define freq 5000
#define resolution 8

int light_state = light_off;
uint16_t glighttimer_Minutes = 900;

RGBStruct RGB = { 0, 0, 0 };
Neotimer light_timer = Neotimer(glighttimer_Minutes * 1000);

const char* light_statename(int state) {
	switch (state) {
		case light_off: return "off";
		case light_on: return "on";
		case light_blink: return "blink";
		default: return "unknown";
	}
}

void setColor(RGBStruct color_) {
	ledcWrite(channel_red, color_.r);
	ledcWrite(channel_green, color_.g);
	ledcWrite(channel_blue, color_.b);
}

void initLight() {
	Serial.println("initLight...");

	ledcSetup(channel_red, freq, resolution);
	ledcSetup(channel_green, freq, resolution);
	ledcSetup(channel_blue, freq, resolution);

	ledcAttachPin(pin_red, channel_red);
	ledcAttachPin(pin_green, channel_green);
	ledcAttachPin(pin_blue, channel_blue);

	setColor(RGBStruct{ 0, 0, 0 });

	light_state = light_off;
	light_timer.start();

}

void loopLight() {
	if (gParamsChanged) {
		light_timer.start(glighttimer_Minutes * 1000);
		if (light_state == light_on) {
			setColor(RGB);
		}
	}

	switch(light_state) {
		case light_off:
			setColor(RGBStruct{ 0, 0, 0 });
			light_timer.reset();
			break;

		case light_on:
			setColor(RGB);
			break;

		case light_blink:
			break;

		default:
			break;
	}
}

