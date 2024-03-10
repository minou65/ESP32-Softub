#include "common.h"
#include "webhandling.h"
#include <Arduino.h>


//////////////////////////////////////////////////
// Feature defines
// #define SERIAL_DEBUG 1


#include <esp_task_wdt.h>
#include "pins.h"
#include "ntp.h"


// ESP32 has 12 bit ADCs
const int ADC_RESOLUTION = 4096;

// Ref: https://esp32.com/viewtopic.php?t=1053
// The ADC measures from 0 to 1.1v, moderated by the attenuation factor.
// The voltage attenuation defaults to ADC_11db, which scales it by a factor of 1/3.6.
// const double ADC_AREF_VOLTAGE = 1.1 * 3.6;

// Reducing the attenuation factor a bit gives us more usable resolution.
// ADC_0db provides no attenuation, so we measure 0 - 1.1v
// ADC_2_5db provides attenuation of 1/1.34, so we measure 0 - 1.47v
// ADC_6db provides an attenuation of 1/2 , so we measure 0 - 2.2v
// ADC_11db provides an attenuation of 1/3.6, so we measure 0 - 3.96v

// From comments in the adc.h header: 
// Due to ADC characteristics, most accurate results are obtained within the following approximate voltage ranges:
// - 0dB attenuaton (ADC_ATTEN_DB_0) between 100 and 950mV
// - 2.5dB attenuation (ADC_ATTEN_DB_2_5) between 100 and 1250mV
// - 6dB attenuation (ADC_ATTEN_DB_6) between 150 to 1750mV
// - 11dB attenuation (ADC_ATTEN_DB_11) between 150 to 2450mV

// I don't want to use ADC_0db, as this is only accurage to 0.95v and maxes out at 1.1 = 110 degrees F, which we could see.
// It's possible that the temperature sensor in the pod would approach 125 degrees, which is the edge of the accurate range for 2.5dB.
// 6dB should be accurate up to 1.75v = 175 degrees F, which should give us ample headroom.
#define ADC_ATTENUATION_FACTOR ADC_6db
const double ADC_AREF_VOLTAGE = 1.1 * 2.0;

const double ADC_DIVISOR = ADC_AREF_VOLTAGE / double(ADC_RESOLUTION);

#ifdef SERIAL_DEBUG
#ifndef SERIAL_DEBUG_SPEED
#define SERIAL_DEBUG_SPEED 9600
#endif

void debug(const char* format, ...)
{
    char string[256];
    va_list arg;
    va_start(arg, format);
    vsnprintf(string, sizeof(string), format, arg);

    Serial.println(string);
}
void debug(const String& string)
{
    Serial.println(string);
}
void serial_debug_init()
{
    // delay(3000);
    Serial.begin(SERIAL_DEBUG_SPEED);
    // while (!Serial);

    debug("Serial initialized");
    // delay(100);
}
#else
#define serial_debug_init(...)
#define debug(...)
#endif

int runstate = runstate_startup;

// Min and max allowed setpoints
const int temp_min = 50;
const int temp_max = 110;

// The time of the last transition. This may be used differently by different states.
uint32_t runstate_last_transition_millis = 0;

// The time of the last change to buttons pressed. This may be used differently by different states.
uint32_t buttons_last_transition_millis = 0;

// The last temperature reading we took
double last_temp = 0;
// The last valid temperature reading we took
double last_valid_temp = 0;

// Used to display temperature for a short time after the user adjusts it
bool temp_adjusted = false;
uint32_t temp_adjusted_millis = 0;
const uint32_t temp_adjusted_display_millis = 5 * 1000l;

// The amount of time to wait on startup before doing anything
const uint32_t startup_wait_seconds = 10;

// The amount of time we run the pump before believing the temperature reading
const uint32_t temp_settle_millis = 30 * 1000l; // 30 seconds

// The amount of time after stopping the pump when we no longer consider the temp valid.
const uint32_t temp_decay_millis = 60 * 1000l; // 60 seconds

// The amount of time in the idle state after which we should run to check the temperature.
const uint32_t idle_seconds = 15 * 60;  // 15 minutes

// When the user turns on the pump manually, run it for this long.
const uint32_t manual_pump_seconds = 5 * 60; // 5 minutes

// The amount of time the user has to hold buttons to escape panic state
const uint32_t panic_wait_seconds = 5;

// If smoothed readings ever disagree by this many degrees f, panic.
const int panic_sensor_difference = 10;

// If the temperature reading ever the max set temperature plus 5 degrees f, panic.
const int panic_high_temp = temp_max + 5;

// Used to flash things in panic mode
bool panic_flash;

String panic_string;

const int pin_temp_count = sizeof(pin_temp) / sizeof(pin_temp[0]);

// smooth the temperature sampling over this many samples, to filter out noise.
const int temp_sample_count = 128;
int temp_sample_pointer = 0;
int temp_samples[pin_temp_count][temp_sample_count];

// Start out the display bytes with a sane value.
uint8_t display_buffer[] = { 0x02, 0x00, 0x01, 0x00, 0x00, 0x01, 0xFF };
const int32_t display_bytes = sizeof(display_buffer) / sizeof(display_buffer[0]);
bool display_dirty = true;

// loop no faster than 16Hz
const uint32_t loop_microseconds = 1000000l / 16;

uint32_t buttons = 0;
uint32_t last_buttons = 0;

bool pump_running = false;
uint32_t pump_switch_millis = 0;
bool temp_valid = false;

int temp_setting = 100;
// stop heating at temp_setting + temp_setting_range, 
double temp_setting_range = 0.5;

enum {
    button_jets = 0x01,
    button_light = 0x02,
    button_up = 0x04,
    button_down = 0x08,
};

void watchdog_init() {
    // Start with the watchdog timer disabled
    esp_task_wdt_init(15, false);
}

void watchdog_start() {
    // Start the watchdog timer watching this task
    esp_task_wdt_init(15, true);
    esp_task_wdt_add(NULL);
}

void watchdog_reset() {
    esp_task_wdt_reset();
}

String dtostr(double number, signed char width = 1, unsigned char prec = 1)
{
    char buf[32];
    dtostrf(number, width, prec, buf);
    return String(buf);
}

void temp_adjust(int amount)
{
    temp_setting += amount;
    if (temp_setting > temp_max)
        temp_setting = temp_max;
    if (temp_setting < temp_min)
        temp_setting = temp_min;
    temp_adjusted = true;
    temp_adjusted_millis = millis();
}

double adc_to_farenheit(double reading)
{
    // Scale the reading to voltage
    reading *= ADC_DIVISOR;

    // The temperature sensors seem to be LM34s.
    // (Linear, 750mv at 75 degrees F, slope 10mv/degree F)
    // Basically, temperature in F is voltage * 100.
    return (reading * 100);
}

void runstate_transition()
{
    runstate_last_transition_millis = millis();
}

const char* state_name(int state)
{
    const char* name = "UNKNOWN";
    switch (state) {
    case runstate_startup:       name = "startup"; break;
    case runstate_idle:          name = "idle"; break;
    case runstate_finding_temp:  name = "finding temp"; break;
    case runstate_heating:       name = "heating"; break;
    case runstate_manual_pump:   name = "manual"; break;
    case runstate_test:          name = "test"; break;
    case runstate_panic:         name = "PANIC"; break;
    }
    return name;
}

void enter_state(int state)
{
    runstate = state;
    runstate_transition();
}

void panic()
{
    if (runstate != runstate_panic) {
        enter_state(runstate_panic);
    }
}

void panic(const char* format, ...)
{
    char string[1024];
    va_list arg;
    va_start(arg, format);
    vsnprintf(string, sizeof(string), format, arg);

    panic_string = String(string);
    panic();
}

// Optionally return the lowest/highest readings in the buffer
double smoothed_sensor_reading(int sensor, int* lowest = NULL, int* highest = NULL)
{
    if (sensor >= pin_temp_count) {
        sensor = 0;
    }
    if (lowest != NULL) {
        *lowest = INT_MAX;
    }
    if (highest != NULL) {
        *highest = INT_MIN;
    }

    double result = 0;
    for (int i = 0; i < temp_sample_count; i++) {
        int sample = temp_samples[sensor][i];
        result += sample;
        if (lowest != NULL) {
            *lowest = min(*lowest, sample);
        }
        if (highest != NULL) {
            *highest = max(*highest, sample);
        }
    }
    result /= temp_sample_count;

    return result;
}

void read_temp_sensors()
{
    // Advance the sample pointer in the ring buffer.
    temp_sample_pointer++;
    if (temp_sample_pointer >= temp_sample_count) {
        temp_sample_pointer = 0;
    }

    double avg_reading = 0;
    for (int i = 0; i < pin_temp_count; i++)
    {
        // Use the calibrated API to read actual voltage in millivolts
        uint32_t mv = analogReadMilliVolts(pin_temp[i]);
        // Convert it to our defined range.
        // The entire right-hand multiplier here is just constant values, so it should be computed at compile-time.
        int value = double(mv) * ((1.0 / ADC_AREF_VOLTAGE) * (ADC_RESOLUTION / 1000.0));

        // Save the current sample in the ring buffer
        temp_samples[i][temp_sample_pointer] = value;

        // Smooth the temperature sampling over temp_sample_count samples
        double smoothed_value = smoothed_sensor_reading(i);

        // The first two sensors are the water temperature sensors. Any further ones are auxiliary.
        if (i < 2) {
            avg_reading += smoothed_value;
        }
    }

    // Calculate the average smoothed temp of the first two sensors and save it as the water temp.
    avg_reading /= 2;
    last_temp = adc_to_farenheit(avg_reading);

    // If the smoothed readings from sensors 0 and 1 ever differ by more than panic_sensor_difference degrees, panic.
    if (fabs(adc_to_farenheit(smoothed_sensor_reading(0)) - adc_to_farenheit(smoothed_sensor_reading(1))) > panic_sensor_difference)
    {
        panic("sensor readings diverged (%s vs %s)",
            dtostr(adc_to_farenheit(smoothed_sensor_reading(0))).c_str(),
            dtostr(adc_to_farenheit(smoothed_sensor_reading(1))).c_str());
    }

    // If calculated temperature is over our defined limit, panic.
    if (last_temp > panic_high_temp)
    {
        panic("last_temp too high (%s > %d)",
            dtostr(last_temp).c_str(),
            panic_high_temp);
    }

    // Reset the watchdog timer.
    watchdog_reset();
}

void read_buttons()
{
    while (Serial1.available()) {
        int raw = Serial1.read();
        // The 4 button bits are replicated and inverted between the low and high nybbles.
        // Check that they match, and extract just one copy.
        if ((raw & 0x0F) == (((raw >> 4) & 0x0F) ^ 0x0F))
        {
            buttons = raw >> 4;
        }
    }
}

void display_update_checksum()
{
    uint8_t sum = 0;
    sum += display_buffer[1];
    sum += display_buffer[2];
    sum += display_buffer[3];
    sum += display_buffer[4];
    display_buffer[5] = sum;
}

// Values in digit places:
// 0 - 9 - digit
// 0x0a - blank
// 0x0b -- "P"
void display_set_digit(int digit, uint8_t value)
{
    if ((digit >= 0) && (digit < 3))
    {
        if (display_buffer[2 + digit] != value)
        {
            display_buffer[2 + digit] = value;
            display_dirty = true;
        }
    }
}

void display_set_digits(uint8_t a, uint8_t b, uint8_t c)
{
    display_set_digit(0, a);
    display_set_digit(1, b);
    display_set_digit(2, c);
}

void display_set_bits(int byte, int mask, bool value)
{
    if (value) {
        if ((display_buffer[byte] & mask) == 0)
        {
            display_buffer[byte] |= mask;
            display_dirty = true;
        }
    }
    else {
        if ((display_buffer[byte] & mask) != 0)
        {
            display_buffer[byte] &= ~mask;
            display_dirty = true;
        }
    }
}

void display_filter(bool on)
{
    display_set_bits(1, 0x10, on);
}

void display_heat(bool on)
{
    display_set_bits(1, 0x20, on);
}

void display_temperature(int temp)
{
    display_set_digits(temp / 100, (temp / 10) % 10, temp % 10);
}

void display_panic()
{
    display_temperature(adc_to_farenheit(smoothed_sensor_reading(panic_flash ? 0 : 1)));
    display_heat(!panic_flash);
    display_filter(panic_flash);
}

void display_panic_countdown(int countdown)
{
    display_set_digits(0x0a, countdown, 0x0a);
    display_filter(true);
    display_heat(true);
}

void display_vcc(){}

void display_send()
{
    if (display_dirty)
    {
        display_update_checksum();
        Serial1.write(display_buffer, display_bytes);
        display_dirty = false;
    }
}

void set_pump(bool running)
{
    if (pump_running != running)
    {
        display_filter(running);
        digitalWrite(pin_pump, running);
        pump_running = running;
        pump_switch_millis = millis();
    }
}

void setup()
{
    watchdog_init();

    serial_debug_init();

    // Make sure the pins shared with hardware SPI via the shield are high-impedance.
#ifdef QUIESCE_PINS
    QUIESCE_PINS
#endif

        // Communications with the control panel is 2400 baud ttl serial.
        Serial1.begin(2400
#if defined(ARDUINO_ARCH_ESP32)
            // Specify format and pins
            , SERIAL_8N1
            , PANEL_RX
            , PANEL_TX
            , false // invert?
#endif
        );

#ifdef ADC_AREF_OPTION
    // Set the AREF voltage for reading from the sensors.
    analogReference(ADC_AREF_OPTION);
#endif
#ifdef ADC_ATTENUATION_FACTOR
    analogSetAttenuation(ADC_ATTENUATION_FACTOR);
#endif

    pinMode(pin_pump, OUTPUT);
    // Make really sure the pump is not running.
    digitalWrite(pin_pump, 0);
    display_vcc();
    enter_state(runstate_startup);

    NTPInit();
}

void check_temp_validity()
{
    // Temperature readings are meaningless if the pump isn't running.
    // We also want to ignore temp readings when the pump was just turned on,
    // and consider the temp valid for a short time after it turns off.
    if (pump_running && !temp_valid)
    {
        if (millis() - pump_switch_millis > temp_settle_millis)
        {
            // The pump has been running long enough for temp to be valid.
            temp_valid = true;
        }
    }
    else if (!pump_running && temp_valid)
    {
        if (millis() - pump_switch_millis > temp_decay_millis)
        {
            // The pump has been off long enough that we should no longer consider the temp valid.
            temp_valid = false;
        }
    }

    // If the temp is currently valid, update the last valid temp
    if (temp_valid) {
        last_valid_temp = last_temp;
    }
}

bool check_scheduler() {
    bool _enable = false;
    bool _active = false;
    ESP32Time _rtc;

    String _time = _rtc.getTime("%H:%M");

    TubeScheduler* _scheduler = &TubeScheduler1;
    while (_scheduler != nullptr) {
        if (_scheduler->isActive()) {
            if ((_time >= _scheduler->On()) && (_time < _scheduler->Off())) {
                _enable = true;
            }
            _active = true;
        }
        _scheduler = (TubeScheduler*)_scheduler->getNext();
    }

    // No scheduler defined, so we return true
    if (!_active) {
        _enable = true;
    }

    return _enable;

}

// Make this a global so it can be displayed by the debug web endpoint
uint32_t loop_time = 0;

void loop() {

    wifiLoop();
    if (iotWebConf.getState() == iotwebconf::OnLine) {
        NTPloop();
    }


    uint32_t loop_start_micros = micros();
    uint32_t loop_start_millis = millis();

    read_temp_sensors();
    read_buttons();
    check_temp_validity();
    
    bool scheduler_enabled = check_scheduler();
    bool jets_pushed = false;
    bool lights_pushed = false;

    if (last_buttons != buttons)
    {
        // One or more buttons changed.
        buttons_last_transition_millis = loop_start_millis;
        debug("Buttons changed to 0x%02x, buttons_last_transition_millis = %ld", int(buttons), buttons_last_transition_millis);

        if (runstate != runstate_panic)
        {
            if (!(last_buttons & button_jets) && (buttons & button_jets))
            {
                // Jets button was pushed.
                jets_pushed = true;
            }

            if (!(last_buttons & button_up) && (buttons & button_up))
            {
                // Up button was pushed.
                temp_adjust(1);
            }

            if (!(last_buttons & button_down) && (buttons & button_down))
            {
                // Down button was pushed.
                temp_adjust(-1);
            }

            if (!(last_buttons & button_light) && (buttons & button_light))
            {
                lights_pushed = true;
            }
        }
        else {
            // When in panic state, the only thing we do is look for the jets and lights buttons to be held down together.
            // Whenever the buttons change, reset the countdown timer.
            runstate_transition();
        }

        last_buttons = buttons;
    }

    // Useful timing shortcuts for the state machine
    uint32_t millis_since_last_transition = loop_start_millis - runstate_last_transition_millis;
    uint32_t seconds_since_last_transition = millis_since_last_transition / 1000l;
    uint32_t millis_since_button_change = loop_start_millis - buttons_last_transition_millis;
    uint32_t seconds_since_button_change = millis_since_button_change / 1000l;

    if (temp_adjusted)
    {
        uint32_t millis_since_temp_adjust = millis() - temp_adjusted_millis;

        // See if the temp-adjusted display window has expired.
        if (millis_since_temp_adjust > temp_adjusted_display_millis)
        {
            temp_adjusted = false;
        }
        else
        {
            if (((buttons == button_up) || (buttons == button_down)) &&
                (millis_since_temp_adjust <= millis_since_button_change))
            {
                // If the user has been holding down the up or down button since the last temp adjustment,
                // and the last temp adjustment was over the repeat time (1/2s for the first adjustment, 1/4s for the next 1.5 seconds, 1/10s thereafter), 
                // adjust again.
                uint32_t repeat_time = 500;
                if (millis_since_button_change > 2000) {
                    repeat_time = 100;
                }
                else if (millis_since_button_change > 500) {
                    repeat_time = 250;
                }

                if (millis_since_temp_adjust >= repeat_time)
                {
                    temp_adjust((buttons == button_up) ? 1 : -1);
                }
            }
        }
    }

    // Default to displaying the current temp if it's valid and not recently adjusted, or the set point otherwise.
    if (temp_valid && !temp_adjusted) {
        display_temperature(last_temp);
    }
    else {
        display_temperature(temp_setting);
    }

    // In most states, we want the heating light off.
    display_heat(false);

    // Pushing the lights button at any time immediately enters the test state.
    // if (runstate != runstate_test && lights_pushed) {
    //   enter_state(runstate_test);
    //   lights_pushed = false;
    // }

    switch (runstate) {
    case runstate_startup:
        if (seconds_since_last_transition > startup_wait_seconds)
        {
            // Before starting the pump for the first time, enable the watchdog timer.
            watchdog_start();

            // While we may not have an _actual_ valid temp yet, we should at least have a smoothed version of the current sensor temp.
            // Use this for reporting stats for now, so we don't return garbage.
            last_valid_temp = last_temp;

            // Turn on the pump and enter the "finding temp" state.
            enter_state(runstate_finding_temp);
        }
        else {
            display_set_digits(0x0a, 0x00, 0x0a);
        }
        break;

    case runstate_finding_temp:
        if (jets_pushed) {
            // From the finding temp state, allow the user to turn off the pump manually.
            enter_state(runstate_idle);
        }
        else if (temp_valid) {
            // Go into the heating state. It will flip back to idle if no heating is needed.
            enter_state(runstate_heating);
        }
        else if (scheduler_enabled) {
            set_pump(true);
            // indicate that we're still finding temp
            display_set_digits(0x0a, 0x0b, 0x0a);
        }
        break;

    case runstate_heating:
        if (jets_pushed) {
            // From the heating state, allow the user to turn off the pump manually.
            enter_state(runstate_idle);
        }
        else if (last_valid_temp > (temp_setting + temp_setting_range)) {
            // We've reached the set point. Turn the pump off and go to idle.
            enter_state(runstate_idle);
        }
        else if (scheduler_enabled) {
            set_pump(true);
            display_heat(true);
        }
        break;

    case runstate_idle:
        if (jets_pushed) {
            // From the idle state, allow the user to turn on the pump manually.
            enter_state(runstate_manual_pump);
        }
        else if (seconds_since_last_transition > idle_seconds) {
            // We've been idle long enough that we should do a temperature check.
            enter_state(runstate_finding_temp);
        }
        else if ((temp_adjusted) && ((temp_setting + temp_setting_range) > last_valid_temp)) {
            // The user adjusted the temperature to above the last valid reading.
            // Transition to the finding temperature state to figure out if we need to heat.
            enter_state(runstate_finding_temp);
        }
        else {
            set_pump(false);
        }
        break;

    case runstate_manual_pump:
        if (jets_pushed) {
            // From the manual state, allow the user to turn off the pump.
            enter_state(runstate_idle);
        }
        else if (seconds_since_last_transition > manual_pump_seconds) {
            // Don't let the manual mode run for too long.
            enter_state(runstate_finding_temp);
        }
        else {
            set_pump(true);
        }
        break;

    case runstate_test:
        if (lights_pushed)
        {
            // Exit the test state
            enter_state(runstate_finding_temp);
        }
        break;

    case runstate_panic:
        set_pump(false);
        if (buttons == (button_jets | button_light)) {
            // User is holding down the button combo. 
            if (seconds_since_button_change >= panic_wait_seconds) {
                // They've successfully escaped.
                enter_state(runstate_startup);
            }
            else {
                display_panic_countdown(panic_wait_seconds - seconds_since_button_change);
            }
        }
        else {
            // Not holding buttons, just flash
            if (millis_since_last_transition >= 500) {
                panic_flash = !panic_flash;
                runstate_transition();
            }
            display_panic();
        }
        break;
    }

    // If the temp was recently adjusted, override other modes and display it.
    if (temp_adjusted) {
        display_temperature(temp_setting);
    }

    // Always update the display of vcc.
    display_vcc();
    display_send();

    loop_time = micros() - loop_start_micros;
    if (loop_time < loop_microseconds)
    {
        uint32_t msRemaining = loop_microseconds - loop_time;
        // debug("loop time %ld, start %ld, remaining %ld", loop_time, loop_start_micros, msRemaining);
        delayMicroseconds(msRemaining);
    }
}