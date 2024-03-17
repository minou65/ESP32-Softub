// 
// 
// 

#include "webhandling.h"
#include "favicon.h"
#include "ntp.h"

#define HTML_Start_Doc "<!DOCTYPE html>\
    <html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>\
    <title>{v}</title>\
";

#define HTML_Start_Body "</head><body>";
#define HTML_Start_Fieldset "<fieldset align=left style=\"border: 1px solid\">";
#define HTML_Start_Table "<table border=0 align=center>";
#define HTML_End_Table "</table>";
#define HTML_End_Fieldset "</fieldset>";
#define HTML_End_Body "</body>";
#define HTML_End_Doc "</html>";
#define HTML_Fieldset_Legend "<legend>{l}</legend>"
#define HTML_Table_Row "<tr><td>{n}</td><td>{v}</td></tr>";

const char IOTWEBCONF_HTML_FORM_InputElements_JAVASCRIPT[] PROGMEM =
"function toggleTemperatureFields() {\n"
"    const checkbox = document.getElementById('tubtemperatur-checkbox');\n"
"    const celsiusField = document.getElementsByClassName('tubtemperatur-presetCelsius')[0];\n"
"    const fahrenheitField = document.getElementsByClassName('tubtemperatur-presetFahrenheit')[0];\n"
"\n"
"    if (checkbox.checked) {\n"
"        // Show Celsius input field\n"
"        celsiusField.style.display = 'block';\n"
"        fahrenheitField.style.display = 'none';\n"
"    }\n"
"    else {\n"
"        // Show Fahrenheit input field\n"
"        celsiusField.style.display = 'none';\n"
"        fahrenheitField.style.display = 'block';\n"
"    }\n"
"}\n";

class CustomHtmlFormatProvider : public iotwebconf::OptionalGroupHtmlFormatProvider {
protected:
    String getScriptInner() override {
        return
            HtmlFormatProvider::getScriptInner() +
            String(FPSTR(IOTWEBCONF_HTML_FORM_OPTIONAL_GROUP_JAVASCRIPT)) +
            String(FPSTR(IOTWEBCONF_HTML_FORM_InputElements_JAVASCRIPT));
    }
};

CustomHtmlFormatProvider customHtmlFormatProvider;



DNSServer dnsServer;
WebServer server(80);

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN
#if ESP32 
#define ON_LEVEL HIGH
#else
#define ON_LEVEL LOW
#endif

// -- Callback methods.
void configSaved();
void wifiConnected();

// -- Method declarations.
void handleRoot();
void handleSet();
void handleReboot();
void handleFavIcon();
void convertParams();

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

NTPConfig ntpConfig = NTPConfig();

TubTemperatur tubTemperatur = TubTemperatur();

TubScheduler TubScheduler1 = TubScheduler("Scheduler 1");
TubScheduler TubScheduler2 = TubScheduler("Scheduler 2");
TubScheduler TubScheduler3 = TubScheduler("Scheduler 3");
TubScheduler TubScheduler4 = TubScheduler("Scheduler 4");
TubScheduler TubScheduler5 = TubScheduler("Scheduler 5");

TubLight tubLight = TubLight("Light 1");


void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");

    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);
    iotWebConf.setHtmlFormatProvider(&customHtmlFormatProvider);

    iotWebConf.addParameterGroup(&tubTemperatur);

    iotWebConf.addParameterGroup(&tubLight);

    TubScheduler1.setNext(&TubScheduler2);
    TubScheduler2.setNext(&TubScheduler3);
    TubScheduler3.setNext(&TubScheduler4);
    TubScheduler4.setNext(&TubScheduler5);


    iotWebConf.addParameterGroup(&TubScheduler1);
    iotWebConf.addParameterGroup(&TubScheduler2);
    iotWebConf.addParameterGroup(&TubScheduler3);
    iotWebConf.addParameterGroup(&TubScheduler4);
    iotWebConf.addParameterGroup(&TubScheduler5);

    iotWebConf.addParameterGroup(&ntpConfig);

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    // -- Initializing the configuration.
    iotWebConf.init();

    convertParams();

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/set", HTTP_GET, []() { handleSet(); });
    server.on("/reboot", HTTP_GET, []() { handleReboot(); });
    server.on("/favicon.ico", []() { handleFavIcon(); });
    server.on("/config", [] { iotWebConf.handleConfig(); });
    server.onNotFound([]() { iotWebConf.handleNotFound(); });

    Serial.println("Ready.");
};

void wifiConnected() {
    ArduinoOTA.begin();
}

void handleRoot() {
    ESP32Time rtc;

    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()){
        return;
    }

    String _page = HTML_Start_Doc;
    _page.replace("{v}", "Softub");
    _page += "<style>";
    _page += ".de{background-color:#ffaaaa;} .em{font-size:0.8em;color:#bb0000;padding-bottom:0px;} .c{text-align: center;} div,input,select{padding:5px;font-size:1em;} input{width:95%;} select{width:100%} input[type=checkbox]{width:auto;scale:1.5;margin:10px;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#16A1E7;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} fieldset{border-radius:0.3rem;margin: 0px;}";
    // _page.replace("center", "left");
    _page += ".dot-grey{height: 12px; width: 12px; background-color: #bbb; border-radius: 50%; display: inline-block; }";
    _page += ".dot-green{height: 12px; width: 12px; background-color: green; border-radius: 50%; display: inline-block; }";
    _page += ".blink-green{2s blink-green ease infinite; height: 12px; width: 12px; background-color: orange; border-radius: 50%; display: inline-block; }";

    _page += "</style>";

    _page += "<meta http-equiv=refresh content=30 />";
    _page += HTML_Start_Body;
    _page += "<table border=0 align=center>";
    _page += "<tr><td>";

    _page += HTML_Start_Fieldset;
    _page += HTML_Fieldset_Legend;
    _page.replace("{l}", "Status");
    _page += HTML_Start_Table;

    if (tubTemperatur.InCelsius()) {
        _page += "<tr><td align=left>Desired temperatur:</td><td>" + String(tubTemperatur.Preset()) + "&deg;C" + "</td></tr>";
		double _last_valid_temp = fahrenheitToCelsius(last_valid_temp);
        _page += "<tr><td align=left>Last valid temperatur:</td><td>" + String(_last_valid_temp) + "&deg;C" + "</td></tr>";
	}
	else {
		_page += "<tr><td align=left>Preset temperatur:</td><td>" + String(tubTemperatur.Preset()) + "&deg;F" + "</td></tr>";
        _page += "<tr><td align=left>last valid temperatur:</td><td>" + String(last_valid_temp) + "&deg;F" + "</td></tr>";
	}

     _page += "<tr><td align=left>Pump:</td><td>" + String(state_name(runstate)) + "</td></tr>";
     _page += "<tr><td align=left>Light:</td><td>" + String(light_statename(light_state)) + "</td></tr>";


    _page += HTML_End_Table;
    _page += HTML_End_Fieldset;

    _page += HTML_Start_Fieldset;
    _page += HTML_Fieldset_Legend;
    _page.replace("{l}", "Scheduler");
    _page += HTML_Start_Table;

    TubScheduler* _scheduler = &TubScheduler1;
    while (_scheduler != nullptr) {
        if (_scheduler->isActive()) {
			_page += "<tr><td align=left>" + _scheduler->Name() + ":</td><td>" + _scheduler->On() + " - " + _scheduler->Off() + "</td></tr>";
		}
        _scheduler = (TubScheduler*)_scheduler->getNext();
	}

    _page += HTML_End_Table;
    _page += HTML_End_Fieldset;

    _page += HTML_Start_Fieldset;
    _page += HTML_Fieldset_Legend;
    _page.replace("{l}", "Buttons");
    _page += HTML_Start_Table;

    // add a button to toggle the jets
    _page += "<tr><td><form action='/set' method='get'><button type='submit' name='jets' value='toggle'>Jets</button></form></td></tr>";

    // Add a button to toggle the light.
    _page += "<tr><td><form action='/set' method='get'><button type='submit' name='light' value='toggle'>Light</button></form></td></tr>";

    // Add a button to increment the temperature.
    _page += "<tr><td><form action='/set' method='get'><button type='submit' name='temp' value='up'>Temp +</button></form></td></tr>";

    // Add a button to decrement the temperature.
    _page += "<tr><td><form action='/set' method='get'><button type='submit' name='temp' value='down'>Temp -</button></form></td></tr>";

    // Add a button to trigger a reboot. 
    _page += "<tr><td><form action='/reboot' method='get'><button type='submit'>Reboot</button></form></td></tr>";
    
    _page += HTML_End_Table;
    _page += HTML_End_Fieldset;

    _page += HTML_Start_Fieldset;
    _page += HTML_Fieldset_Legend;
    _page.replace("{l}", "Network");
    _page += HTML_Start_Table;

    _page += "<tr><td align=left>MAC Address:</td><td>" + String(WiFi.macAddress()) + "</td></tr>";
    _page += "<tr><td align=left>IP Address:</td><td>" + String(WiFi.localIP().toString().c_str()) + "</td></tr>";
    
    _page += HTML_End_Table;
    _page += HTML_End_Fieldset;

    _page += "<br>";
    _page += "<br>";

    _page += HTML_Start_Table;
    _page += "<tr><td align=left>Time:</td><td>" + rtc.getDateTime() + "</td></tr>";
    _page += "<tr><td align=left><a href = 'config'>Configuration page</a> </td></tr>";
    _page += "<tr><td align=left><font size = 1>Version: " + String(Version) + "</font></td></tr>";
    // page += "<tr><td align=left>Go to <a href='setruntime'>runtime modification page</a> to change runtime data.</td></tr>";
    _page += HTML_End_Table;
    _page += HTML_End_Body;

    _page += HTML_End_Doc;


    server.send(200, "text/html", _page);
};


void handleSet() {
    String _message;
    // Redirect to the root page when we're done here.
    // message += "<HEAD><meta http-equiv=\"refresh\" content=\"0;url=/\"></HEAD>\n<BODY>\n";
    _message += "<HEAD></HEAD>\n<BODY>\n";

    if (server.hasArg("jets")) {
        String _jets = server.arg("jets");

        bool accepted = false;
        switch (runstate) {
        case runstate_idle:
            if (_jets.equals("on") || _jets.equals("toggle")) {
                accepted = true;
                _message += "Turning on manual pump<br>\n";
                enter_state(runstate_manual_pump);
            }
            break;
        case runstate_finding_temp:
        case runstate_manual_pump:
        case runstate_heating:
            if (_jets.equals("off") || _jets.equals("toggle")) {
                accepted = true;
               _message += "Turning off manual pump<br>\n";
                enter_state(runstate_idle);
            }
            break;
        default:
            break;
        }

        if (!accepted)
        {
            _message += "Jets command \"";
            _message += _jets;
            _message += "\" ignored in run state ";
            _message += state_name(runstate);
            _message += "<br>\n";
        }
    }

    if (server.hasArg("temp")) {
        String _tempString = server.arg("temp");

        int _temp = _tempString.toInt();
        // If the conversion fails, temp will be 0. This is handled by the range check below.
        if (_tempString.equals("up")) {
            temp_adjust(1);
            _message += "Temp incremented to ";
            _message += temp_setting;
            _message += "<br>\n";
        }
        else if (_tempString.equals("down")) {
            temp_adjust(-1);
            _message += "Temp decremented to ";
            _message += temp_setting;
            _message += "<br>\n";
        }
        else if ((_temp >= temp_min) && (_temp <= temp_max)) {
            temp_setting = _temp;
            // This handles updating the last-temp-adjust time, etc.
            temp_adjust(0);
            _message += "Temp set to ";
            _message += temp_setting;
            _message += "<br>\n";
        }
        else {
            _message += "Temp arg out of range: ";
            _message += _tempString;
            _message += "<br>\n";
        }
    }

    if (server.hasArg("light")) {
		String _light = server.arg("light");
        
        switch(light_state) {
			case light_off:
				if (_light.equals("on") || _light.equals("toggle")) {
					_message += "Turning on light<br>\n";
					light_state = light_on;
				}
				break;
			case light_on:
				if (_light.equals("off") || _light.equals("toggle")) {
					_message += "Turning off light<br>\n";
					light_state = light_off;
				}
				break;
			default:
				_message += "Light command \"";
				_message += _light;
				_message += "\" ignored<br>\n";
				break;
		}
    }

    _message += "</BODY>\n";

    // Redirect to the root page
    server.sendHeader("Location", "/", false);
    server.send(303, "text/html; charset=UTF-8", _message);
}

void handleReboot() {
    String _message;

    // redirect to the root page after 15 seconds
    _message += "<HEAD><meta http-equiv=\"refresh\" content=\"15;url=/\"></HEAD>\n<BODY><p>\n";
	_message += "Rebooting...<br>\n";
    _message += "Redirected after 15 seconds...\n";
    _message += "</p></BODY>\n";

    server.send(200, "text/html", _message);
    ESP.restart();
}

void handleFavIcon() {
    server.send_P(200, "image/x-icon", whirlpool_16x16_bits, sizeof(whirlpool_16x16_bits));
}

void wifiLoop() {
    iotWebConf.doLoop();
    ArduinoOTA.handle();

    //if (gSaveParams) {} {
    //    tubTemperatur.SetTemperature(temp_setting);
    //    iotWebConf.saveConfig();
    //    gSaveParams = false;
    //}

};

void convertParams() {
    RGB = tubLight.Color();
    glighttimer_Minutes = tubLight.Delay();
}

void configSaved() {
    convertParams();
    
}