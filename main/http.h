#pragma once

#include <esp_event.h>
#include "circular_buffer.h"

enum airco_t{software, hardware, design, canteen, ceo, conference, flexwork};
enum mode_t{automode, cool, dry, fan, heat};
enum status_t{normal, powerful, economy, off};
enum strength_t{autostrength, high, med, low, quiet};

struct signal_settings {
    enum airco_t airco; //SOFTWARE/HARDWARE/DESIGN/CANTEEN/CEO/CONFERENCE/FLEXWORK
    uint8_t temp; //18-30 (16-30 bij HEAT)
    enum mode_t mode; //AUTO/COOL/DRY/FAN/HEAT
    enum strength_t strength; //AUTO/HIGH/MED/LOW/QUIET
    enum status_t status; //NORMAL/POWERFUL/ECONOMY/OFF
    bool turnoff;
};

void spiff_config (void);
void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void save_struct_location(struct circ_buf* location);
bool to_enum_airco(char* input, enum airco_t* location);
bool to_enum_mode(char* input, enum mode_t* location);
bool to_enum_status(char* input, enum status_t* location);
bool to_enum_strength(char* input, enum strength_t* location);