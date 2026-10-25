#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"

#define AP_SSID         "ShotTimer-Mess"
#define AP_PASS         "shoot1234"
#define AP_CHANNEL      6
#define TX_POWER        40      // Einheit 0,25 dBm -> 10 dBm, reicht für wenige Meter
#define RING_LINES      100
#define LINE_LEN        112
#define MAX_PER_POLL    25

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static SemaphoreHandle_t s_lock;
static char s_ring[RING_LINES][LINE_LEN];
static uint32_t s_head;                 // Anzahl bisher veröffentlichter Zeilen
static char s_level[LINE_LEN];
static char s_info[LINE_LEN];
static web_cmd_fn s_on_cmd;

void web_publish(const char *line)
{
    if (s_lock == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_ring[s_head % RING_LINES], line, LINE_LEN);
    s_head++;
    xSemaphoreGive(s_lock);
}

void web_set_level(const char *line)
{
    if (s_lock == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_level, line, LINE_LEN);
    xSemaphoreGive(s_lock);
}

void web_set_info(const char *text)
{
    if (s_lock == NULL) {
        return;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    strlcpy(s_info, text, LINE_LEN);
    xSemaphoreGive(s_lock);
}

// JSON-String ohne Sonderbehandlung außer " und \ (Firmware-Zeilen enthalten nur ASCII)
static void json_str(char *dst, size_t len, const char *src)
{
    size_t k = 0;
    dst[k++] = '"';
    for (; *src && k < len - 3; src++) {
        if (*src == '"' || *src == '\\') {
            dst[k++] = '\\';
        }
        dst[k++] = (*src >= 0x20) ? *src : ' ';
    }
    dst[k++] = '"';
    dst[k] = '\0';
}

static esp_err_t get_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start);
}

static esp_err_t get_lines(httpd_req_t *req)
{
    uint32_t since = 0;
    char query[32], val[16];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
        httpd_query_key_value(query, "since", val, sizeof(val)) == ESP_OK) {
        since = strtoul(val, NULL, 10);
    }

    static char lines[MAX_PER_POLL][LINE_LEN];
    char level[LINE_LEN], info[LINE_LEN];
    uint32_t first, next;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    next = s_head;
    first = since;
    if (first > next) {
        first = 0;                              // Timer neu gestartet
    }
    if (next - first > RING_LINES) {
        first = next - RING_LINES;
    }
    if (next - first > MAX_PER_POLL) {
        next = first + MAX_PER_POLL;            // Rest beim nächsten Abruf
    }
    for (uint32_t k = first; k < next; k++) {
        strlcpy(lines[k - first], s_ring[k % RING_LINES], LINE_LEN);
    }
    strlcpy(level, s_level, sizeof(level));
    strlcpy(info, s_info, sizeof(info));
    xSemaphoreGive(s_lock);

    char buf[2 * LINE_LEN + 32];
    httpd_resp_set_type(req, "application/json");
    snprintf(buf, sizeof(buf), "{\"next\":%lu,\"level\":", (unsigned long)next);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    json_str(buf, sizeof(buf), level);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, ",\"info\":", HTTPD_RESP_USE_STRLEN);
    json_str(buf, sizeof(buf), info);
    httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send_chunk(req, ",\"lines\":[", HTTPD_RESP_USE_STRLEN);
    for (uint32_t k = first; k < next; k++) {
        if (k > first) {
            httpd_resp_send_chunk(req, ",", 1);
        }
        json_str(buf, sizeof(buf), lines[k - first]);
        httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send_chunk(req, "]}", HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t post_cmd(httpd_req_t *req)
{
    char line[64];
    int n = httpd_req_recv(req, line, sizeof(line) - 1);
    if (n <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "leer");
    }
    line[n] = '\0';
    line[strcspn(line, "\r\n")] = '\0';
    s_on_cmd(line);
    return httpd_resp_sendstr(req, "ok");
}

static void write_http(const char *data, void *ctx)
{
    httpd_resp_send_chunk((httpd_req_t *)ctx, data, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_dump(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"shottimer_dump.txt\"");
    store_dump_to(write_http, req);
    return httpd_resp_send_chunk(req, NULL, 0);
}

bool web_start(web_cmd_fn on_cmd)
{
    s_on_cmd = on_cmd;
    s_lock = xSemaphoreCreateMutex();

    if (esp_netif_init() != ESP_OK || esp_event_loop_create_default() != ESP_OK) {
        return false;
    }
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&cfg) != ESP_OK) {
        return false;
    }
    wifi_config_t ap = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof(AP_SSID) - 1,
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = 2,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (esp_wifi_start() != ESP_OK) {
        return false;
    }
    esp_wifi_set_max_tx_power(TX_POWER);

    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.stack_size = 6144;
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &hcfg) != ESP_OK) {
        return false;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/", .method = HTTP_GET, .handler = get_index },
        { .uri = "/api/lines", .method = HTTP_GET, .handler = get_lines },
        { .uri = "/api/cmd", .method = HTTP_POST, .handler = post_cmd },
        { .uri = "/api/dump", .method = HTTP_GET, .handler = get_dump },
    };
    for (size_t k = 0; k < sizeof(uris) / sizeof(uris[0]); k++) {
        httpd_register_uri_handler(server, &uris[k]);
    }
    return true;
}
