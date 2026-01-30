// relays_app.cpp
// Quick-Fix implementation:
// - Uses ESP-IDF mqtt_client in a FreeRTOS task
// - Exposes void relays_app_start() to call from Arduino setup() after WiFi connected
// - Controls relays (GPIOs from config.h), persists states in NVS, publishes MQTT state

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include <cstdio>
#include <array>

static auto TAG = "relays_app";

constexpr size_t RELAY_COUNT = 3; // update if you enable a 4th
static constexpr gpio_num_t RELAY_GPIOS[RELAY_COUNT] = {
    RELAY_GPIO_1,
    RELAY_GPIO_2,
    RELAY_GPIO_3
};

struct RelayState {
    bool on = false; // logical state: true = on
};

static std::array<RelayState, RELAY_COUNT> g_states{};

// Helpers: convert logical state to GPIO level considering active HIGH/LOW
static inline int logical_to_level(const bool on) {
    return (RELAY_ACTIVE_HIGH ? (on ? 1 : 0) : (on ? 0 : 1));
}

// NVS helpers
static esp_err_t nvs_init_and_erase_if_needed() {
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition format required, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    return r;
}

static bool nvs_load_state(const size_t idx, bool &out_on) {
    nvs_handle_t handle;
    char key[16];
    std::snprintf(key, sizeof(key), NVS_KEY_FMT, static_cast<unsigned>(idx + 1)); // r1..rN
    esp_err_t r = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (r != ESP_OK) return false;
    uint8_t val = 0;
    r = nvs_get_u8(handle, key, &val);
    nvs_close(handle);
    if (r == ESP_OK) {
        out_on = (val != 0);
        return true;
    }
    return false;
}

static esp_err_t nvs_save_state(const size_t idx, const bool on) {
    nvs_handle_t handle;
    char key[16];
    std::snprintf(key, sizeof(key), NVS_KEY_FMT, static_cast<unsigned>(idx + 1)); // r1..rN
    esp_err_t r = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (r != ESP_OK) return r;
    const uint8_t val = on ? 1 : 0;
    r = nvs_set_u8(handle, key, val);
    if (r == ESP_OK) r = nvs_commit(handle);
    nvs_close(handle);
    return r;
}

// MQTT topic helpers
static void build_topic(char *buf, size_t buf_sz, const char *purpose, const size_t relay_idx) {
    // purpose: "command" or "state"
    // topic: MQTT_TOPIC_BASE/light{N}/{purpose}
    std::snprintf(buf, buf_sz, "%s/light%u/%s", MQTT_TOPIC_BASE, static_cast<unsigned>(relay_idx + 1), purpose);
}

static esp_mqtt_client_handle_t mqtt_client = nullptr;

// Publish state (qos0, no retain here; Home Assistant discovery may prefer retained state — change if desired)
static void publish_state(size_t idx) {
    char topic[64];
    build_topic(topic, sizeof(topic), "state", idx);
    const char *msg = g_states[idx].on ? "ON" : "OFF";
    if (mqtt_client) {
        int msg_id = esp_mqtt_client_publish(mqtt_client, topic, msg, 0, 0, 1); // retain=1 to keep last state across broker restarts
        ESP_LOGI(TAG, "Published %s -> %s (msg_id=%d)", topic, msg, msg_id);
    } else {
        ESP_LOGW(TAG, "MQTT client not ready, cannot publish %s", topic);
    }
}

// Build discovery topic: homeassistant/switch/<device>_lightN/config
static void build_discovery_topic(char *buf, const size_t buf_sz, const size_t idx) {
    std::snprintf(buf, buf_sz, "homeassistant/switch/%s_light%u/config", DEVICE_NAME, static_cast<unsigned>(idx + 1));
}

// Build discovery payload (retained) — minimal fields
static void publish_discovery(const size_t idx) {
    if (!mqtt_client) return;
    char topic[128];
    build_discovery_topic(topic, sizeof(topic), idx);

    // state and command topics
    char state_topic[64], cmd_topic[64];
    build_topic(state_topic, sizeof(state_topic), "state", idx);
    build_topic(cmd_topic, sizeof(cmd_topic), "command", idx);

    // Unique ID and JSON payload
    char payload[512];
    std::snprintf(payload, sizeof(payload),
                  R"({"name":"%s light%u","uniq_id":"%s_light%u","cmd_t":"%s","stat_t":"%s","qos":0,"pl_on":"ON","pl_off":"OFF","~":""})",
                  DEVICE_NAME, static_cast<unsigned>(idx + 1),
                  DEVICE_NAME, static_cast<unsigned>(idx + 1),
                  cmd_topic, state_topic);

    // Publish retained so Home Assistant can discover after restart
    const int msg_id = esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 0, 1);
    ESP_LOGI(TAG, "Published discovery %s (msg_id=%d)", topic, msg_id);
}

// Apply logical state to GPIO and persist & publish
static void apply_state(const size_t idx, const bool on) {
    if (idx >= RELAY_COUNT) return;
    g_states[idx].on = on;
    const int level = logical_to_level(on);
    gpio_set_level(RELAY_GPIOS[idx], level);
    if (const esp_err_t r = nvs_save_state(idx, on); r != ESP_OK) {
        ESP_LOGW(TAG, "Failed save state r%u: %d", unsigned(idx + 1), r);
    }
    publish_state(idx);
}

// MQTT event handler
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // subscribe to all command topics for relays
        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            char topic[64];
            build_topic(topic, sizeof(topic), "command", i);
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, topic, 0);
            ESP_LOGI(TAG, "Subscribed to %s (msg_id=%d)", topic, msg_id);

            // Publish discovery and current retained state
            publish_discovery(i);
            publish_state(i);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
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
        // topic and payload are NOT null-terminated
        std::string topic(event->topic, event->topic_len);
        std::string payload(event->data, event->data_len);
        ESP_LOGI(TAG, "MQTT_EVENT_DATA topic=%s payload=%s", topic.c_str(), payload.c_str());
        // Check command topics: "home/lightN/command"
        for (size_t i = 0; i < RELAY_COUNT; ++i) {
            char expected[64];
            build_topic(expected, sizeof(expected), "command", i);
            if (topic == expected) {
                // Accept "ON"/"OFF" (case-insensitive) and "1"/"0"
                if (!payload.empty()) {
                    if (payload == "ON" || payload == "on" || payload == "1") {
                        apply_state(i, true);
                    } else if (payload == "OFF" || payload == "off" || payload == "0") {
                        apply_state(i, false);
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
        ESP_LOGI(TAG, "Other MQTT event id=%d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    mqtt_event_handler_cb(static_cast<esp_mqtt_event_handle_t>(event_data));
}

static void start_mqtt_client_task(void *pvParameters) {
    ESP_LOGI(TAG, "Relays MQTT task starting...");
    // Initialize NVS
    if (esp_err_t r = nvs_init_and_erase_if_needed(); r != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %d", r);
    }

    // Configure GPIOs
    gpio_config_t io_conf{};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 0;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    for (auto i : RELAY_GPIOS) {
        io_conf.pin_bit_mask |= (1ULL << i);
    }
    gpio_config(&io_conf);

    // Load saved states (if present) and apply (this will also set GPIOs)
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        bool on = false;
        if (nvs_load_state(i, on)) {
            ESP_LOGI(TAG, "Loaded r%u = %u", unsigned(i + 1), unsigned(on));
        } else {
            ESP_LOGI(TAG, "No saved state for r%u, default OFF", unsigned(i + 1));
        }
        apply_state(i, on);
    }

    // Quick startup action: toggle relay 1 once (turn on -> publish -> turn off)
    ESP_LOGI(TAG, "Quick toggle relay 1 for test");
    apply_state(0, true);
    vTaskDelay(pdMS_TO_TICKS(500));
    apply_state(0, false);

    // Prepare MQTT client config
    char uri[64];
    std::snprintf(uri, sizeof(uri), "mqtt://%s:%u", MQTT_BROKER_IP, static_cast<unsigned>(MQTT_BROKER_PORT));
    esp_mqtt_client_config_t mqtt_cfg{};
    mqtt_cfg.broker.address.uri = uri;
    mqtt_cfg.credentials.client_id = DEVICE_NAME;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to init mqtt client");
        vTaskDelete(nullptr);
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, nullptr);
    if (esp_err_t err = esp_mqtt_client_start(mqtt_client); err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start mqtt client: %d", err);
    }

    // Keep task alive to handle events (esp-mqtt uses event loop thread internally)
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// Public API to start the "relays" app. Call this from Arduino setup() AFTER WiFi is connected.
void relays_app_start() {
    // Create the task; it will initialize NVS, GPIOs, and MQTT client
    if (BaseType_t r =
            xTaskCreate(start_mqtt_client_task, "relays_mqtt", 8192, nullptr, 5, nullptr);
        r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create relays task");
    } else {
        ESP_LOGI(TAG, "Relays task created");
    }
}
