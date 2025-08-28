#include "freertos/FreeRTOS.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "config.h"
#include <Wire.h>
#include <HardwareSerial.h>


/////// Currently unused, for diagnostics only ///////


// Gesture sensor config
constexpr uint8_t APDS9960_I2C_ADDR = 0x39;
constexpr uint8_t APDS9960_ID_REG = 0x92;
constexpr uint8_t APDS9960_ID_EXPECTED = 0xAB; // typical chip ID

constexpr TickType_t PER_ADDR_DELAY_MS = 20;

constexpr const char TAG[] = "APDS_DIAG";

void read_adps_regs() {
    uint8_t regs[] = {0x00, 0x01, 0x02, 0x13, 0x14, 0x19, 0x1E, 0x26, 0x27};
    // 0x00 (ID), 0x13 (ENABLE), 0x14 (ATIME/PROX-related), 0x19 (PIE/PDATA?),
    // 0x1E (STATUS), 0x26 (PILT), 0x27 (PIHT)
    for (size_t i = 0; i < sizeof(regs); ++i) {
        Wire.beginTransmission(APDS9960_I2C_ADDR);
        Wire.write(regs[i]);
        Wire.endTransmission(false);
        Wire.requestFrom((int)APDS9960_I2C_ADDR, 1);
        if (Wire.available()) {
            Serial.printf("REG 0x%02X = 0x%02X\n", regs[i], Wire.read());
        } else {
            Serial.printf("REG 0x%02X read failed\n", regs[i]);
        }
    }
}


static esp_err_t i2c_master_init() {
    i2c_config_t conf{};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.scl_io_num = I2C_SCL;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_FREQ_HZ;

    esp_err_t err = i2c_param_config(I2C_PORT, &conf);
    if (err != ESP_OK)
        return err;
    return i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

static esp_err_t i2c_probe_address(i2c_port_t port, uint8_t addr, TickType_t timeout_ms) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t res = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(timeout_ms));
    i2c_cmd_link_delete(cmd);
    return res;
}

static esp_err_t i2c_read_register(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t* out) {
    if (!out)
        return ESP_ERR_INVALID_ARG;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // write register address
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    // repeated start and read one byte
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, out, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t res = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return res;
}

static void i2c_scan_task(void* arg) {
    ESP_LOGI(TAG, "Starting I2C scanner on port %d (SDA=%d, SCL=%d)", I2C_PORT, I2C_SDA,
             I2C_SCL);

    for (int addr = 0x03; addr <= 0x77; ++addr) {
        esp_err_t res = i2c_probe_address(I2C_PORT, static_cast<uint8_t>(addr), 100);
        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);

            if (addr == APDS9960_I2C_ADDR) {
                uint8_t id = 0;
                esp_err_t r = i2c_read_register(I2C_PORT, APDS9960_I2C_ADDR, APDS9960_ID_REG, &id);
                if (r == ESP_OK) {
                    ESP_LOGI(TAG, "APDS9960 ID read: 0x%02X", id);
                    if (id == APDS9960_ID_EXPECTED) {
                        ESP_LOGI(TAG, "APDS9960 detected at 0x%02X", APDS9960_I2C_ADDR);
                    } else {
                        ESP_LOGW(TAG,
                                 "Device at 0x%02X responded but ID 0x%02X != expected 0x%02X",
                                 APDS9960_I2C_ADDR, id, APDS9960_ID_EXPECTED);
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to read ID from 0x%02X (err=%s)",
                             APDS9960_I2C_ADDR, esp_err_to_name(r));
                }
            }
        } else if (res == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Timeout at 0x%02X", addr);
        } else {
            ESP_LOGD(TAG, "No device at 0x%02X (err=%d)", addr, res);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    ESP_LOGI(TAG, "I2C scan complete");
    vTaskDelete(nullptr);
}


static esp_err_t i2c_write_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t val) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg(i2c_port_t port, uint8_t addr, uint8_t reg, uint8_t *out) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, out, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/* High-level APDS helpers */
static esp_err_t apds_read_reg(uint8_t reg, uint8_t *out) {
    return i2c_read_reg(I2C_PORT, APDS9960_I2C_ADDR, reg, out);
}
static esp_err_t apds_write_reg(uint8_t reg, uint8_t val) {
    return i2c_write_reg(I2C_PORT, APDS9960_I2C_ADDR, reg, val);
}
static esp_err_t apds_clear_proximity_interrupt(void) {
    return apds_write_reg(0xE5, 0x00); // PICLEAR
}
static esp_err_t apds_clear_all_interrupts(void) {
    return apds_write_reg(0xE7, 0x00); // AICLEAR
}
static esp_err_t apds_read_status(uint8_t *status) { return apds_read_reg(0x93, status); }
static esp_err_t apds_read_proximity(uint8_t *pdata) { return apds_read_reg(0x9C, pdata); }
static esp_err_t apds_read_enable(uint8_t *en) { return apds_read_reg(0x80, en); }
static esp_err_t apds_write_enable(uint8_t v) { return apds_write_reg(0x80, v); }

/* Generic reg reader for logging */
static void log_reg(const char *name, uint8_t reg) {
    uint8_t v;
    if (apds_read_reg(reg, &v) == ESP_OK) {
        ESP_LOGI(TAG, "%s (0x%02X) = 0x%02X", name, reg, v);
    } else {
        ESP_LOGE(TAG, "Failed read %s (0x%02X)", name, reg);
    }
}

void diagnose_apds(void)
{
    uint8_t status = 0, pdata = 0, en = 0;

    ESP_LOGI(TAG, "=== APDS DIAG START ===");

    // 1) baseline read
    apds_read_status(&status);
    apds_read_proximity(&pdata);
    ESP_LOGI(TAG, "BASELINE STATUS=0x%02X PDATA=%u", status, pdata);
    ESP_LOGI(TAG, "Bits: PGSAT=%d PINT=%d PVALID=%d",
             !!(status & (1<<6)), !!(status & (1<<5)), !!(status & (1<<1)));

    // Log related regs
    log_reg("ENABLE", 0x80);
    log_reg("PILT",   0x89);
    log_reg("PIHT",   0x8B);
    log_reg("PERS",   0x8C);
    log_reg("PPULSE", 0x8E);
    log_reg("CONTROL",0x8F);
    log_reg("CONFIG2",0x90);
    log_reg("STATUS", 0x93);
    log_reg("PDATA",  0x9C);

    // 2) clear proximity interrupt (PICLEAR) and re-read
    ESP_LOGI(TAG, "Clearing proximity interrupt (PICLEAR 0xE5)...");
    apds_clear_proximity_interrupt();
    vTaskDelay(pdMS_TO_TICKS(50));
    apds_read_status(&status);
    apds_read_proximity(&pdata);
    ESP_LOGI(TAG, "AFTER PICLEAR STATUS=0x%02X PDATA=%u", status, pdata);
    ESP_LOGI(TAG, "Bits: PGSAT=%d PINT=%d PVALID=%d",
             !!(status & (1<<6)), !!(status & (1<<5)), !!(status & (1<<1)));

    // 3) disable PIEN (stop hardware interrupt) and check
    if (apds_read_enable(&en) == ESP_OK) {
        ESP_LOGI(TAG, "ENABLE before change = 0x%02X", en);
        uint8_t new_en = en & ~(1 << 5); // clear PIEN
        apds_write_enable(new_en);
        vTaskDelay(pdMS_TO_TICKS(50));
        apds_read_enable(&en);
        ESP_LOGI(TAG, "ENABLE after PIEN clear = 0x%02X", en);
    } else {
        ESP_LOGE(TAG, "Failed to read ENABLE reg");
    }
    apds_read_status(&status);
    apds_read_proximity(&pdata);
    ESP_LOGI(TAG, "PIEN OFF STATUS=0x%02X PDATA=%u", status, pdata);

    // 4) re-enable PIEN after clearing interrupts
    ESP_LOGI(TAG, "Clearing PICLEAR then re-enabling PIEN...");
    apds_clear_proximity_interrupt();
    vTaskDelay(pdMS_TO_TICKS(20));
    if (apds_read_enable(&en) == ESP_OK) {
        uint8_t new_en = en | (1 << 5); // set PIEN
        apds_write_enable(new_en);
        vTaskDelay(pdMS_TO_TICKS(50));
        apds_read_enable(&en);
        ESP_LOGI(TAG, "ENABLE after re-enable PIEN = 0x%02X", en);
    }
    apds_read_status(&status);
    apds_read_proximity(&pdata);
    ESP_LOGI(TAG, "RE-ENABLED STATUS=0x%02X PDATA=%u", status, pdata);
    ESP_LOGI(TAG, "Bits: PGSAT=%d PINT=%d PVALID=%d",
             !!(status & (1<<6)), !!(status & (1<<5)), !!(status & (1<<1)));

    // 5) try AICLEAR if PICLEAR didn't clear
    ESP_LOGI(TAG, "Trying AICLEAR (0xE7)...");
    apds_clear_all_interrupts();
    // vTaskDelay(pdMS_TO_TICKS(50));
    apds_read_status(&status);
    ESP_LOGI(TAG, "AFTER AICLEAR STATUS=0x%02X", status);

    ESP_LOGI(TAG, "=== APDS DIAG END ===");
}

uint8_t readReg(uint8_t r) {
    Wire.beginTransmission(APDS9960_I2C_ADDR);
    Wire.write(r);
    Wire.endTransmission(false);
    Wire.requestFrom(APDS9960_I2C_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}
void writeReg(uint8_t r, uint8_t v) {
    Wire.beginTransmission(APDS9960_I2C_ADDR);
    Wire.write(r);
    Wire.write(v);
    Wire.endTransmission();
}

void printG(uint8_t g) {
    Serial.printf("GCONF4=0x%02X  INTpin=%d\n", g, digitalRead(APDS_INT_PIN));
}


void read_and_print_APDS9960_status() {
    auto status = readReg(0x93);
    Serial.printf("STATUS=0x%02X %s GPIO_%d=%d\n", status, status & 0x20 ? "(PINT)" : "",
                  APDS_INT_PIN, digitalRead(APDS_INT_PIN));
}
