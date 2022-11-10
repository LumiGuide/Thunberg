#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"
#include "esp32/rom/uart.h"
#include "circular_buffer.h"
#include "wifi.h"
#include "http.h"
#include "ir_sender.h"
#include "sntp.h"
#include "addressable_led.h"
#include "ota.h"

#define TAG "spiffs"

static TaskHandle_t handle_sender = NULL;

void app_main(void)
{
	set_led(0,0,0);
	esp_timer_early_init();

	//SERVER OPZETTEN
	struct server_ctx_t* serv_ctx = (struct server_ctx_t*)malloc(sizeof(struct server_ctx_t));
	serv_ctx->server = NULL;
	serv_ctx->webserver.buffer = buffer_init(sizeof(struct signal_settings), 3, false);

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	
	// ESP_ERROR_CHECK(init_fs());
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, serv_ctx));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, serv_ctx));

	ESP_ERROR_CHECK(wifi_start_connect());

	// Set the LEDC peripheral configuration
    ledc_init();
    // Set duty to 0%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTYOFF));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    xTaskCreate(&IR_sender, "IR_sender", 2048, (void*) serv_ctx->webserver.buffer, 2, &handle_sender);

	#if CONFIG_AIRCO_HOST
	spiff_config ();
	#endif
	
	ota_init();
}