#include "relays_app.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <array>
#include <cstdio>

static auto TAG = "relays_app";

static constexpr gpio_num_t RELAY_GPIOS[RELAY_COUNT] = {RELAY_GPIO_1, RELAY_GPIO_2, RELAY_GPIO_3};

struct RelayState {
    bool on = false;
};

static std::array<RelayState, RELAY_COUNT> g_states{};

static inline int logical_to_level(const bool on) {
    return (RELAY_ACTIVE_HIGH ? (on ? 1 : 0) : (on ? 0 : 1));
}

static esp_err_t nvs_init_and_erase_if_needed() {
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition format required, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        r = nvs_flash_init();
    }
    return r;
}

static bool nvs_load_state(const size_t idx, bool& out_on) {
    nvs_handle_t handle;
    char key[16];
    std::snprintf(key, sizeof(key), NVS_KEY_FMT, static_cast<unsigned>(idx + 1));
    esp_err_t r = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (r != ESP_OK)
        return false;
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
    std::snprintf(key, sizeof(key), NVS_KEY_FMT, static_cast<unsigned>(idx + 1));
    esp_err_t r = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (r != ESP_OK)
        return r;
    const uint8_t val = on ? 1 : 0;
    r = nvs_set_u8(handle, key, val);
    if (r == ESP_OK)
        r = nvs_commit(handle);
    nvs_close(handle);
    return r;
}

void relays_app_init() {
    ESP_LOGI(TAG, "Initializing relays app...");

    if (esp_err_t r = nvs_init_and_erase_if_needed(); r != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %d", r);
    }

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

    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        bool on = false;
        if (nvs_load_state(i, on)) {
            ESP_LOGI(TAG, "Loaded r%u = %u", unsigned(i + 1), unsigned(on));
        } else {
            ESP_LOGI(TAG, "No saved state for r%u, default OFF", unsigned(i + 1));
        }
        relays_app_set_state(i, on);
    }

    ESP_LOGI(TAG, "Relays app initialized");
}

void relays_app_set_state(size_t idx, bool on) {
    if (idx >= RELAY_COUNT)
        return;
    g_states[idx].on = on;
    const int level = logical_to_level(on);
    gpio_set_level(RELAY_GPIOS[idx], level);
    if (const esp_err_t r = nvs_save_state(idx, on); r != ESP_OK) {
        ESP_LOGW(TAG, "Failed save state r%u: %d", unsigned(idx + 1), r);
    }
}

bool relays_app_get_state(size_t idx) {
    if (idx >= RELAY_COUNT)
        return false;
    return g_states[idx].on;
}
