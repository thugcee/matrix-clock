#pragma once

/**
 * @brief Initializes and starts the MQTT client.
 *
 * Configures the native ESP-IDF MQTT client, registers an event handler,
 * and starts the client. It will automatically connect and reconnect in the
 * background. It should be called once during application setup.
 */
void mqtt_app_start();
