#pragma once
#include "freertos/queue.h"

// The handle for the queue that passes firmware URLs from triggers to the update task.
extern QueueHandle_t otaUrlQueue;

/**
 * @brief Initializes the OTA module.
 *
 * Creates the necessary FreeRTOS queue and task for handling OTA updates.
 * The task listens for firmware URLs on the queue and performs the update.
 * The triggering of OTA updates (e.g., via MQTT or HTTP) is implemented elsewhere.
 */
void ota_app_start();
