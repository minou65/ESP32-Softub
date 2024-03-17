#include "common.h"

double fahrenheitToCelsius(double fahrenheit) {
    double celsius;

    celsius = (fahrenheit - 32.0) * 5.0 / 9.0;
    return celsius;
}


double celsiusToFahrenheit(double celsius) {
	double fahrenheit;

	fahrenheit = celsius * 9.0 / 5.0 + 32.0;
	return fahrenheit;
}