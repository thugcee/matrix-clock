#pragma once
#include <NTPClient.h>

namespace net_utils {

bool setup_wifi();
void setup_NTP(NTPClient& timeClient);

} // namespace net_utils