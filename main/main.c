
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"

#define TAG "driveway_motion_main"

int wifi_retry_count = 0;
const int WIFI_CONNECTED_BIT = BIT0;
static EventGroupHandle_t wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
            esp_err_t err;
            char hostname[33] = {0};
            snprintf(hostname, 33, "driveway_motion_sensor");
            ESP_LOGI(TAG, "set hostname to %s", hostname);
            if ((err = tcpip_adapter_set_hostname(WIFI_IF_STA, hostname)) != ESP_OK)
            {
                ESP_LOGE(TAG, "set hostname failed: %s", esp_err_to_name(err));
            }
    }
    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
        wifi_retry_count++;
        ESP_LOGI(TAG, "retry to connect to the AP");
        if (wifi_retry_count > 10)
        {
            ESP_LOGI(TAG, "reboot to many tries");
            fflush(stdout);
            esp_restart();
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_count = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void connect_to_wifi()
{
    wifi_event_group = xEventGroupCreate();

    esp_netif_init();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASSWORD},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", CONFIG_WIFI_SSID,
             CONFIG_WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    const esp_partition_t* current_partition = esp_ota_get_running_partition();
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    ESP_LOGI(TAG, "running partition %s version %s", current_partition->label, app_desc->version);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    connect_to_wifi();

    ESP_LOGI(TAG, "Minimum free heap size: %d bytes", esp_get_minimum_free_heap_size());

    while(1) {
        ESP_LOGI(TAG, "cycle...");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}