#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace reboot_control {

extern uint8_t reboot_count;
extern const uint8_t MAX_REBOOTS;
extern const uint32_t STABLE_RUNTIME_MS;
extern const uint32_t SLEEP_TIME_ON_STORM_US;

void reset_reboot_counter_task(void* param);

} // namespace reboot_control
