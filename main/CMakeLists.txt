idf_component_register(SRCS "main.c" "wifi_ap/wifi_ap.c" "dns_server/dns_server.c" "web/web.c" "chat/chat.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash esp_http_server lwip freertos
                       EMBED_FILES "index.html")