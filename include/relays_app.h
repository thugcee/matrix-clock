#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t RELAY_COUNT = 3;

void relays_app_init();
void relays_app_set_state(size_t idx, bool on);
bool relays_app_get_state(size_t idx);
