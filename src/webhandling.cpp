// 
// 
// 

#include "webhandling.h"
#include "common.h"

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

TubScheduler TubScheduler5 = TubScheduler();
TubScheduler TubScheduler4 = TubScheduler(TubScheduler5);
TubScheduler TubScheduler3 = TubScheduler(TubScheduler4);
TubScheduler TubScheduler2 = TubScheduler(TubScheduler3);
TubScheduler TubScheduler1 = TubScheduler(TubScheduler2);

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");

    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    iotWebConf.addParameterGroup(&tubTemperatur);

    iotWebConf.addParameterGroup(&TubScheduler1);
    iotWebConf.addParameterGroup(&TubScheduler2);
    iotWebConf.addParameterGroup(&TubScheduler3);
    iotWebConf.addParameterGroup(&TubScheduler4);
    iotWebConf.addParameterGroup(&TubScheduler5);

    iotWebConf.addParameterGroup(&ntpConfig);

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

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
    // -- Let IotWebConf test and handle captive portal requests.
    if (iotWebConf.handleCaptivePortal()){
        return;
    }
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
    }

    _message += "</BODY>\n";

    // Redirect to the root page
    server.sendHeader("Location", "/", false);
    server.send(303, "text/html; charset=UTF-8", _message);
}

void handleReboot() {
    String _message;
    _message += "Rebooting in 5 seconds...\n";
    server.send(200, "text/plain; charset=UTF-8", _message);
    delay(5 * 1000);
    ESP.restart();
}

void handleFavIcon() {

}

void wifiLoop() {
    iotWebConf.doLoop();
    ArduinoOTA.handle();
};

void convertParams() {
}

void configSaved() {
    convertParams();
}