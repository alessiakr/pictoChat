#include <stdint.h>
#include <string.h>
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "dns_server.h"

static const char *TAG = "dns";
static uint32_t s_ip_be;          // our IP, network byte order

static uint16_t get16(const uint8_t *p) { 
    return (p[0] << 8) | p[1]; }
static void     put16(uint8_t *p, uint16_t v) { 
    p[0] = v >> 8; p[1] = v & 0xFF; }

// Returns reply length in bytes, or -1 if we should ignore the packet.
static int build_reply(const uint8_t *rx, int n, uint8_t *tx, int txsize){
    if (n < 12) return -1;  // too short for a DNS header
    if (get16(rx + 4) != 1) return -1;

    int pos = 12;  // skip the header
    while (pos < n && rx[pos] != 0) pos += rx[pos] + 1;
    if (pos >= n) return -1;
    pos++;  // skip the zero byte
    if (pos + 4 > n) return -1;
    uint16_t qtype = get16(rx + pos);
    pos += 4;  // skip qtype and qclass
    if (pos + 16 > txsize) return -1;  // room for the answer
    memcpy(tx, rx, pos);  // copy header + question
    put16(tx + 2, 0x8180);  // flags: QR=1, AA=1, RCODE=0
    if (qtype == 1) {  // A record
        put16(tx + 6, 1);  // ancount = 1
        put16(tx + 8, 0);  // nscount = 0
        put16(tx + 10, 0); // arcount = 0
        // append the answer at tx + pos:
        tx[pos++] = 0xC0; tx[pos++] = 0x0C; // name: pointer to offset 12
        put16(tx + pos, 1); pos += 2;         // type A
        put16(tx + pos, 1); pos += 2;         // class IN
        put16(tx + pos, 60); pos += 4;        // TTL = 60 seconds
        put16(tx + pos, 4); pos += 2;         // RDLENGTH = 4 bytes
        memcpy(tx + pos, &s_ip_be, 4); pos += 4; // RDATA = our IP
    } else {
        put16(tx + 6, 0);   // ancount = 0
        put16(tx + 8, 0);   // nscount = 0
        put16(tx + 10, 0);  // arcount = 0
    }
    while (pos < n && rx[pos] != 0) {
        pos += rx[pos] + 1;
    }
    if (pos < n) {
        pos++;  // skip the zero byte
    } else {
        return -1;  // malformed packet
    }

    if (pos + 4 <= n) {
        qtype = get16(rx + pos);
        pos += 4;
    } else {
        return -1;  // malformed packet
    }
    
    if (pos + 16 <= txsize) {
        // room for the answer
    } else {
        return -1;  // not enough space
    }
    
    memcpy(tx, rx, pos);
    
    put16(tx + 2, 0x8180);  // flags: QR=1, AA=1, RCODE=0
    if (qtype == 1) {  // A record
        put16(tx + 6, 1);  // ancount = 1
        put16(tx + 8, 0);  // nscount = 0
        put16(tx + 10, 0); // arcount = 0
        tx[pos++] = 0xC0; tx[pos++] = 0x0C; // name: pointer to offset 12
        put16(tx + pos, 1); pos += 2;         // type A
        put16(tx + pos, 1); pos += 2;         // class IN
        put16(tx + pos, 60); pos += 4;        // TTL = 60 seconds
        put16(tx + pos, 4); pos += 2;         // RDLENGTH = 4 bytes
        memcpy(tx + pos, &s_ip_be, 4); pos += 4; // RDATA = our IP
    } else {
        put16(tx + 6, 0);   // ancount = 0
        put16(tx + 8, 0);   // nscount = 0
        put16(tx + 10, 0);  // arcount = 0
    }
    return pos;
}

static void dns_task(void *arg)
{
    static uint8_t rx[512], tx[512];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket failed: %s", strerror(errno));
        vTaskDelete(NULL);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        ESP_LOGE(TAG, "bind failed: %s", strerror(errno));
        close(sock);
        vTaskDelete(NULL);
    }
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
        ESP_LOGE(TAG, "bind failed: %s", strerror(errno));
        close(sock);
        vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "DNS listening on :53");

    for (;;) {
        struct sockaddr_in src;
        socklen_t srclen = sizeof src;
        int n = recvfrom(sock, rx, sizeof rx, 0,
                         (struct sockaddr *)&src, &srclen);
        if (n < 0) continue;
        int len = build_reply(rx, n, tx, sizeof tx);
        if (len < 0) sendto(sock, tx, len, 0, (struct sockaddr *)&src, srclen);
}

void dns_server_start(uint32_t ip_be)
{
    s_ip_be = ip_be;
    xTaskCreate(dns_task, "dns", 4096, NULL, 5, NULL);
}