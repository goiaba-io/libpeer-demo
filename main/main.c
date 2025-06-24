#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "peer-manager.h"
#include "wifi-manager.h"

static const char *TAG = "core";
static const char *SSID = "your_ssid";
static const char *PASSWORD = "your_password";

void app_main(void) {
    wifi_init_sta(SSID, PASSWORD);
    ESP_LOGI(TAG, "Initializing libpeer...");
    peer_manager_init();
    vTaskDelete(NULL);
}
