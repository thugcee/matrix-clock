#include "apds9960.h"
#include "config.h"
#include "display_pages.h"
#include "forecast.h"
#include "mem_mon.h"
#include "net_utils.h"
#include "reboot_control.h"
#include "secrets.h"
#include "time_utils.h"

#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <Adafruit_APDS9960.h> // <- Changed include
#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <MD_Parola.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

volatile DisplayPage current_page = DisplayPage::Time;

WiFiUDP net_UDP;
NTPClient timeClient(net_UDP, NTP_SERVER, 0, NTP_UPDATE_INTERVAL_MS); // UTC offset (0)

bool wifi_enabled = false;
bool gestures_enabled = false;
bool forecast_enabled = true;

// Gesture Sensor
Adafruit_APDS9960 apds;

// LED Matrix Display
MD_Parola parola_display = MD_Parola(DISPLAY_HARDWARE_TYPE, DISPLAY_DATA_PIN, DISPLAY_CLK_PIN,
                                     DISPLAY_CS_PIN, DISPLAY_MAX_DEVICES);

String time_data = "Err time";
ForecastData16 forecast_data{};
ForecastData16 forecast_err_data{0, 0, {0.f}, 0};
SemaphoreHandle_t display_data_sem;

static TaskHandle_t gestureTaskHandle = nullptr;

void display_forecast_chart();

/**
 * @brief FreeRTOS task that runs periodically to update the weather forecast.
 * @param pvParameters Task parameters (not used here).
 */
void weatherUpdateTask(void* pvParameters) {
    Serial.println("Weather update task started.");
    for (;;) { // Infinite loop for the task
        Serial.println("[Task] Fetching new weather forecast...");
        int startHour = get_GMT_hour();
        if (ForecastResult newForecast = get_forecast<FORECAST_HOURS>(startHour); newForecast) {
            if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
                forecast_data = std::move(newForecast.unwrap());
                xSemaphoreGive(display_data_sem);
                Serial.println("[Task] Forecast updated successfully.");
            }
            Serial.println("[Task] Successfully fetched forecast.");
        } else {
            if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
                forecast_data = forecast_err_data;
                xSemaphoreGive(display_data_sem);
            }
            Serial.println("[Task] Error fetching forecast: " + newForecast.unwrapErr());
        }

        // Wait for one hour before the next run
        const int updateIntervalMs = 3600 * 1000; // 1 hour
        vTaskDelay(updateIntervalMs / portTICK_PERIOD_MS);
    }
}

void display_time(String time, MD_Parola& parolaDisplay) {
    parolaDisplay.setTextAlignment(PA_CENTER);
    parolaDisplay.printf("%s", time.c_str());
}

void minuteChangeTask(void* pvParameters) {
    // Task that updates the time display every minute.
    while (1) {
        String tmp = get_formatted_local_time("%H : %M");
        if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
            if (current_page == DisplayPage::Time) {
                time_data = tmp;
                Serial.printf("[minute-change] displayed: %s, current time: %s\n,",
                              time_data.c_str(), get_formatted_local_time().c_str());
                display_time(time_data, parola_display);
            }
            xSemaphoreGive(display_data_sem);
        }

        wait_until_next_minute();
    }
}

void printStatusTask(void* parameter) {
    // Task that prints device uptime and local time periodically.
    char forecast_buf[12];
    while (true) {
        unsigned long currentMillis = millis();
        String uptime = format_millis(currentMillis);
        String localTime = get_formatted_local_time();
        format_temp_range(forecast_buf, sizeof(forecast_buf), forecast_data.min_temp,
                          forecast_data.max_temp);
        Serial.println("⏱️  Device Uptime: " + uptime + " | Real time: " + localTime +
                       " | Forecast: " + forecast_buf);
        vTaskDelay(pdMS_TO_TICKS(STATUS_UPDATE_INTERVAL_SECONDS * 1000));
    }
}

void initForecastUpdate() {
    // Initialize the weather forecast task
    display_data_sem = xSemaphoreCreateMutex();
    if (display_data_sem == NULL) {
        Serial.println("Error: Failed to create mutex.");
        forecast_enabled = false; // Disable forecast updates
    }
    if (wifi_enabled && forecast_enabled) {
        xTaskCreate(weatherUpdateTask, "WeatherUpdate", 8192, nullptr, 1, nullptr);
    } else {
        Serial.println("Weather updates are disabled.");
    }
}

void initSerial() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    } // wait for Serial on some boards
}

ForecastPage detect_forecast_page(uint8_t proximity) {
    if (proximity < FORECAST_PAGE_SWITCH_PROXIMITY) {
        return ForecastPage::Range;
    } else {
        return ForecastPage::Chart;
    }
}

void processProximity(ForecastPage page) {
    if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
        switch (page) {
        case ForecastPage::Range:
            parola_display.setTextAlignment(PA_LEFT);
            char forecast_buf[12];
            format_temp_range(forecast_buf, sizeof(forecast_buf), forecast_data.min_temp,
                              forecast_data.max_temp);
            parola_display.printf("%s", forecast_buf);
            break;
        case ForecastPage::Chart:
            display_forecast_chart();
            break;
        default:
            break;
        }
        xSemaphoreGive(display_data_sem);
    }
}

void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(gestureTaskHandle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

void gestureTask(void* pvParameters) {
    auto sensor = static_cast<Adafruit_APDS9960*>(pvParameters);
    for (;;) {
        sensor->enableProximityInterrupt();
        Serial.println("Gesture Task: waiting for notification...");
        // Block until notified by ISR. Use ulTaskNotifyTake or semaphore take.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // clear on exit

        Serial.println("Gesture Task: notification detected...");
        sensor->disableProximityInterrupt();
        if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
            current_page = DisplayPage::Forecast;
            xSemaphoreGive(display_data_sem);
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        unsigned long start_millis = get_uptime_millis();
        Serial.println("Gesture task: waiting for proximity leave...");
        uint8_t proximity;
        ForecastPage last_page = ForecastPage::None;
        while ((proximity = sensor->readProximity()) > 0) {
            ForecastPage page = detect_forecast_page(proximity);
            if (page != last_page) {
                processProximity(page);
                last_page = page;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            unsigned long current_millis = get_uptime_millis();
            if (current_millis - start_millis > 30 * 1000UL)
                break;
        }
        Serial.println("Gesture task: proximity cleared.");
        unsigned long end_millis = get_uptime_millis();
        unsigned long elapsed = end_millis - start_millis;
        if (long left = FORECAST_MINIMAL_DISPLAY_TIME_SECONDS * 1000UL - elapsed;
            left > 0) {
            Serial.printf(
                "Gesture task: waiting for %ld milliseconds before returning to time display.\n",
                left);
            vTaskDelay(pdMS_TO_TICKS(left));
        }
        if (xSemaphoreTake(display_data_sem, portMAX_DELAY) == pdTRUE) {
            current_page = DisplayPage::Time;
            display_time(time_data, parola_display);
            xSemaphoreGive(display_data_sem);
        }
        sensor->clearInterrupt();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void display_forecast_chart() {
    parola_display.displayClear();
    parola_display.setTextAlignment(PA_LEFT);
    parola_display.printf("%d:", int(round(forecast_data.hourly_temps[0])));
    auto columns_bits = forecast_to_columns<FORECAST_HOURS>(forecast_data);
    for (int i = 0; i < 16 && i < FORECAST_HOURS && i < MATRIX_WIDTH; i++) {
        parola_display.getGraphicObject()->setColumn(FORECAST_HOURS - 1 - i,
                                                     reverse_bits_compact(columns_bits[i]));
    }
}

void ntpUpdateTask(void* pvParameters) {
    while (wifi_enabled) {
        vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000)); // sync with NTP servers once an hour
        timeClient.update();                       // Non-blocking sync
    }
    vTaskDelete(NULL);
}

void screenUpdateTask(void* pvParameters) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void prepareMatrixDisplay(MD_Parola& display) {
    display.begin();
    display.displayClear();
    display.setIntensity(DISPLAY_BRIGHTNESS);
    display.setTextAlignment(PA_CENTER);
}

void setup() {
    initSerial();

    reboot_control::handleRebootStormDetection();

    gpio_install_isr_service(ESP_INTR_FLAG_SHARED);

    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
    Serial.println("I2C initialized.");

    wifi_enabled = net_utils::setup_wifi();
    if (wifi_enabled) {
        net_utils::setup_NTP(timeClient);
        initForecastUpdate();
    }

    // Setup APDS9960 gesture sensor
    gestures_enabled = setupAPDS9960(apds, APDS_INT_PIN, gpio_isr_handler);

    // Power saving
    btStop(); // disables Bluetooth
    setCpuFrequencyMhz(80);

    prepareMatrixDisplay(parola_display);

    xTaskCreate(ntpUpdateTask, "Sync Time", 2048, nullptr, tskIDLE_PRIORITY, nullptr);
    xTaskCreate(minuteChangeTask, "Minute Change", 4096, nullptr, 1, nullptr);
    xTaskCreate(printStatusTask, "Print Status", 4096, nullptr, tskIDLE_PRIORITY, nullptr);
    xTaskCreate(screenUpdateTask, "Print Status", 4096, nullptr, tskIDLE_PRIORITY, nullptr);
    if (gestures_enabled)
        xTaskCreate(gestureTask, "gestureTask", 4096, &apds, 5, &gestureTaskHandle);
#ifdef DEBUG_MEM
    xTaskCreatePinnedToCore(heap_monitor_task, "heapMon", 4096, nullptr, tskIDLE_PRIORITY + 1,
                            nullptr, tskNO_AFFINITY);
#endif
}

void display_seconds(int current_second) {
    // Display the current second as a point on the last row
    static int last_column = current_second;
    int max_columns = parola_display.getGraphicObject()->getColumnCount();
    int current_second_column = max_columns - 1 - map(current_second, 0, 59, 0, max_columns - 1);
    if (current_second_column != last_column) {
        parola_display.getGraphicObject()->setPoint(ROW_SIZE - 1, current_second_column, true);
        parola_display.getGraphicObject()->setPoint(ROW_SIZE - 1, last_column, false);
        last_column = current_second_column;
    }
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));

    struct tm currentTime = get_local_time();
    int currentSecond = currentTime.tm_sec;
    if (current_page == DisplayPage::Time)
        display_seconds(currentSecond);
    vTaskDelay(pdMS_TO_TICKS(1000));
}
