#pragma once

#include "result.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace reboot_control {

using ByteResult = Result<uint8_t, const char*>;

void handleRebootStormDetection();

} // namespace reboot_control
