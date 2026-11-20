#include <string.h>
#include <stddef.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "dns_server/dns_server.h"

static const char *TAG = "wifi_ap";

// Callback: ESP-IDF calls this whenever a WiFi event happens.
static void on_wifi_event(void *arg, esp_event_base_t base,
                          int32_t id, void *data)
{
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)data;
        ESP_LOGI(TAG, "joined: " MACSTR, MAC2STR(ev->mac));
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *ev = (wifi_event_ap_stadisconnected_t *)data;
        ESP_LOGI(TAG, "left: " MACSTR, MAC2STR(ev->mac));
    }
}

void wifi_ap_start(void)
{
    //NVS (WiFi stores calibration data in it)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    //Network stack + event system
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *netif = esp_netif_create_default_wifi_ap();

    //WiFi driver
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    //Register the callback
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &on_wifi_event, NULL));

    //Configure the access point
    wifi_config_t cfg;
    memset(&cfg, 0, sizeof cfg);          // zero everything first
    const char *ssid = "PictoChatESP";

    size_t ssid_len = strlen(ssid);
    memcpy(cfg.ap.ssid, ssid, ssid_len);
    cfg.ap.ssid_len = ssid_len;
    cfg.ap.channel = 1;
    cfg.ap.max_connection = 4;
    cfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
    dns_server_start(ip_info.ip.addr);


    ESP_LOGI(TAG, "AP started, SSID: %s", ssid);
}