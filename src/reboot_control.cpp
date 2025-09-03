#include "reboot_control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <HardwareSerial.h>

// Reboot storm detection mechanism.
namespace reboot_control {

constexpr uint8_t MAX_REBOOTS = 3;
constexpr uint32_t STABLE_RUNTIME_MS = 20000;
constexpr uint32_t SLEEP_TIME_ON_STORM_US = 30000000;
const char NVS_NAMESPACE[] = "reboots";
const char VAR_NAME[] = "reboot_count";

ByteResult increment_reboot_count() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return ByteResult::Err("RebootControl: Failed to open NVS namespace");

    uint8_t count = 0;
    esp_err_t r = nvs_get_u8(handle, VAR_NAME, &count);
    if (r != ESP_OK && r != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ByteResult::Err("RebootControl: Failed to read reboot count from NVS");
    }
    count++;
    nvs_set_u8(handle, VAR_NAME, count);
    r = nvs_commit(handle);
    if (r != ESP_OK) {
        nvs_close(handle);
        return ByteResult::Err("RebootControl: Failed to commit reboot count to NVS");
    }
    nvs_close(handle);

    Serial.printf("RebootControl: Reboot count: %d\n", count);

    return ByteResult::Ok(count);
}

ByteResult reset_reboot_count() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
        return ByteResult::Err("RebootControl: Failed to open NVS namespace");

    nvs_set_u8(handle, VAR_NAME, 0);
    esp_err_t r = nvs_commit(handle);
    if (r != ESP_OK) {
        nvs_close(handle);
        return ByteResult::Err("RebootControl: Failed to commit reboot count to NVS");
    }
    nvs_close(handle);

    Serial.println("RebootControl: Reboot count reset.");
    return ByteResult::Ok(0);
}

void reset_reboot_counter_task(void* param) {
    vTaskDelay(pdMS_TO_TICKS(STABLE_RUNTIME_MS));
    ByteResult res = reset_reboot_count();
    if (!res) {
        Serial.println(res.unwrapErr());
        return;
    } else {
        Serial.println("Device runtime stable. Reboot counter reset.");
    }
    vTaskDelete(NULL); // Delete the task
}

void handleRebootStormDetection() {
    // Reboot storm detection mechanism.
    ByteResult res = increment_reboot_count();
    if (!res) {
        Serial.println(res.unwrapErr());
        return;
    }
    uint8_t reboot_count = res.unwrap();
    if (reboot_count >= MAX_REBOOTS) {
        Serial.println("Reboot storm detected! Entering deep sleep for " +
                       String(SLEEP_TIME_ON_STORM_US) + " ms");
        esp_sleep_enable_timer_wakeup(SLEEP_TIME_ON_STORM_US);
        esp_deep_sleep_start();
    }
    // Monitor uptime to reset reboot counter after stable runtime.
    xTaskCreate(reboot_control::reset_reboot_counter_task, "ResetRebootCounter", 2048, NULL, 1,
                NULL);
}

} // namespace reboot_control
