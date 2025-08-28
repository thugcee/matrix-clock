#pragma once
#include "result.h"
#include <WString.h>

Result<String, String> get_forecast(int start_hour = 0);
