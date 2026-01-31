#pragma once

#include <cstddef>
#include <cstdint>

struct esp_mqtt_client;

constexpr size_t RELAY_COUNT = 3;

typedef struct esp_mqtt_client* esp_mqtt_client_handle_t;

void relays_app_init();
void relays_app_set_state(size_t idx, bool on);
bool relays_app_get_state(size_t idx);
void relays_app_publish_state(size_t idx);
void relays_app_publish_discovery(size_t idx);
void relays_app_publish_all();
void relays_app_subscribe_all();
void relays_set_mqtt_client(esp_mqtt_client_handle_t client);
esp_mqtt_client_handle_t relays_get_mqtt_client();
