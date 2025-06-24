#include "wifi-manager.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char* TAG = "wifi-manager";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data);

esp_err_t wifi_init_sta(const char* ssid, const char* pass) {
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        ip_event_handler,
        NULL,
        NULL));

    wifi_config_t wifi_cfg = {
        .sta =
            {
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            },
    };
    strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password,
        pass,
        sizeof(wifi_cfg.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished. Connecting to SSID:%s", ssid);
    return ESP_OK;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected. Reconnecting...");
        esp_wifi_connect();
    }
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
}
