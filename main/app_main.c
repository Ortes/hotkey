#include <esp_log.h>
#include <string.h>
#include <esp_https_ota.h>
#include <mdns.h>
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "hotkey";

extern const uint8_t cert_pem_start[] asm("_binary_firmware_crt_start");
extern const uint8_t cert_pem_end[]   asm("_binary_firmware_crt_end");

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    wifi_event_ap_staconnected_t *event_connected;
    wifi_event_ap_stadisconnected_t *event_disconnected;
    switch (event_id) {
        case WIFI_EVENT_AP_STACONNECTED:
            event_connected = (wifi_event_ap_staconnected_t *) event_data;
            ESP_LOGI(TAG, "station "MACSTR" join, AID=%d", MAC2STR(event_connected->mac), event_connected->aid);

            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            event_disconnected = (wifi_event_ap_stadisconnected_t *) event_data;
            ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d", MAC2STR(event_disconnected->mac), event_disconnected->aid);
            break;
        default:
            break;
    }
}

void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
            .ap = {
                    .ssid = "YOLO",
                    .ssid_len = strlen("YOLO"),
                    .channel = 1,
                    .password = "12345678",
                    .max_connection = 5,
                    .authmode = WIFI_AUTH_WPA2_PSK
            },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void ota_task(void *args) {
    ESP_ERROR_CHECK(mdns_init());

    vTaskDelay(10000 / portTICK_PERIOD_MS);
    struct esp_ip4_addr addr;
    esp_err_t ret = mdns_query_a("firmware", 1000, &addr);
    mdns_free();

    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No server OTA found");
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Firmware server found at "IPSTR, IP2STR(&addr));
    char url[64];
    sprintf(url, "https://"IPSTR":34271/firmware", IP2STR(&addr));

    esp_http_client_config_t config = {
            .url = url,
            .cert_pem = (char *) cert_pem_start,
            .skip_cert_common_name_check = true
    };

    ESP_LOGI(TAG, "start OTA");
    ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Firmware upgrade success rebooting");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
        vTaskDelete(NULL);
    }
}

void app_main(void) {
    init_nvs();
    wifi_init_softap();

    xTaskCreate(ota_task, "ota_task", 4096, NULL, 10, NULL);

    vTaskSuspend(NULL);
}