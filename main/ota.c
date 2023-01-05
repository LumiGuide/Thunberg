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

// #include "events.h"
// #include "watchdog.h"

static const char *TAG = "ota-update";

ota_status_t ota_status = OTA_STATUS_INACTIVE;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id)
  {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    ota_status = OTA_STATUS_DOWNLOADING;
    ota_size += evt->data_len;
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
    break;
  case HTTP_EVENT_REDIRECT:
    //TODO: help ik moest hier iets mee volgens de compiler
    break;
  }
  return ESP_OK;
}

void ota_task(void *ota_url_buf)
{
  ESP_LOGI(TAG, "Starting OTA");

  esp_http_client_config_t http_config = {
      .url = (const char *)ota_url_buf,
      .event_handler = _http_event_handler,
      .skip_cert_common_name_check = true,
      .buffer_size = 1024,
  };

  esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

  ota_error = esp_https_ota(&ota_config);
  if (ota_error == ESP_OK)
  {
    ota_status = OTA_STATUS_WAITING_FOR_REBOOT;
    ESP_LOGI(TAG, "OTA finished, starting in 3.. 2.. 1..");
    esp_restart();
  }
  else
  {
    ota_status = OTA_STATUS_FAILED;
    ESP_LOGE(TAG, "OTA failed: %d", ota_error);
  }

  vTaskDelete(NULL);
}

void lumicam_ota_status(cJSON *root)
{
  switch (ota_status) {
    case OTA_STATUS_INACTIVE:
      cJSON_AddStringToObject(root, "status", "inactive");
      break;
    case OTA_STATUS_CONNECTING:
      cJSON_AddStringToObject(root, "status", "connecting");
      break;
    case OTA_STATUS_DOWNLOADING:
      cJSON_AddStringToObject(root, "status", "downloading");
      cJSON_AddNumberToObject(root, "size", ota_size);
      break;
    case OTA_STATUS_WAITING_FOR_REBOOT:
      cJSON_AddStringToObject(root, "status", "waiting_for_reboot");
      break;
    case OTA_STATUS_WAITING_FOR_COMMIT:
      cJSON_AddStringToObject(root, "status", "waiting_for_commit");
      break;
    case OTA_STATUS_FAILED:
      cJSON_AddStringToObject(root, "status", "failed");
      cJSON_AddNumberToObject(root, "error", ota_error);
      break;
  }
}

//TODO: nodig? zo ja, aanroepen in main
void lumicam_ota_init()
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
