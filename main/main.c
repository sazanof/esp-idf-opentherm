/**
* @package     Opentherm library for ESP-IDF framework - EXAMPLE
* @author:     Mikhail Sazanof
* @copyright   Copyright (C) 2024 - Sazanof.ru
* @licence     MIT
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "opentherm.h"
#include "esp_log.h"
#include "esp_err.h"

#define GPIO_OT_IN 22
#define GPIO_OT_OUT 23
#define ESP_INTR_FLAG_DEFAULT 0

volatile float dhwTemp = 0;
volatile float chTemp = 0;
volatile bool fault = false;
static int targetDHWTemp = 59;
static int targetCHTemp = 60;

static const char *T = "OT";

static void IRAM_ATTR esp_ot_process_response_callback(unsigned long response, open_therm_response_status_t esp_ot_response_status)
{
    // ESP_DRAM_LOGW(T, "Response from esp_ot_process_responseCallback!");
    // ESP_DRAM_LOGW(T, "var response From CB: %lu", response);
    // ESP_DRAM_LOGW(T, "var esp_ot_response_status from CB: %d", (int)esp_ot_response_status);
}

void esp_ot_control_task_handler(void *pvParameter)
{
    while (1)
    {
        unsigned long status = esp_ot_set_boiler_status(false, true, false, false, false);

        ESP_LOGI(T, "====== OPENTHERM =====");
        ESP_LOGI(T, "Free heap size before: %ld", esp_get_free_heap_size());
        open_therm_response_status_t esp_ot_response_status = esp_ot_get_last_response_status();
        if (esp_ot_response_status == OT_STATUS_SUCCESS)
        {
            ESP_LOGI(T, "Central Heating: %s", esp_ot_is_central_heating_active(status) ? "ON" : "OFF");
            ESP_LOGI(T, "Hot Water: %s", esp_ot_is_hot_water_active(status) ? "ON" : "OFF");
            ESP_LOGI(T, "Flame: %s", esp_ot_is_flame_on(status) ? "ON" : "OFF");
            fault = esp_ot_is_fault(status);
            ESP_LOGI(T, "Fault: %s", fault ? "YES" : "NO");
            if (fault)
            {
                ot_reset();
            }
            esp_ot_set_boiler_temperature(targetCHTemp);
            ESP_LOGI(T, "Set CH Temp to: %i", targetCHTemp);

            esp_ot_set_dhw_setpoint(targetDHWTemp);
            ESP_LOGI(T, "Set DHW Temp to: %i", targetDHWTemp);

            dhwTemp = esp_ot_get_dhw_temperature();
            ESP_LOGI(T, "DHW Temp: %.1f", dhwTemp);

            chTemp = esp_ot_get_boiler_temperature();
            ESP_LOGI(T, "CH Temp: %.1f", chTemp);

            float pressure = esp_ot_get_pressure();
            ESP_LOGI(T, "Slave OT Version: %.1f", pressure);

            unsigned long slaveProductVersion = esp_ot_get_slave_product_version();
            ESP_LOGI(T, "Slave Version: %08lX", slaveProductVersion);

            float slaveOTVersion = esp_ot_get_slave_ot_version();
            ESP_LOGI(T, "Slave OT Version: %.1f", slaveOTVersion);
        }
        else if (esp_ot_response_status == OT_STATUS_TIMEOUT)
        {
            ESP_LOGE(T, "OT Communication Timeout");
        }
        else if (esp_ot_response_status == OT_STATUS_INVALID)
        {
            ESP_LOGE(T, "OT Communication Invalid");
        }
        else if (esp_ot_response_status == OT_STATUS_NONE)
        {
            ESP_LOGE(T, "OpenTherm not initialized");
        }

        if (fault)
        {
            ESP_LOGE(T, "Fault Code: %i", esp_ot_get_fault());
        }
        ESP_LOGI(T, "Free heap size after: %ld", esp_get_free_heap_size());
        ESP_LOGI(T, "====== OPENTHERM =====\r\n\r\n");

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    esp_ot_init(
        GPIO_OT_IN,
        GPIO_OT_OUT,
        false,
        esp_ot_process_response_callback);

    xTaskCreate(esp_ot_control_task_handler, T, configMINIMAL_STACK_SIZE * 4, NULL, 3, NULL);
    vTaskSuspend(NULL);
}
