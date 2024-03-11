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

#include <IotWebConf.h>
#include <IotWebConfOptionalGroup.h>

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "123456789";

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "MySoftube";

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "A1"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN  -1

#define STRING_LEN 50
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
	TubScheduler(ChainedParameterGroup* _nextGroup = nullptr) : ChainedParameterGroup("tubescheduler", "Scheduler") {
		snprintf(TimeOnId, STRING_LEN, "%s-on", this->getId());
		snprintf(TimeOffId, STRING_LEN, "%s-off", this->getId());

		this->addItem(&this->TimeOnParam);
		this->addItem(&this->TimeOffParam);
		this->setNext(_nextGroup);
	}

	String On() { return String(TimeOnValue); };
	String Off() { return String(TimeOffValue); };

private:
	iotwebconf::TimeParameter TimeOnParam = iotwebconf::TimeParameter("On", TimeOnId, TimeOnValue, STRING_LEN, "00:00");
	iotwebconf::TimeParameter TimeOffParam = iotwebconf::TimeParameter("Off", TimeOffId, TimeOffValue, STRING_LEN, "24:00");

	char TimeOnValue[STRING_LEN];
	char TimeOffValue[STRING_LEN];

	char TimeOnId[STRING_LEN];
	char TimeOffId[STRING_LEN];

};

class TubTemperatur : public iotwebconf::ParameterGroup {
public:
	TubTemperatur() : ParameterGroup("tubetemperatur", "Temperatur") {
		snprintf(TempInCelsiusId, STRING_LEN, "%s-checkbox", this->getId());
		snprintf(TempDesiredId, STRING_LEN, "%s-desired", this->getId());

		this->addItem(&this->TempInCelsiusParam);
		this->addItem(&this->TempDesiredParam);

	}

	bool InCelsius() { return TempInCelsiusParam.isChecked(); };
	String Desired() { return String(atoi(TempDesiredValue)); };

private:
	iotwebconf::CheckboxParameter TempInCelsiusParam = iotwebconf::CheckboxParameter("in &deg;Celsius", TempInCelsiusId, TempInCelsiusValue, STRING_LEN, true);
	iotwebconf::NumberParameter TempDesiredParam = iotwebconf::NumberParameter("Desired", TempDesiredId, TempDesiredValue, STRING_LEN, "35", "10...110", "min='10' max='110' step='1'");

	char TempInCelsiusValue[STRING_LEN];
	char TempInCelsiusId[STRING_LEN];

	char TempDesiredValue[STRING_LEN];
	char TempDesiredId[STRING_LEN];

};

extern NTPConfig ntpConfig;
extern TubTemperatur tubTemperatur;
extern TubScheduler TubScheduler1;
extern TubScheduler TubScheduler2;
extern TubScheduler TubScheduler3;
extern TubScheduler TubScheduler4;
extern TubScheduler TubScheduler5;


#endif

