#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "http.h"

// LEDC TIMER SETUP
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_HIGH_SPEED_MODE
#define LEDC_OUTPUT_IO          (18) // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT // Set duty resolution to 10 bits
#define LEDC_DUTYON             (255) // (2 ** 13) - 1) 
#define LEDC_DUTY50             (127) // Set duty to x%. ((2 ** bits) - 1) * x% = ..
#define LEDC_DUTYOFF            (0) // Set duty to x%. ((2 ** bits) - 1) * x% = ..
#define LEDC_FREQUENCY          (38000) // Frequency in Hertz. Set frequency at 38 kHz

void IR_sender(void* rowan);
void send_signal(struct signal_settings settings, uint8_t* signal);
uint8_t checksum(uint8_t base, uint8_t* signal, size_t len);
void send_inverted(uint8_t* signal, size_t length);
void ir_led(bool, int);
void ledc_init(void);