#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "peer-manager.h"
#include "wifi-manager.h"

static const char *TAG = "core";

void app_main(void) {
    ESP_LOGI(TAG, "Initializing libpeer...");
    wifi_init(CONFIG_WIFI_CONNECT_SSID, CONFIG_WIFI_CONNECT_PASSWORD);
    peer_manager_init();
    peer_manager_register_connection_task();
    peer_manager_register_signaling_task();
    peer_manager_register_send_task();
    vTaskDelete(NULL);
}
