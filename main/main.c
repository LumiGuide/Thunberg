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
#define AIRCO_FUJI              1
#if AIRCO_FUJI
#include "fuji.h"
#elif AIRCO_MITSUBISHI
#include "mitsubishi.h"
#endif

#define TAG "spiffs"

static TaskHandle_t handle_sender = NULL;

void app_main(void)
{
	esp_timer_early_init();

	//SERVER OPZETTEN
	static httpd_handle_t server = NULL;

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// ESP_ERROR_CHECK(init_fs());

	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

	ESP_ERROR_CHECK(connect());
	
	// Set the LEDC peripheral configuration
    ledc_init();
    // Set duty to 0%
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTYOFF));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    struct circ_buf* rowan = buffer_init(sizeof(struct signal_settings), 3, false);
    save_struct_location(rowan);

    xTaskCreate(&IR_sender, "IR_sender", 2048, (void*) rowan, 2, &handle_sender);

	spiff_config ();
	
	//VANAF HIER SPIFFS
	// esp_vfs_spiffs_conf_t config = {
	// 	.base_path = "/spiffs",
	// 	.partition_label = NULL,
	// 	.max_files = 5,
	// 	.format_if_mount_failed = true,
	// };
	// esp_err_t ret = esp_vfs_spiffs_register(&config);

	// if (ret != ESP_OK) {
    //     if (ret == ESP_FAIL) {
    //         ESP_LOGE(TAG, "Failed to mount or format filesystem");
    //     } else if (ret == ESP_ERR_NOT_FOUND) {
    //         ESP_LOGE(TAG, "Failed to find SPIFFS partition");
    //     } else {
    //         ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    //     }
    // }

	// size_t total = 0, used = 0;
    // ret = esp_spiffs_info(NULL, &total, &used);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    // } else {
    //     ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    // }

	// FILE *file = fopen("/spiffs/hello.txt", "r");
	// if(file ==NULL)
	// {
	//   ESP_LOGE(TAG,"File does not exist!");
	// }
	// else 
	// {
	// 	char line[256];
	// 	while(fgets(line, sizeof(line), file) != NULL)
	// {
	// 	printf(line);
	// }
	// fclose(file);
	// }
	// esp_vfs_spiffs_unregister(NULL);
}