#include "http.h"
#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include "esp_vfs.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "sntp.h"
#include "esp_spiffs.h"
#include "addressable_led.h"
#include "driver/gpio.h"
#include "ota.h"
#include "wifi.h"

static const char *TAG = "setup http";
static const char *REST_TAG = "get html";
#define SCRATCH_BUFSIZE (10240)

static TaskHandle_t handle_realtime = NULL;

bool to_enum_mode(char* input, enum mode_t* location);
bool to_enum_status(char* input, enum status_t* location);
bool to_enum_strength(char* input, enum strength_t* location);

static esp_err_t post_settings_handler(httpd_req_t *req)
{
    struct signal_settings settings_local;
    char buf[256];

    if (req->content_len > sizeof(buf) - 1)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JSON is larger than 255 bytes");
        return ESP_FAIL;
    }

    if (httpd_req_recv(req, buf, req->content_len) != req->content_len)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to import JSON file");
        return ESP_FAIL;
    }
    
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Request contains malformed JSON, can't parse");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "JSON file parsed");

    cJSON *modeObj = cJSON_GetObjectItem(json, "mode");

    if (!cJSON_IsString(modeObj))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Mode is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input mode: %s", modeObj->valuestring);

    if (to_enum_mode(modeObj->valuestring, &settings_local.mode) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID MODE INPUT, TRY AGAIN");
        return ESP_FAIL;
    }

    cJSON *statusObj = cJSON_GetObjectItem(json, "status");
    if (!cJSON_IsString(statusObj))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Status value in JSON is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input status: %s", statusObj->valuestring);
    if (to_enum_status(statusObj->valuestring, &settings_local.status) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID STATUS INPUT, TRY AGAIN");
        return ESP_FAIL;
    }

    if(settings_local.status == powerful)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        pthread_mutex_lock(&((struct usercontext *)req->user_ctx)->lock);
        ((struct usercontext *)req->user_ctx)->powerful_time = tv.tv_sec;
        pthread_mutex_unlock(&((struct usercontext *)req->user_ctx)->lock);
    }

    cJSON *tempObj = cJSON_GetObjectItem(json, "temperature");
    if (!cJSON_IsNumber(tempObj)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Temperature in JSON is not a number");
        return ESP_FAIL;
    }

    if (statusObj->valueint != 4 && modeObj->valueint == 5 && (tempObj->valueint < 16 || tempObj->valueint > 30))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Temperature is not between 16 and 30");
        return ESP_FAIL;
    }
    else if (statusObj->valueint != 4 && (tempObj->valueint < 18 || tempObj->valueint > 30))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Temperature is not between 18 and 30");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "user input temp: %d", tempObj->valueint);
    settings_local.temp = tempObj->valueint;


    cJSON *strengthObj = cJSON_GetObjectItem(json, "strength");
    if (!cJSON_IsString(strengthObj)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Strength value in JSON is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input strength: %s", strengthObj->valuestring);
    
    if (to_enum_strength(strengthObj->valuestring, &settings_local.strength) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID STRENGTH INPUT, TRY AGAIN");
        return ESP_FAIL;
    }

    cJSON *powerObj = cJSON_GetObjectItem(json, "power");
    if (!cJSON_IsString(powerObj)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Strength value in JSON is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input power: %s", powerObj->valuestring);
    
    if (strcmp(powerObj->valuestring, "on") != 0 && strcmp(powerObj->valuestring, "off") != 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID POWER INPUT, TRY AGAIN");
        return ESP_FAIL;
    }

    if (strcmp(powerObj->valuestring, "on") == 0)
    {
        settings_local.turnoff = false;
    }
    else if (strcmp(powerObj->valuestring, "off") == 0)
    {
        settings_local.turnoff = true;
    }

    #if CONFIG_AIRCO_MITSUBISHI
    if (settings_local.mode == fan)
    {
        settings_local.status = normal;
    }

    if (settings_local.mode == dry && settings_local.status == powerful)
    {
        settings_local.status = normal;
    }
    #endif

    pthread_mutex_lock(&((struct usercontext *)req->user_ctx)->lock);
    ((struct usercontext *)req->user_ctx)->send_settings = settings_local;
    buffer_push((struct circ_buf*) ((struct usercontext *)req->user_ctx)->buffer, &((struct usercontext *)req->user_ctx)->send_settings);
    pthread_mutex_unlock(&((struct usercontext *)req->user_ctx)->lock);

    cJSON_Delete(json);

    ESP_LOGI(TAG, "JSON FILE RECEIVED FROM USER");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t get_settings_handler(httpd_req_t *req)
{

    pthread_mutex_lock(&((struct usercontext *)req->user_ctx)->lock);
    struct signal_settings settings_local = ((struct usercontext *)req->user_ctx)->send_settings;
    pthread_mutex_unlock(&((struct usercontext *)req->user_ctx)->lock);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", settings_local.temp);
    cJSON_AddStringToObject(root, "mode", from_enum_mode[settings_local.mode]);
    cJSON_AddStringToObject(root, "strength", from_enum_strength[settings_local.strength]);

    if(settings_local.status == powerful)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        pthread_mutex_lock(&((struct usercontext *)req->user_ctx)->lock);
        time_t powerful_oldtime = ((struct usercontext *)req->user_ctx)->powerful_time;
        pthread_mutex_unlock(&((struct usercontext *)req->user_ctx)->lock);
        if (tv.tv_sec > powerful_oldtime + 900)
        {
            settings_local.status = normal;
        }
    }

    cJSON_AddStringToObject(root, "status", from_enum_status[settings_local.status]);

    time_t now;
    struct tm current_time;
    char strftime_buf_a[64];
    char strftime_buf_b[64];

    time(&now);
    localtime_r(&now, &current_time);

    pthread_mutex_lock(&syncdata.lock);
    if(syncdata.time == NULL)
    {
        strcpy(strftime_buf_a, "Not synchronized");
        strcpy(strftime_buf_b, "Not synchronized");
    }
    else
    {
        strftime(strftime_buf_a, sizeof(strftime_buf_a), "%d/%m/%y, %X", &current_time);
        strftime(strftime_buf_b, sizeof(strftime_buf_b), "%d/%m/%y, %X", syncdata.time);
    }
    pthread_mutex_unlock(&syncdata.lock);

    cJSON_AddStringToObject(root, "time", strftime_buf_a);
    cJSON_AddStringToObject(root, "sync", strftime_buf_b);

    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "JSON FILE SENT TO USER");
    return ESP_OK;
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_img_state;
    if (esp_ota_get_state_partition(running, &ota_img_state) == ESP_OK)
    {
        if (ota_status == OTA_STATUS_WAITING_FOR_COMMIT)
        {
            ESP_LOGI(TAG, "OTA needs to be committed");
            httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTA needs to be committed");
            return ESP_FAIL;
        }
        else if (ota_status == OTA_STATUS_CONNECTING || ota_status == OTA_STATUS_DOWNLOADING)
        {
            ESP_LOGI(TAG, "OTA in progress");
            httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTA in progress");
            return ESP_FAIL;
        }
        else if (ota_status == OTA_STATUS_WAITING_FOR_REBOOT)
        {
            ESP_LOGI(TAG, "OTA waiting for reboot");
            httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "OTA waiting for reboot");
            return ESP_FAIL;
        }
    }

    char *ota_url_buf = (char *) malloc(OTA_URL_SIZE);
    if (ota_url_buf == NULL)
    {
        ESP_LOGI(TAG, "No memory to store URL");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No memory to store URL");
    }

    int total_len = req->content_len;
    int cur_len = 0;
    int received = 0;
    if (total_len >= OTA_URL_SIZE)
    {
        /* Respond with 400 Bad Request */
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware URL too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len)
    {
        received = httpd_req_recv(req, ota_url_buf + cur_len, total_len);
        if (received <= 0)
        {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive firmware URL");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    ota_url_buf[total_len] = '\0';

    httpd_resp_sendstr(req, "ota starting\n");

    ota_status = OTA_STATUS_CONNECTING;

    BaseType_t ret = xTaskCreate(&ota_task, "ota_task", 8192, (void *)ota_url_buf, configMAX_PRIORITIES - 2, NULL);
    if (ret != pdPASS)
    {
        ota_status = OTA_STATUS_FAILED;
        ESP_LOGE(TAG, "Error starting OTA task");    
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error starting OTA task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ota_commit_post_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
          ESP_LOGI(TAG, "OTA commit requested");
          esp_ota_mark_app_valid_cancel_rollback();
          ESP_LOGI(TAG, "OTA commit completed");
          httpd_resp_sendstr(req, "Commit completed\n");
          ota_status = OTA_STATUS_INACTIVE;
          return ESP_OK;
        }
        else
        {
            ESP_LOGI(TAG, "Nothing to commit");
            httpd_resp_sendstr(req, "Nothing to commit\n");
        }
    }
    else
    {
        httpd_resp_sendstr(req, "Not running ota\n");
    }
    return ESP_OK;
}

esp_err_t ota_rollback_post_handler(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
        {
            ESP_LOGE(TAG, "Rolling back OTA");
            httpd_resp_sendstr(req, "Rolling back\n");
            sleep(1);
            esp_system_abort(NULL);
            //TODO: make it work with the accurate function for this:
            // esp_ota_mark_app_invalid_rollback_and_reboot();
        }
        else
        {
            httpd_resp_sendstr(req, "Nothing to rollback\n");
        }
    }
    else
    {
        httpd_resp_sendstr(req, "not running ota\n");
    }
    return ESP_OK;
}

#if CONFIG_AIRCO_HOST
static esp_err_t get_index_handler(httpd_req_t *req)
{
    FILE *file = fopen("/spiffs/index.html", "r");
    if(file == NULL)
    {
        ESP_LOGE(TAG,"File does not exist!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    if (httpd_resp_set_type(req, "text/html") != ESP_OK)
    {
        ESP_LOGE(REST_TAG, "Failed to set type to text/html");
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *chunk = (char *)req->user_ctx;
    size_t r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);

    while (r > 0)
    {
        if (httpd_resp_send_chunk(req, chunk, r) != ESP_OK)
        {
            fclose(file);
            ESP_LOGE(REST_TAG, "File sending failed!");
            /* Abort sending file */
            httpd_resp_sendstr_chunk(req, NULL);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            return ESP_FAIL;
        }
        r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);
    }

    /* Close file after sending complete */
    fclose(file);
    ESP_LOGI(REST_TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t get_favicon_handler(httpd_req_t *req)
{
    FILE *file = fopen("/spiffs/favicon.ico", "r");
    if(file == NULL)
    {
        ESP_LOGE(TAG,"File does not exist!");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    if (httpd_resp_set_type(req, "image/x-icon") != ESP_OK)
    {
        ESP_LOGE(REST_TAG, "Failed to set type to image/x-icon");
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *chunk = (char *)req->user_ctx;
    size_t r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);

    while (r > 0)
    {
        if (httpd_resp_send_chunk(req, chunk, r) != ESP_OK)
        {
            fclose(file);
            ESP_LOGE(REST_TAG, "File sending failed!");
            /* Abort sending file */
            httpd_resp_sendstr_chunk(req, NULL);
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send icon");
            return ESP_FAIL;
        }
        r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);
    }

    /* Close file after sending complete */
    fclose(file);
    ESP_LOGI(REST_TAG, "Icon sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

httpd_handle_t start_webserver(struct server_t arg)
{
    struct server_t webserver = arg;

    webserver.rest_context = (char *)malloc(sizeof(char) * SCRATCH_BUFSIZE);

    webserver.sendthis_context = (struct usercontext *)malloc(sizeof(struct usercontext));
    webserver.sendthis_context->buffer = webserver.buffer;
    webserver.sendthis_context->send_settings.temp = 20;
    webserver.sendthis_context->send_settings.mode = automode;
    webserver.sendthis_context->send_settings.strength = autostrength;
    webserver.sendthis_context->send_settings.status = normal;

    xTaskCreate(&realtime_checker, "realtime_check", 2048, webserver.sendthis_context, 1, &handle_realtime);

    while (pthread_mutex_init(&webserver.sendthis_context->lock, NULL) != 0)
    {
        continue;
    }

    const httpd_uri_t post_settings = {
        .uri       = "/settings",
        .method    = HTTP_POST,
        .handler   = post_settings_handler,
        .user_ctx  = webserver.sendthis_context
    };

    const httpd_uri_t get_settings = {
        .uri       = "/settings",
        .method    = HTTP_GET,
        .handler   = get_settings_handler,
        .user_ctx  = webserver.sendthis_context
    };

    const httpd_uri_t ota_post = {
        .uri = "/ota/new",
        .method = HTTP_POST,
        .handler = ota_post_handler
    };

    const httpd_uri_t ota_commit_post = {
        .uri = "/ota/commit",
        .method = HTTP_POST,
        .handler = ota_commit_post_handler
    };

    const httpd_uri_t ota_rollback_post = {
        .uri = "/ota/rollback",
        .method = HTTP_POST,
        .handler = ota_rollback_post_handler
    };

    #if CONFIG_AIRCO_HOST
    const httpd_uri_t get_index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = get_index_handler,
        .user_ctx  = webserver.rest_context
    };

    const httpd_uri_t get_favicon = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = get_favicon_handler,
        .user_ctx  = webserver.rest_context
    };
    #endif

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &post_settings);
        httpd_register_uri_handler(server, &get_settings);
        httpd_register_uri_handler(server, &ota_post);
        httpd_register_uri_handler(server, &ota_commit_post);
        httpd_register_uri_handler(server, &ota_rollback_post);
        #if CONFIG_AIRCO_HOST
        httpd_register_uri_handler(server, &get_index);
        httpd_register_uri_handler(server, &get_favicon);
        #endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

#if CONFIG_AIRCO_HOST
void spiff_config (void)
{
    esp_vfs_spiffs_conf_t config = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = true,
	};
	esp_err_t ret = esp_vfs_spiffs_register(&config);

	if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
    }

	size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}
#endif

void stop_webserver(struct server_ctx_t* server_ctx)
{
    httpd_stop(server_ctx->server);

    if(handle_realtime != NULL)
    {
        vTaskDelete(handle_realtime);
    }

    free((void *)server_ctx->webserver.rest_context);
    free((void *)server_ctx->webserver.sendthis_context);
    free((void *)syncdata.time);
}

void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct server_ctx_t* server_ctx = (struct server_ctx_t*) arg;
    if (server_ctx->server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(server_ctx);
        server_ctx->server = NULL;
    }

    set_led(0, 0, 0);
}

void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct server_ctx_t* server_ctx = (struct server_ctx_t*) arg;
    if (server_ctx->server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        server_ctx->server = start_webserver(server_ctx->webserver);
    }
    set_led(0, 25, 0);
}

bool to_enum_mode(char* input, enum mode_t* location)
{
    if (strcmp(input, "auto") == 0)
    {
        *location = automode;
    }
    else if (strcmp(input, "cool") == 0)
    {
        *location = cool;
    }
    else if (strcmp(input, "dry") == 0)
    {
        *location = dry;
    }
    else if (strcmp(input, "fan") == 0)
    {
        *location = fan;
    }
    else if (strcmp(input, "heat") == 0)
    {
        *location = heat;
    }
    else
    {
        return false;
    }

    return true;
}


bool to_enum_status(char* input, enum status_t* location)
{
    if (strcmp(input, "normal") == 0)
    {
        *location = normal;
    }
    else if (strcmp(input, "powerful") == 0)
    {
        *location = powerful;
    }
    else if (strcmp(input, "economy") == 0)
    {
        *location = economy;
    }
    else
    {
        return false;
    }

    return true;
}


bool to_enum_strength(char* input, enum strength_t* location)
{
    if (strcmp(input, "auto") == 0)
    {
        *location = autostrength;
    }
    else if (strcmp(input, "quiet") == 0)
    {
        *location = quiet;
    }
    else if (strcmp(input, "low") == 0)
    {
        *location = low;
    }
    else if (strcmp(input, "med") == 0)
    {
        *location = med;
    }
    else if (strcmp(input, "high") == 0)
    {
        *location = high;
    }
    else
    {
        return false;
    }

    return true;
}