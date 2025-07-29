#pragma once
#include <NTPClient.h>

namespace net_utils {

bool setupWiFi();
void setupNTP(NTPClient& timeClient);

} // namespace net_utils