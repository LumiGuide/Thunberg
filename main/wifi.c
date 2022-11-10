#include "wifi.h"
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include <unistd.h>

static const char *IR_NETIF_DESC_STA = "IR_netif_sta";
static const wifi_scan_method_t IR_WIFI_SCAN_METHOD = WIFI_ALL_CHANNEL_SCAN;
static const wifi_sort_method_t IR_WIFI_CONNECT_AP_SORT_METHOD = WIFI_CONNECT_AP_BY_SIGNAL;
static const int8_t IR_CONFIG_WIFI_SCAN_RSSI_THRESHOLD = -127;
static const wifi_auth_mode_t IR_WIFI_SCAN_AUTH_MODE_THRESHOLD = WIFI_AUTH_MAX;
static const int IR_CONFIG_WIFI_CONN_MAX_RETRY = 6;

static const char *TAG = "connecting wifi";

esp_netif_t *sta_netif = NULL;
int s_retry_num = 0;

void wifi_start(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.if_desc = IR_NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    s_retry_num++;
    if (s_retry_num > IR_CONFIG_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", s_retry_num);
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

/* Checks the netif description if it contains specified prefix.
 * All netifs created within common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module */
bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

esp_netif_t *get_example_netif_from_desc(const char *desc)
{
    esp_netif_t *netif = NULL;
    while ((netif = esp_netif_next(netif)) != NULL) {
        if (strcmp(esp_netif_get_desc(netif), desc) == 0) {
            return netif;
        }
    }
    return netif;
}

void handler_on_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    s_retry_num = 0;
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(IR_NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
}

esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config)
{
    s_retry_num = 0;
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, NULL));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    return ESP_OK;
}

esp_err_t wifi_connect(void)
{
    ESP_LOGI(TAG, "Start connect.");
    wifi_start();
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .scan_method = IR_WIFI_SCAN_METHOD,
            .sort_method = IR_WIFI_CONNECT_AP_SORT_METHOD,
            .threshold.rssi = IR_CONFIG_WIFI_SCAN_RSSI_THRESHOLD,
            .threshold.authmode = IR_WIFI_SCAN_AUTH_MODE_THRESHOLD,
        },
    };

    return wifi_sta_do_connect(wifi_config);
}

esp_err_t wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip));

    return esp_wifi_disconnect();
}

void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif));
    sleep(10);
    esp_netif_destroy(sta_netif);
    sta_netif = NULL;
}

void wifi_shutdown(void)
{
    wifi_sta_do_disconnect();
    wifi_stop();
}

void print_all_netif_ips(const char *prefix)
{
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t *netif = NULL;
    esp_netif_ip_info_t ip;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        netif = esp_netif_next(netif);
        if (is_our_netif(prefix, netif)) {
            ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR ",", IP2STR(&ip.ip));
        }
    }
}

esp_err_t wifi_start_connect(void)
{
    if (wifi_connect() != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_shutdown));

    print_all_netif_ips(IR_NETIF_DESC_STA);

    return ESP_OK;
}