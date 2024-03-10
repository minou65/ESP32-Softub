// 
// 
// 

#include "webhandling.h"

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
void handleHeating(String value_);
void handleFavIcon();
void convertParams();

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

NTPConfig ntpConfig = NTPConfig();

TubeScheduler TubeScheduler5 = TubeScheduler();
TubeScheduler TubeScheduler4 = TubeScheduler(TubeScheduler5);
TubeScheduler TubeScheduler3 = TubeScheduler(TubeScheduler4);
TubeScheduler TubeScheduler2 = TubeScheduler(TubeScheduler3);
TubeScheduler TubeScheduler1 = TubeScheduler(TubeScheduler2);

void wifiInit() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("starting up...");

    iotWebConf.setStatusPin(STATUS_PIN, ON_LEVEL);
    iotWebConf.setConfigPin(CONFIG_PIN);

    iotWebConf.addParameterGroup(&ntpConfig);

    iotWebConf.addParameterGroup(&TubeScheduler1);
    iotWebConf.addParameterGroup(&TubeScheduler2);
    iotWebConf.addParameterGroup(&TubeScheduler3);
    iotWebConf.addParameterGroup(&TubeScheduler4);
    iotWebConf.addParameterGroup(&TubeScheduler5);

    iotWebConf.setConfigSavedCallback(&configSaved);
    iotWebConf.setWifiConnectionCallback(&wifiConnected);

    iotWebConf.getApTimeoutParameter()->visible = true;

    // -- Set up required URL handlers on the web server.
    server.on("/", handleRoot);
    server.on("/heating", HTTP_GET, []() { handleHeating(server.arg("turn")); });
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

void handleHeating(String value_) {
    if (value_ == "on") {

    }
    else if (value_ == "off") {

    }
    else {

    }
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