#include "freertos/FreeRTOS.h"
#include "ota.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"

// --- Module-Private Constants ---
namespace {
    constexpr const char* TAG = "OTA";
    constexpr size_t OTA_URL_MAX_LENGTH = 256;
    constexpr size_t OTA_URL_QUEUE_LENGTH = 1;
    constexpr uint32_t OTA_TASK_STACK_SIZE = 8192;
    constexpr UBaseType_t OTA_TASK_PRIORITY = 5;
    constexpr uint32_t DEV_TRIGGER_TASK_STACK_SIZE = 4096;
    constexpr UBaseType_t DEV_TRIGGER_TASK_PRIORITY = 4;
}

// --- Module Globals ---
QueueHandle_t otaUrlQueue;

// =========================================================================
// Production OTA Update Task
// =========================================================================
void otaUpdateTask(void* pvParameters) {
    ESP_LOGI(TAG, "Production OTA Task started, waiting for update URL...");
    char urlBuffer[OTA_URL_MAX_LENGTH];

    for (;;) {
        if (xQueueReceive(otaUrlQueue, &urlBuffer, portMAX_DELAY) == pdPASS) {
            ESP_LOGI(TAG, "OTA task received URL: %s", urlBuffer);

            // Step 1: Configure the underlying HTTP client.
            // This is the same as before, but we'll name it http_config for clarity.
            const esp_http_client_config_t http_config = {
                .url = urlBuffer,
                .cert_pem = nullptr, // Setting this to nullptr allows plain HTTP URLs.
                .timeout_ms = 15000,
                .keep_alive_enable = true,
            };

            // Step 2: Create the new, required esp_https_ota_config_t struct.
            // This struct wraps the HTTP client configuration.
            const esp_https_ota_config_t ota_config = {
                .http_config = &http_config,
            };

            // Step 3: Call esp_https_ota() with a pointer to the new ota_config struct.
            const esp_err_t ret = esp_https_ota(&ota_config);

            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "OTA update successful. Rebooting now.");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
            }
        }
    }
}


// ... The rest of the file (DevTrigger namespace and ota_app_start) remains exactly the same ...
// =========================================================================
// Development OTA Trigger (HTTP Server)
// =========================================================================
namespace DevTrigger {
    esp_err_t ota_trigger_post_handler(httpd_req_t *req) {
        char url_buf[OTA_URL_MAX_LENGTH] = {0};
        const size_t remaining = req->content_len;

        if (remaining >= OTA_URL_MAX_LENGTH) {
            ESP_LOGE(TAG, "URL is too long");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        if (httpd_req_recv(req, url_buf, remaining) <= 0) { return ESP_FAIL; }

        ESP_LOGI(TAG, "Received dev trigger with URL: %s", url_buf);

        if (xQueueSend(otaUrlQueue, &url_buf, pdMS_TO_TICKS(100)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to send URL to OTA queue.");
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        constexpr char resp[] = "OK, update triggered.";
        httpd_resp_send(req, resp, static_cast<ssize_t>(strlen(resp)));
        return ESP_OK;
    }

    const httpd_uri_t ota_trigger_uri = {
        .uri       = "/ota_trigger",
        .method    = HTTP_POST,
        .handler   = ota_trigger_post_handler,
    };

    void dev_ota_trigger_task(void* pvParameters) {
        httpd_handle_t server = nullptr;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.lru_purge_enable = true;

        if (httpd_start(&server, &config) == ESP_OK) {
            httpd_register_uri_handler(server, &ota_trigger_uri);
            ESP_LOGI(TAG, "Development OTA trigger server is running.");
        } else {
            ESP_LOGE(TAG, "Error starting dev trigger server!");
        }
        vTaskDelete(nullptr);
    }
} // namespace DevTrigger

// =========================================================================
// Public Start Function
// =========================================================================
void ota_app_start() {
    otaUrlQueue = xQueueCreate(OTA_URL_QUEUE_LENGTH, OTA_URL_MAX_LENGTH);
    if (otaUrlQueue == nullptr) {
        ESP_LOGE(TAG, "Error creating OTA URL queue");
        return;
    }

    xTaskCreate(otaUpdateTask, "OTA Update Task", OTA_TASK_STACK_SIZE, nullptr, OTA_TASK_PRIORITY, nullptr);
    xTaskCreate(DevTrigger::dev_ota_trigger_task, "Dev OTA Trigger", DEV_TRIGGER_TASK_STACK_SIZE, nullptr, DEV_TRIGGER_TASK_PRIORITY, nullptr);
}