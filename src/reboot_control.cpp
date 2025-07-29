#include "reboot_control.h"
#include <Arduino.h>

// Reboot storm detection mechanism.
namespace RebootControl {

const uint8_t MAX_REBOOTS = 3;
const uint32_t STABLE_RUNTIME_MS = 20000;
const uint32_t SLEEP_TIME_ON_STORM_US = 30000000;

RTC_DATA_ATTR uint8_t rebootCount = 0;

void resetRebootCounterTask(void* param) {
    const int STABLE_RUNTIME_MS = 5000; // Example stable runtime duration
    vTaskDelay(STABLE_RUNTIME_MS / portTICK_PERIOD_MS);
    rebootCount = 0; // Reset the reboot counter
    Serial.println("Device runtime stable. Reboot counter reset.");
    vTaskDelete(NULL); // Delete the task
}

} // namespace RebootControl
