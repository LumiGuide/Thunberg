#pragma once

#include <esp_event.h>
#include "circular_buffer.h"
#include <esp_http_server.h>

enum mode_t{automode, cool, dry, fan, heat};
enum status_t{normal, powerful, economy};
enum strength_t{autostrength, high, med, low, quiet};

static const char *from_enum_mode[] = {"auto", "cool", "dry", "fan", "heat"};
static const char *from_enum_status[] = {"normal", "powerful", "economy"};
static const char *from_enum_strength[] = {"auto", "high", "med", "low", "quiet"};

struct signal_settings {
    uint8_t temp; //18-30
    enum mode_t mode; //AUTO/COOL/DRY/FAN/HEAT
    enum strength_t strength; //AUTO/HIGH/MED/LOW/QUIET
    enum status_t status; //NORMAL/POWERFUL/ECONOMY/OFF
    bool turnoff;
    struct tm time;
    struct tm sync;
};

struct usercontext {
        struct circ_buf* buffer;
        pthread_mutex_t lock;
        struct signal_settings send_settings;
        time_t powerful_time;
};

struct server_t {
    struct circ_buf* buffer;
    char* rest_context;
    struct usercontext* sendthis_context;
};

struct server_ctx_t {
	httpd_handle_t server;
	struct server_t webserver;
};

void spiff_config (void);
void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void configure_led(void);