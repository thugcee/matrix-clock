#pragma once
#include <Adafruit_APDS9960.h>
#include "driver/gpio.h"

bool setupAPDS9960(Adafruit_APDS9960& sensor, gpio_num_t interruptPin, void (*isr)(void*));
