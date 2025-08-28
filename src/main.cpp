#include "apds9960.h"
#include "config.h"
#include "display_pages.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "forecast.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mem_mon.h"
#include "net_utils.h"
#include "reboot_control.h"
#include "secrets.h"
#include "time_utils.h"

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

void prepareMatrixDisplay(MD_Parola& display);

volatile DisplayPage currentPage = DisplayPage::Time;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0,
                     60 * 60 * 1000); // UTC offset (0), update interval 1h

bool wifiEnabled = false;
bool gestureProcessingEnabled = false;
bool forecastEnabled = true;

MD_Parola parolaDisplay = MD_Parola(DISPLAY_HARDWARE_TYPE, DISPLAY_DATA_PIN, DISPLAY_CLK_PIN,
                                    DISPLAY_CS_PIN, DISPLAY_MAX_DEVICES);

String timeData = "Err time";
String forecastData = "Loading";
SemaphoreHandle_t displayDataSem;

// ----- Gesture Sensor
Adafruit_APDS9960 apds;

void displayTime(String time, MD_Parola& parolaDisplay);

/**
 * @brief FreeRTOS task that runs periodically to update the weather forecast.
 * @param pvParameters Task parameters (not used here).
 */
void weatherUpdateTask(void* pvParameters) {
    Serial.println("Weather update task started.");
    for (;;) { // Infinite loop for the task
        Serial.println("[Task] Fetching new weather forecast...");
        int startHour = getGMTHour();
        StringResult newForecast = getForecast(startHour);
        String newForecastStr;
        if (newForecast.isErr()) {
            Serial.println("[Task] Error fetching forecast: " + newForecast.unwrapErr());
            newForecastStr = "Error";
        } else {
            Serial.println("[Task] Successfully fetched forecast.");
            newForecastStr = newForecast.unwrap();
        }

        if (xSemaphoreTake(displayDataSem, portMAX_DELAY) == pdTRUE) {
            forecastData = newForecastStr;
            xSemaphoreGive(displayDataSem);
            Serial.println("[Task] Forecast updated successfully.");
        }

        // Wait for one hour before the next run
        const int updateIntervalMs = 3600 * 1000; // 1 hour
        vTaskDelay(updateIntervalMs / portTICK_PERIOD_MS);
    }
}

void wait_until_next_minute(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Convert to milliseconds since epoch
    int64_t ms_now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // Compute ms into current minute
    int64_t ms_into_minute = ms_now % 60000;

    // How many ms to next minute
    int64_t ms_to_next_minute = 60000 - ms_into_minute;

    // Convert ms to FreeRTOS ticks (round up to nearest tick)
    TickType_t ticks = pdMS_TO_TICKS(ms_to_next_minute);

    vTaskDelay(ticks);
}

void minuteChangeTask(void* pvParameters) {
    while (1) {
        String tmp = getFormattedLocalTime("%H : %M");
        if (xSemaphoreTake(displayDataSem, portMAX_DELAY) == pdTRUE) {
            timeData = tmp;
            currentPage = DisplayPage::Time; // Switch to time display on minute change
            xSemaphoreGive(displayDataSem);
        }
        displayTime(timeData, parolaDisplay);

        Serial.printf("Minute changed! New time: %s\n", getFormattedLocalTime().c_str());
        wait_until_next_minute();
    }
}

// Task that prints device uptime and local time periodically.
void printStatusTask(void* parameter) {
    while (true) {
        unsigned long currentMillis = millis();
        String uptime = formatMillis(currentMillis);
        String localTime = getFormattedLocalTime();
        Serial.println("⏱️  Device Uptime: " + uptime + " | Real time: " + localTime +
                       " | Forecast: " + forecastData);
        vTaskDelay(pdMS_TO_TICKS(STATUS_UPDATE_INTERVAL_SECONDS * 1000));
    }
}

void prepareMatrixDisplay(MD_Parola& display) {
    display.begin();
    display.displayClear();
    display.setIntensity(DISPLAY_BRIGHTNESS);
    display.setTextAlignment(PA_CENTER);
}

void initForecastUpdate() {
    // Initialize the weather forecast task
    displayDataSem = xSemaphoreCreateMutex();
    if (displayDataSem == NULL) {
        Serial.println("Error: Failed to create mutex.");
        forecastData = "Er:Mutex";
        forecastEnabled = false; // Disable forecast updates
    }
    if (wifiEnabled && forecastEnabled) {
        xTaskCreate(weatherUpdateTask, "WeatherUpdate", 8192, nullptr, 1, nullptr);
    } else {
        Serial.println("Weather updates are disabled.");
    }
}

void handleRebootStormDetection() {
    // Reboot storm detection mechanism.
    RebootControl::rebootCount++;
    if (RebootControl::rebootCount >= RebootControl::MAX_REBOOTS) {
        Serial.println("Reboot storm detected! Entering deep sleep for " +
                       String(RebootControl::SLEEP_TIME_ON_STORM_US) + " ms");
        esp_sleep_enable_timer_wakeup(RebootControl::SLEEP_TIME_ON_STORM_US);
        esp_deep_sleep_start();
    }
    // Monitor uptime to reset reboot counter after stable runtime.
    xTaskCreate(RebootControl::resetRebootCounterTask, "ResetRebootCounter", 2048, NULL, 1, NULL);
}

void initSerial() {
    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    } // wait for Serial on some boards
}

void displaySeconds(int currentSecond) {
    // Display the current second as a point on the last row
    int column =
        map(currentSecond, 0, 59, 0, parolaDisplay.getGraphicObject()->getColumnCount() - 2);
    parolaDisplay.getGraphicObject()->setPoint(ROW_SIZE - 1, column, false);
    parolaDisplay.getGraphicObject()->setPoint(ROW_SIZE - 1, 1 + column, true);
}

static TaskHandle_t gestureTaskHandle = nullptr;

// --- Gesture Handler Function ---
// This is also decoupled. It just handles the command logic.
void handleGestureCommand(uint8_t gesture) {
    switch (gesture) {
    case APDS9960_UP:
        Serial.println("GESTURE: UP");
        break;
    case APDS9960_DOWN:
        Serial.println("GESTURE: DOWN");
        break;
    case APDS9960_LEFT:
        Serial.println("GESTURE: LEFT");
        break;
    case APDS9960_RIGHT:
        Serial.println("GESTURE: RIGHT");
        break;
    default:
        break;
    }
}

void processProximityInterruption(Adafruit_APDS9960* sensor) {
    uint8_t proximity = sensor->readProximity();
    Serial.printf("Gesture hander: Proximity value: %d\n", proximity);
    if (xSemaphoreTake(displayDataSem, portMAX_DELAY) == pdTRUE) {
        currentPage = DisplayPage::Forecast;
        parolaDisplay.setTextAlignment(PA_LEFT);
        parolaDisplay.printf("%s", forecastData.c_str());
        xSemaphoreGive(displayDataSem);
    }
}

void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(gestureTaskHandle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE)
        portYIELD_FROM_ISR();
}

int getCurrentSecond() {
    int second;
    struct tm currentTime = getLocalTime();
    second = currentTime.tm_sec;
    return second;
}

void gestureTask(void* pvParameters) {
    auto sensor = static_cast<Adafruit_APDS9960*>(pvParameters);
    for (;;) {
        sensor->enableProximityInterrupt();
        Serial.println("Gesture Task: waiting for notification...");
        // Block until notified by ISR. Use ulTaskNotifyTake or semaphore take.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // clear on exit
        Serial.println("Gesture Task: notification detected, processing...");
        sensor->disableProximityInterrupt();
        vTaskDelay(pdMS_TO_TICKS(10));

        int startSecond = getCurrentSecond();
        processProximityInterruption(sensor);

        vTaskDelay(pdMS_TO_TICKS(10));
        Serial.println("Gesture task: waiting for proximity leave...");
        while (sensor->readProximity() > 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        Serial.println("Gesture task: proximity cleared.");
        int endSecond = getCurrentSecond();
        int elapsed = endSecond - startSecond;
        if (int left = FORECAST_DISPLAY_TIME_SECONDS - elapsed; left > 0) {
            Serial.printf("Gesture task: waiting for %d seconds before returning to time display.\n", left);
            vTaskDelay(pdMS_TO_TICKS(left * 1000));
        }
        if (xSemaphoreTake(displayDataSem, portMAX_DELAY) == pdTRUE) {
            currentPage = DisplayPage::Time;
            displayTime(timeData, parolaDisplay);
            xSemaphoreGive(displayDataSem);
        }
        sensor->clearInterrupt();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void ntpUpdateTask(void* pvParameters) {
    while (wifiEnabled) {
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

void setup() {
    initSerial();

    gpio_install_isr_service(0); // installs default ISR service; call once

    handleRebootStormDetection();

    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ_HZ);
    Serial.println("I2C initialized.");

    // Setup APDS9960 gesture sensor
    gestureProcessingEnabled = setupAPDS9960(apds, APDS_INT_PIN, gpio_isr_handler);

    wifiEnabled = net_utils::setupWiFi();
    if (wifiEnabled) {
        net_utils::setupNTP(timeClient);
        initForecastUpdate();
    }

    // Power saving
    btStop(); // disables Bluetooth
    setCpuFrequencyMhz(80);

    prepareMatrixDisplay(parolaDisplay);

    xTaskCreate(ntpUpdateTask, "Sync Time", 2048, nullptr, tskIDLE_PRIORITY, nullptr);
    xTaskCreate(minuteChangeTask, "Minute Change", 4096, nullptr, 1, nullptr);
    xTaskCreate(printStatusTask, "Print Status", 4096, nullptr, tskIDLE_PRIORITY, nullptr);
    xTaskCreate(screenUpdateTask, "Print Status", 4096, nullptr, tskIDLE_PRIORITY, nullptr);
    if (gestureProcessingEnabled)
        xTaskCreate(gestureTask, "gestureTask", 4096, &apds, 5, &gestureTaskHandle);
#ifdef DEBUG_MEM
    xTaskCreatePinnedToCore(heap_monitor_task, "heapMon", 4096, nullptr, tskIDLE_PRIORITY + 1,
                            nullptr, tskNO_AFFINITY);
#endif
}

void loop() {

    vTaskDelay(pdMS_TO_TICKS(1000));

    // if (forecastEnabled && (currentPage == DisplayPage::Forecast ||
    //                         currentSecond % FORECAST_UPDATE_INTERVAL_SECONDS == 3)) {
    //     if (xSemaphoreTake(displayDataSem, portMAX_DELAY) == pdTRUE) {
    //         parolaDisplay.setTextAlignment(PA_LEFT);
    //         parolaDisplay.printf("%s", forecastData.c_str());
    //         xSemaphoreGive(displayDataSem);
    //         displaySeconds(currentSecond);
    //         vTaskDelay(pdMS_TO_TICKS(FORECAST_DISPLAY_TIME_SECONDS * 1000));
    //         currentPage = DisplayPage::Time;
    //     }
    // }

    struct tm currentTime = getLocalTime();
    int currentSecond = currentTime.tm_sec;
    displaySeconds(currentSecond);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void displayTime(String time, MD_Parola& parolaDisplay) {
    parolaDisplay.setTextAlignment(PA_CENTER);
    parolaDisplay.printf("%s", time.c_str());
}
