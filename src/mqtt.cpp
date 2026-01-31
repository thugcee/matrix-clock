#include "mqtt.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mqtt_client.h"
#include "ota.h"
#include "relays_app.h"
#include <string>

namespace {
constexpr const char* TAG = "MQTT";
constexpr const char* MQTT_OTA_TOPIC = "device/ota/url";
} // namespace

static void build_relay_topic(char* buf, size_t buf_sz, const char* purpose, size_t relay_idx) {
    std::snprintf(buf, buf_sz, "%s/light%u/%s", MQTT_TOPIC_BASE,
                  static_cast<unsigned>(relay_idx + 1), purpose);
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                               void* event_data) {
    const auto* event = static_cast<const esp_mqtt_event_handle_t>(event_data);

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        esp_mqtt_client_subscribe(event->client, MQTT_OTA_TOPIC, 0);

        relays_app_subscribe_all();
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA: {
        std::string topic(event->topic, event->topic_len);
        std::string payload(event->data, event->data_len);
        ESP_LOGI(TAG, "MQTT_EVENT_DATA topic=%s payload=%s", topic.c_str(), payload.c_str());

        if (topic == MQTT_OTA_TOPIC) {
            if (event->data_len >= 256) {
                ESP_LOGE(TAG, "Received OTA URL is too long.");
                break;
            }
            char urlBuffer[256];
            memcpy(urlBuffer, event->data, event->data_len);
            urlBuffer[event->data_len] = '\0';

            ESP_LOGI(TAG, "Received firmware URL via MQTT: %s", urlBuffer);
            if (xQueueSend(otaUrlQueue, urlBuffer, pdMS_TO_TICKS(100)) != pdPASS) {
                ESP_LOGE(TAG, "Error sending URL to OTA queue.");
            }
            break;
        }

        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            char expected[64];
            build_relay_topic(expected, sizeof(expected), "command", i);
            if (topic == expected) {
                if (!payload.empty()) {
                    if (payload == "ON" || payload == "on" || payload == "1") {
                        relays_app_set_state(i, true);
                        relays_app_publish_state(i);
                    } else if (payload == "OFF" || payload == "off" || payload == "0") {
                        relays_app_set_state(i, false);
                        relays_app_publish_state(i);
                    } else {
                        ESP_LOGW(TAG, "Unknown payload for %s: %s", expected, payload.c_str());
                    }
                }
                break;
            }
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        break;
    }
}

void mqtt_app_start() {
    ESP_LOGI(TAG, "Starting MQTT client...");

    relays_app_init();

    char uri[64];
    std::snprintf(uri, sizeof(uri), "mqtt://%s:%u", MQTT_BROKER_IP,
                  static_cast<unsigned>(MQTT_BROKER_PORT));

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = uri;
    mqtt_cfg.credentials.client_id = DEVICE_NAME;
    mqtt_cfg.credentials.username = MQTT_USERNAME;
    mqtt_cfg.credentials.authentication.password = MQTT_PASSWORD;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    relays_set_mqtt_client(client);
    esp_mqtt_client_register_event(client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                   mqtt_event_handler, nullptr);

    if (esp_err_t err = esp_mqtt_client_start(client); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %d", err);
    } else {
        ESP_LOGI(TAG, "MQTT client started successfully");
    }
}
