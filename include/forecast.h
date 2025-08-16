#pragma once
#include "result.h"
#include <WString.h>

Result<String, String> getForecast(int startHour = 0);
