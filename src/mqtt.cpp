#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt.h"
#include "ota.h"
#include "esp_log.h"
#include "mqtt_client.h"

// --- Module-Private Constants ---
namespace {
    constexpr const char* TAG = "MQTT";
    constexpr const char* MQTT_BROKER_URI = "mqtt://your_broker_ip"; // CHANGE THIS
    constexpr const char* MQTT_CLIENT_ID = "sew-matrix-clock-client";
    constexpr const char* MQTT_OTA_TOPIC = "device/ota/url";
}

// --- Native MQTT Event Handler ---
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    const auto* event = static_cast<const esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(event->client, MQTT_OTA_TOPIC, 0);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, MQTT_OTA_TOPIC, event->topic_len) == 0) {
                char urlBuffer[256];
                if (event->data_len >= sizeof(urlBuffer)) {
                    ESP_LOGE(TAG, "Received URL is too long.");
                    break;
                }

                memcpy(urlBuffer, event->data, event->data_len);
                urlBuffer[event->data_len] = '\0';

                ESP_LOGI(TAG, "Received firmware URL via MQTT: %s", urlBuffer);
                if (xQueueSend(otaUrlQueue, &urlBuffer, pdMS_TO_TICKS(100)) != pdPASS) {
                    ESP_LOGE(TAG, "Error sending URL to OTA queue.");
                }
            }
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;

        default:
            break;
    }
}

// --- Public Start Function ---
void mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {};

    // This nested structure is correct for ESP-IDF v5.x
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;
    mqtt_cfg.credentials.client_id = MQTT_CLIENT_ID;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, nullptr);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "Native MQTT client started.");
}