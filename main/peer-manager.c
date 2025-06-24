
#include "peer-manager.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "peer.h"

static const char *TAG = "peer-manager";

static const char *CONFIG_SIGNALING_URL =
    "mqtts://libpeer.com/public/striped-lazy-eagle";
extern char *peer_connection_get_local_sdp(PeerConnection *pc);

SemaphoreHandle_t xSemaphore = NULL;
PeerConnection *g_pc = NULL;
PeerConnectionState eState = PEER_CONNECTION_CLOSED;
int gDataChannelOpened = 0;

int64_t get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL + tv.tv_usec / 1000LL);
}

static void oniceconnectionstatechange(PeerConnectionState state,
    void *user_data) {
    ESP_LOGI(TAG, "PeerConnectionState: %d", state);
    eState = state;
    if (state != PEER_CONNECTION_COMPLETED) {
        gDataChannelOpened = 0;
    }
}

static void onmessage(char *msg, size_t len, void *userdata, uint16_t sid) {
    ESP_LOGI(TAG, "Datachannel message: %.*s", len, msg);
}

static void onopen(void *userdata) {
    ESP_LOGI(TAG, "Datachannel opened");
    gDataChannelOpened = 1;
}

static void signaling_task(void *arg) {
    ESP_LOGI(TAG, "Signaling task started");
    peer_signaling_connect(CONFIG_SIGNALING_URL, "", g_pc);
    for (;;) {
        peer_signaling_loop(g_pc);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void connection_task(void *arg) {
    ESP_LOGI(TAG, "Connection task started");
    peer_connection_create_offer(g_pc);

    for (;;) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
            peer_connection_loop(g_pc);
            xSemaphoreGive(xSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

static void send_task(void *arg) {
    while (!gDataChannelOpened) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    const char *msg = "pong";
    size_t len = strlen(msg);

    ESP_LOGI(TAG, "Starting send_task – sending \"%s\" every second", msg);

    for (;;) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY)) {
            peer_connection_datachannel_send(g_pc, (char *)msg, len);
            xSemaphoreGive(xSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void on_local_sdp(char *sdp, void *ud) {
    char *sdp_copy = strdup(sdp);
    if (!sdp_copy) {
        ESP_LOGW(TAG, "Could not strdup SDP");
        return;
    }

    char sdpMid[32] = {0};
    char *mid_line = strstr(sdp_copy, "a=mid:");
    if (mid_line) {
        mid_line += strlen("a=mid:");
        char *eol = strpbrk(mid_line, "\r\n");
        if (eol) *eol = '\0';
        strncpy(sdpMid, mid_line, sizeof(sdpMid) - 1);
    } else {
        strcpy(sdpMid, "datachannel");
    }

    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char real_ip[16];
        sprintf(real_ip, IPSTR, IP2STR(&ip_info.ip));

        size_t len = strlen(sdp_copy) + 1;
        char *fixed = malloc(len);
        if (!fixed) {
            ESP_LOGW(TAG, "Failed to malloc for SDP fix");
            free(sdp_copy);
            return;
        }
        memcpy(fixed, sdp_copy, len);

        char *p = strstr(fixed, "127.0.0.1");
        while (p) {
            size_t tail = strlen(p + 9) + 1;
            memmove(p + strlen(real_ip), p + 9, tail);
            memcpy(p, real_ip, strlen(real_ip));
            p = strstr(p + strlen(real_ip), "127.0.0.1");
        }
        // strip out " raddr 0.0.0.0 rport 0"
        const char *bad = " raddr 0.0.0.0 rport 0";
        char *q = strstr(fixed, bad);
        if (q) {
            memmove(q, q + strlen(bad), strlen(q + strlen(bad)) + 1);
        }

        ESP_LOGI(TAG, "=== Fixed Local SDP Offer ===\n%s", fixed);
        char *fp = strstr(fixed, "a=fingerprint:");
        if (fp) {
            char *nl = strpbrk(fp, "\r\n");
            if (nl) *nl = '\0';
            ESP_LOGI(TAG, "DTLS-SRTP fingerprint: %s", fp);
        }

        char *line = strtok(fixed, "\r\n");
        while (line) {
            if (strncmp(line, "a=candidate:", 12) == 0) {
                ESP_LOGI(TAG,
                    "Candidate JSON: { \"candidate\": \"%s\", \"sdpMid\": "
                    "\"%s\", \"sdpMLineIndex\": 0 }",
                    line,
                    sdpMid);
            }
            line = strtok(NULL, "\r\n");
        }

        free(fixed);
    } else {
        ESP_LOGW(TAG, "Could not get local IP; printing raw SDP");
        ESP_LOGI(TAG, "=== Raw SDP Offer ===\n%s", sdp_copy);
    }

    free(sdp_copy);
}

void peer_manager_init(const char *ssid, const char *password) {
    xSemaphore = xSemaphoreCreateMutex();

    PeerConfiguration config = {
        .ice_servers = {{.urls = "stun:stun.l.google.com:19302"}},
        .datachannel = DATA_CHANNEL_STRING,
        .audio_codec = CODEC_NONE,
        .video_codec = CODEC_NONE,
        .user_data = NULL,
    };
    ESP_LOGI(TAG,
        "Initializing libpeer with ICE server: %s",
        config.ice_servers[0].urls);

    peer_init();
    g_pc = peer_connection_create(&config);
    if (!g_pc) {
        ESP_LOGE(TAG, "peer_connection_create failed");
        return;
    }

    peer_connection_oniceconnectionstatechange(g_pc,
        oniceconnectionstatechange);
    peer_connection_ondatachannel(g_pc, onmessage, onopen, NULL);

    peer_connection_onicecandidate(g_pc, on_local_sdp);

    if (xTaskCreate(connection_task, "conn", 8 * 1024, NULL, 5, NULL) !=
        pdPASS) {
        ESP_LOGW(TAG, "Failed to create connection task");
    }
    if (xTaskCreate(signaling_task, "sig", 8 * 1024, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Failed to create signaling task");
    }
    if (xTaskCreate(send_task, "send", 4 * 1024, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Failed to create send task");
    }
    ESP_LOGI(TAG, "Peer manager initialized");
}
