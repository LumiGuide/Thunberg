#pragma once

#include <cJSON.h>
#include <esp_http_server.h>

#define OTA_ERR_CREATE_TASK_FAILED 0x9101
#define OTA_URL_SIZE 256

static size_t ota_size = 0;
static esp_err_t ota_error = ESP_OK;

typedef enum {
  // There is no OTA scheduled
  OTA_STATUS_INACTIVE,

  // Trying to connect to firmware provisioning server
  OTA_STATUS_CONNECTING,

  // Downloading firmware
  OTA_STATUS_DOWNLOADING,

  // Download complete, please reboot
  OTA_STATUS_WAITING_FOR_REBOOT,

  // Pending commit
  OTA_STATUS_WAITING_FOR_COMMIT,

  // OTA attempt failed
  OTA_STATUS_FAILED,
} ota_status_t;

extern ota_status_t ota_status;

void lumicam_ota_status(cJSON *root);
void ota_task(void *ota_url_buf);