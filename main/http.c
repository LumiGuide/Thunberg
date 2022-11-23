#include "http.h"
#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs.h"
#include "cJSON.h"

#include "esp_spiffs.h"

static const char *TAG = "setup http";
static const char *REST_TAG = "get html";
#define SCRATCH_BUFSIZE (10240)

// enum mode_t{automode, cool, dry, fan, heat};
// enum strength_t{autostrength, high, med, low, quiet};
// enum status_t{normal, powerful, economy, off};


struct signal_settings send_settings;
//TODO: dit bij initialisatie
// send_settings.temp = 0;
// send_settings.mode = 0;
// send_settings.strength = 0;
// send_settings.status = 0;

bool to_enum_mode(char* input, enum mode_t* location);
bool to_enum_status(char* input, enum status_t* location);
bool to_enum_strength(char* input, enum strength_t* location);

static esp_err_t post_settings_handler(httpd_req_t *req)
{
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

    if (to_enum_mode(modeObj->valuestring, &send_settings.mode) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID MODE INPUT, TRY AGAIN"); //TODO: foutmelding naar frontend
        return ESP_FAIL;
    }


    cJSON *statusObj = cJSON_GetObjectItem(json, "status");
    if (!cJSON_IsString(statusObj))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Status value in JSON is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input status: %s", statusObj->valuestring);
    if (to_enum_status(statusObj->valuestring, &send_settings.status) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID STATUS INPUT, TRY AGAIN"); //TODO: foutmelding naar frontend
        return ESP_FAIL;
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
    send_settings.temp = tempObj->valueint;


    cJSON *strengthObj = cJSON_GetObjectItem(json, "strength");
    if (!cJSON_IsString(strengthObj)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Strength value in JSON is not a string");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "user input strength: %s", strengthObj->valuestring);
    
    if (to_enum_strength(strengthObj->valuestring, &send_settings.strength) == false)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID STRENGTH INPUT, TRY AGAIN"); //TODO: foutmelding naar frontend
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
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "INVALID POWER INPUT, TRY AGAIN"); //TODO: foutmelding naar frontend
        return ESP_FAIL;
    }

    if (strcmp(powerObj->valuestring, "on") == 0)
    {
        send_settings.turnoff = false;
    }
    else if (strcmp(powerObj->valuestring, "off") == 0)
    {
        send_settings.turnoff = true;
    }
    
    buffer_push((struct circ_buf*) req->user_ctx, &send_settings);

    //TODO: cJSON_Delete ?

    // httpd_resp_set_type(req, "application/json");
    // cJSON *root = cJSON_CreateObject();
    // cJSON_AddStringToObject(root, "received_settings", "hallo");
    // const char *sys_info = cJSON_Print(root);
    // httpd_resp_sendstr(req, sys_info);
    // free((void *)sys_info);
    // cJSON_Delete(root);
    ESP_LOGI(TAG, "JSON FILE RECEIVED FROM USER");

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t get_settings_handler(httpd_req_t *req)
{
    //TODO: strings sturen ipv ints!
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", send_settings.temp);
    cJSON_AddNumberToObject(root, "mode", send_settings.mode);
    cJSON_AddNumberToObject(root, "strength", send_settings.strength);
    cJSON_AddNumberToObject(root, "status", send_settings.status);
    const char *sys_info = cJSON_Print(root);
    httpd_resp_sendstr(req, sys_info);
    free((void *)sys_info);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "JSON FILE SENT TO USER");
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

    char *chunk = (char *)req->user_ctx;
    size_t r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);

    while (r > 0)
    {
        //TODO: if return != ESP_OK dan error
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

    char *chunk = (char *)req->user_ctx;
    size_t r = fread(chunk, sizeof(char), SCRATCH_BUFSIZE, file);

    while (r > 0)
    {
        //TODO: if return != ESP_OK dan error
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

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

httpd_handle_t start_webserver(struct circ_buf* buffer)
{
    char* rest_context = (char *)malloc(sizeof(char) * SCRATCH_BUFSIZE);

    const httpd_uri_t post_settings = {
        .uri       = "/settings",
        .method    = HTTP_POST,
        .handler   = post_settings_handler,
        .user_ctx  = buffer
    };

    const httpd_uri_t get_settings = {
        .uri       = "/settings",
        .method    = HTTP_GET,
        .handler   = get_settings_handler,
        .user_ctx  = NULL
    };

    #if CONFIG_AIRCO_HOST
    const httpd_uri_t get_index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = get_index_handler,
        .user_ctx  = rest_context
    };

    const httpd_uri_t get_favicon = {
        .uri       = "/favicon.ico",
        .method    = HTTP_GET,
        .handler   = get_favicon_handler,
        .user_ctx  = rest_context
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

void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct server_ctx_t* server_ctx = (struct server_ctx_t*) arg;
    if (server_ctx->server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(server_ctx->server);
        server_ctx->server = NULL;
    }
}

void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct server_ctx_t* server_ctx = (struct server_ctx_t*) arg;
    if (server_ctx->server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        server_ctx->server = start_webserver(server_ctx->buffer);
    }
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