// webhandling.h

#ifndef _WEBHANDLING_h
#define _WEBHANDLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#if ESP32
#include <WiFi.h>
#include <ArduinoOTA.h>
#else
#include <ESP8266WiFi.h>      
#endif


#include <DNSServer.h>
#include<iostream>
#include <string.h>
#include "common.h"

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>
#include <IotWebConfTParameter.h>

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "ESP32-Softub";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

#define STRING_LEN 80
#define NUMBER_LEN 6

static WiFiClient wifiClient;
extern IotWebConf iotWebConf;

extern void wifiInit();
extern void wifiLoop();

class NTPConfig : public iotwebconf::ParameterGroup {
public:
	NTPConfig() : ParameterGroup("ntpconfig", "NTP Config") {
		snprintf(UseNTPServerId, STRING_LEN, "%s-checkbox", this->getId());
		snprintf(NTPServerId, STRING_LEN, "%s-server", this->getId());
		snprintf(TimeZoneId, STRING_LEN, "%s-timezone", this->getId());

		this->addItem(&this->UseNTPServerParam);
		this->addItem(&this->NTPServerParam);
		this->addItem(&this->TimeZoneParam);
	}

	bool UseNTPServer() { return UseNTPServerParam.isChecked(); };
	String NTPServerAddress() { return String(NTPServerValue); };
	String PosixTimeZone() { return String(TimeZoneValue); };


private:
	iotwebconf::CheckboxParameter UseNTPServerParam = iotwebconf::CheckboxParameter("Use NTP server", UseNTPServerId, UseNTPServerValue, STRING_LEN, true);
	iotwebconf::TextParameter NTPServerParam = iotwebconf::TextParameter("NTP server (FQDN or IP address)", NTPServerId, NTPServerValue, STRING_LEN, "pool.ntp.org");
	iotwebconf::TextParameter TimeZoneParam = iotwebconf::TextParameter("POSIX timezones string", TimeZoneId, TimeZoneValue, STRING_LEN, "CET-1CEST,M3.5.0,M10.5.0/3");

	char UseNTPServerValue[STRING_LEN];
	char NTPServerValue[STRING_LEN];
	char TimeZoneValue[STRING_LEN];

	char UseNTPServerId[STRING_LEN];
	char NTPServerId[STRING_LEN];
	char TimeZoneId[STRING_LEN];
};

class TubScheduler : public iotwebconf::ChainedParameterGroup {
public:
	TubScheduler(const char* id) : ChainedParameterGroup(id, "Scheduler") {
		snprintf(TimeOnId, STRING_LEN, "%s-on", this->getId());
		snprintf(TimeOffId, STRING_LEN, "%s-off", this->getId());

		this->addItem(&this->TimeOnParam);
		this->addItem(&this->TimeOffParam);
	}

	String On() { return String(TimeOnValue); };
	String Off() { return String(TimeOffValue); };
	String Name() { return this->getId(); };

private:
	iotwebconf::TimeParameter TimeOnParam = iotwebconf::TimeParameter("On", TimeOnId, TimeOnValue, STRING_LEN, "00:00");
	iotwebconf::TimeParameter TimeOffParam = iotwebconf::TimeParameter("Off", TimeOffId, TimeOffValue, STRING_LEN, "23:59");

	char TimeOnValue[STRING_LEN];
	char TimeOffValue[STRING_LEN];

	char TimeOnId[STRING_LEN];
	char TimeOffId[STRING_LEN];

};

class MyCheckboxParameter : public iotwebconf::CheckboxParameter {
public:
	MyCheckboxParameter(
		const char* label, 
		const char* id, 
		char* valueBuffer, 
		int length,
		bool defaultValue = false, 
		const char* customHtml = nullptr 
	) : iotwebconf::CheckboxParameter(label, id, valueBuffer, length, defaultValue) {
		this->customHtml = customHtml;
	};

	String renderHtml(bool dataArrived, bool hasValueFromPost, String valueFromPost){
		bool checkSelected = false;
		if (dataArrived)
		{
			if (hasValueFromPost && valueFromPost.equals("selected"))
			{
				checkSelected = true;
			}
		}
		else
		{
			if (this->isChecked())
			{
				checkSelected = true;
			}
		}

		if (checkSelected)
		{
			this->customHtml = "checked='checked' onchange='toggleTemperatureFields()'";
		}
		else
		{
			this->customHtml = "onchange='toggleTemperatureFields()'";
		}


		return TextParameter::renderHtml("checkbox", true, "selected");
	}

private:

};

class TubTemperatur : public iotwebconf::ParameterGroup {
public:
	TubTemperatur() : ParameterGroup("tubtemperatur", "Temperatur") {
		snprintf(TempInCelsiusId, STRING_LEN, "%s-checkbox", this->getId());
		snprintf(TempPresetId, STRING_LEN, "%s-presetFahrenheit", this->getId());
		snprintf(TempPresetCelsiusId, STRING_LEN, "%s-presetCelsius", this->getId());

		this->addItem(&this->TempInCelsiusParam);
		this->addItem(&this->TempPresetFahrenheitParam);
		this->addItem(&this->TempPresetCelsiusParam);
	}

	bool InCelsius() { return TempInCelsiusParam.isChecked(); };

	double Preset() { 
		if (InCelsius()) {
			return atoi(TempPresetCelsiusdValue);
		}
		else
		{
			return atoi(TempPresetFahrenheitValue);
		}
	};

	void SetFahrenheit(double temp_) { 
		snprintf(TempPresetFahrenheitValue, STRING_LEN, "%d", temp_);
		snprintf(TempPresetCelsiusdValue, STRING_LEN, "%d", (temp_ - 32) * 5 / 9);
	};

private:
	MyCheckboxParameter TempInCelsiusParam = MyCheckboxParameter("in &deg;Celsius", TempInCelsiusId, TempInCelsiusValue, STRING_LEN, true, "onchange='toggleTemperatureFields()'");
	iotwebconf::NumberParameter TempPresetFahrenheitParam = iotwebconf::NumberParameter("Preset &deg;F", TempPresetId, TempPresetFahrenheitValue, STRING_LEN, "100", "68...104", "min='68' max='104' step='1'");
	iotwebconf::NumberParameter TempPresetCelsiusParam = iotwebconf::NumberParameter("Preset &deg;C", TempPresetCelsiusId, TempPresetCelsiusdValue, STRING_LEN, "38", "20...40", "min='20' max='40' step='1'");

	char TempInCelsiusValue[STRING_LEN];
	char TempInCelsiusId[STRING_LEN];

	char TempPresetFahrenheitValue[STRING_LEN];
	char TempPresetId[STRING_LEN];

	char TempPresetCelsiusdValue[STRING_LEN];
	char TempPresetCelsiusId[STRING_LEN];

	String getEndTemplate() override {

		String result = "<script>toggleTemperatureFields();</script>\n";
		result += ParameterGroup::getEndTemplate();

		return result;
	};

};

class TubLight : public iotwebconf::ParameterGroup {
public:
	TubLight(const char* id) : ParameterGroup(id, "Light") {
		snprintf(LightColorId, STRING_LEN, "%s-color", this->getId());
		snprintf(DelayId, STRING_LEN, "%s-delay", this->getId());

		this->addItem(&this->LightColorParam);
		this->addItem(&this->DelayParam);
	}

	uint32_t Delay() { 
		return atoi(DelayValue) * 60; 
	};

	RGBStruct Color() { 
		RGBStruct rgb;
		rgb.r = (atoi(LightColorParam.value()) >> 16) & 0xFF;
		rgb.g = (atoi(LightColorParam.value()) >> 8) & 0xFF;
		rgb.b = atoi(LightColorParam.value()) & 0xFF;
		return rgb;
	};

private:
	iotwebconf::ColorTParameter LightColorParam = iotwebconf::ColorTParameter(LightColorId, "Color", "FFFFFF");
	iotwebconf::NumberParameter DelayParam = iotwebconf::NumberParameter("Light off delay (minutes)", DelayId, DelayValue, NUMBER_LEN, "15", "0..60", "min='0' max='60' step='1'");

	char LightColorId[STRING_LEN];
	char DelayId[STRING_LEN];

	char DelayValue[STRING_LEN];
};

extern NTPConfig ntpConfig;
extern TubTemperatur tubTemperatur;
extern TubScheduler TubScheduler1;
extern TubScheduler TubScheduler2;
extern TubScheduler TubScheduler3;
extern TubScheduler TubScheduler4;
extern TubScheduler TubScheduler5;
extern TubLight tubLight;

#endif

