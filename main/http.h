#pragma once

#include <esp_event.h>
#include "circular_buffer.h"
#include <esp_http_server.h>

enum mode_t{automode, cool, dry, fan, heat};
enum status_t{normal, powerful, economy, off};
enum strength_t{autostrength, high, med, low, quiet};

struct signal_settings {
    uint8_t temp; //18-30 (16-30 bij HEAT)
    enum mode_t mode; //AUTO/COOL/DRY/FAN/HEAT
    enum strength_t strength; //AUTO/HIGH/MED/LOW/QUIET
    enum status_t status; //NORMAL/POWERFUL/ECONOMY/OFF
    bool turnoff;
};

struct server_ctx_t{
	httpd_handle_t server;
	struct circ_buf* buffer;
};

void spiff_config (void);
void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);