// common.h

#ifndef _COMMON_h
#define _COMMON_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

// run states
enum {
    // Just started up.
    runstate_startup,
    // Not running, displaying setpoint temperature.
    runstate_idle,
    // Pump has just started running, we don't have confidence in the temperature reading yet
    runstate_finding_temp,
    // Temperature is known to be low, we're trying to increase it.
    runstate_heating,
    // The pump was turned on manually. Run it for 10 minutes.
    runstate_manual_pump,

    // for testing
    runstate_test,

    // Go to this state if we detect a problem with the temperature sensors. 
    // It shuts down the pump and sets the display into a "help me" state.
    // The user can reset to startup mode by holding the "light" and "jets" buttons for 5 seconds.
    runstate_panic
};

extern int runstate;
extern void enter_state(int state);
extern const char* state_name(int state);

extern const int temp_min;
extern const int temp_max;
extern bool tempInCelsius;
extern int temp_setting;
extern void temp_adjust(int amount);
#endif

