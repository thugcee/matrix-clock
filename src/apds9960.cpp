#include "apds9960.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <Adafruit_APDS9960.h>
#include <Arduino.h>

bool setupAPDS9960(Adafruit_APDS9960& sensor, gpio_num_t interruptPin, void (*isr)(void*)) {
    Serial.printf("APDS-9960: Initializing APDS9960 with interruption on PIN %d\n", interruptPin);

    // Configure the interrupt pin using the passed parameter
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // falling edge
    io_conf.pin_bit_mask = (1ULL << interruptPin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // I2C should already be initialized before calling this function
    if (!sensor.begin()) {
        Serial.println("APDS-9960: Error: APDS-9960 not found on I2C bus!");
        return false;
    }
    Serial.println("APDS-9960: sensor found.");

    sensor.enableGesture(false);
    sensor.enableColor(false);
    sensor.setProxGain(APDS9960_PGAIN_8X);
    sensor.setProximityInterruptThreshold(0, 5, 10); // low, high
    sensor.enableProximity(true);
    sensor.enableProximityInterrupt();

    Serial.println("APDS-9960: Sensing enabled.");

    // Attach the interrupt handler
    gpio_isr_handler_add(interruptPin, isr, nullptr);

    Serial.println("APDS-9960: Initialization completed.");
    return true;
}
