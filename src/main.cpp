#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include "sdkconfig.h"
#include <Arduino.h>

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO (gpio_num_t)CONFIG_BLINK_GPIO

#ifndef LED_BUILTIN
#define LED_BUILTIN 4
#endif

String formatMillis(unsigned long rawMillis);

void blink_task(void *pvParameter)
{
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
    while(1) {
        /* Blink off (output low) */
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// Task that prints uptime every 10 seconds
void printUptimeTask(void *parameter) {
  while (true) {
    unsigned long currentMillis = millis();
    String uptime = formatMillis(currentMillis);
    Serial.println("⏱️ Device Uptime: " + uptime);
    vTaskDelay(pdMS_TO_TICKS(10000));  // Delay for 10 seconds
  }
}

void setup() {
    Serial.begin(115200);
    xTaskCreate(&printUptimeTask, "Print running time", configMINIMAL_STACK_SIZE, NULL, 0, NULL);

    pinMode(LED_BUILTIN, OUTPUT);

    // Power saving
    btStop();  // disables Bluetooth
    setCpuFrequencyMhz(80);
}

void loop()
{
}

String formatMillis(unsigned long rawMillis) {
  unsigned long hours    = rawMillis / 3600000;
  unsigned long minutes  = (rawMillis % 3600000) / 60000;
  unsigned long seconds  = (rawMillis % 60000) / 1000;
  unsigned long millisec = rawMillis % 1000;

  char timeBuffer[16];
  snprintf(timeBuffer, sizeof(timeBuffer), "%02lu:%02lu:%02lu.%03lu",
           hours, minutes, seconds, millisec);

  return String(timeBuffer);
}