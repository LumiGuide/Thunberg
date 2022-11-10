#include "sntp.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "http.h"

static const char *TAG = "sntp";
#define INET6_ADDRSTRLEN 48

struct sync_t syncdata = {
    .time = NULL,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

static void initialize_sntp(void);
void time_sync_notification_cb(struct timeval *tv);

void realtime_checker(void *arg)
{
    initialize_sntp();

    struct usercontext *sendthiscontext = (struct usercontext *) arg;
    bool auto_off = false;

	time_t now;
    struct tm timeinfo;

    // Set timezone
    setenv("TZ", "UTC-1UTC-2,M3.5.0/2,M10.5.0/2", 1);
    tzset();

    while(1)
    {
        time(&now);
        localtime_r(&now, &timeinfo);

        if(auto_off == false && timeinfo.tm_hour > 18)
        {
            pthread_mutex_lock(&sendthiscontext->lock);
            sendthiscontext->send_settings.turnoff = true;
            pthread_mutex_unlock(&sendthiscontext->lock);
            buffer_push((struct circ_buf*) sendthiscontext->buffer, &sendthiscontext->send_settings);
            auto_off = true;
        }
        if (auto_off == true && timeinfo.tm_hour < 19)
        {
            auto_off = false;
        }
        sleep(900); // 900 = kwartier
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    /* LWIP_DHCP_GET_NTP_SRV && (SNTP_MAX_SERVERS > 1) */
    // otherwise, use DNS address from a pool
    sntp_setservername(0, CONFIG_SNTP_TIME_SERVER);
    sntp_setservername(1, "pool.ntp.org");     // set the secondary NTP server (will be used only if SNTP_MAX_SERVERS > 1)

    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

    sntp_init();

    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (sntp_getservername(i)){
            ESP_LOGI(TAG, "server %d: %s", i, sntp_getservername(i));
        } else {
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }

    if(syncdata.time == NULL)
    {
        pthread_mutex_lock(&syncdata.lock);
        syncdata.time = (struct tm *) malloc(sizeof(struct tm));
        pthread_mutex_unlock(&syncdata.lock);
    }

    time_t now;
    time(&now);
    pthread_mutex_lock(&syncdata.lock);
    localtime_r(&now, syncdata.time);
    pthread_mutex_unlock(&syncdata.lock);
}