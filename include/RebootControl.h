// RebootControl.h
#ifndef REBOOTCONTROL_H
#define REBOOTCONTROL_H

#include <Arduino.h>

namespace RebootControl {

extern RTC_DATA_ATTR uint8_t rebootCount;
extern const uint8_t MAX_REBOOTS;
extern const uint32_t STABLE_RUNTIME_MS;
extern const uint32_t SLEEP_TIME_ON_STORM_US;

void resetRebootCounterTask(void* param);

} // namespace RebootControl

#endif // REBOOTCONTROL_H
