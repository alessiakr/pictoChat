#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "wifi_ap/wifi_ap.h"
#include "chat/chat.h"
#include "web/web.h"
#include "dns_server/dns_server.h"

static const char *TAG = "main";

void app_main(void) {
    wifi_ap_start();
}
