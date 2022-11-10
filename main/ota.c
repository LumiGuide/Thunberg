#include "ota.h"

#include <esp_err.h>
#include <esp_event.h>
#include <esp_http_client.h>
#include <esp_http_server.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <string.h>
#include <cJSON.h>

static const char *TAG = "ota-update";

ota_status_t ota_status = OTA_STATUS_INACTIVE;
esp_err_t ota_error = ESP_OK;

void ota_task(void *ota_url_buf)
{
  ESP_LOGI(TAG, "Starting OTA");

  esp_http_client_config_t http_config = {
      .url = (char *)ota_url_buf,
      // .event_handler = _http_event_handler,
      .skip_cert_common_name_check = true,
      // .buffer_size = 1024,
  };

  esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

  ota_status = OTA_STATUS_DOWNLOADING;

  ota_error = esp_https_ota(&ota_config);
  if (ota_error == ESP_OK)
  {
    ota_status = OTA_STATUS_WAITING_FOR_REBOOT;
    free(ota_url_buf);
    ESP_LOGI(TAG, "OTA finished, restarting in 3.. 2.. 1..");
    esp_restart();
  }
  else
  {
    ota_status = OTA_STATUS_FAILED;
    ESP_LOGE(TAG, "OTA failed: %d", ota_error);
  }

  free(ota_url_buf);
  vTaskDelete(NULL);
}

void ota_init(void)
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_img_state;
  if (esp_ota_get_state_partition(running, &ota_img_state) == ESP_OK)
  {
    if (ota_img_state == ESP_OTA_IMG_PENDING_VERIFY)
    {
      ota_status = OTA_STATUS_WAITING_FOR_COMMIT;
      ESP_LOGI(TAG, "OTA pending verification, please commit");
    }
  }
}