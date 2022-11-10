/* written for LED SK6812 a.k.a. built-in LED of ESP32 stamp pico:
https://cdn-shop.adafruit.com/product-files/1138/SK6812+LED+datasheet+.pdf*/

#pragma once

#include <stdint.h>

void set_led(uint8_t red, uint8_t green, uint8_t blue);