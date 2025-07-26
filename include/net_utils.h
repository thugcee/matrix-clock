#pragma once
#include <NTPClient.h>

bool setupWiFi();
void setupNTP(NTPClient& timeClient);