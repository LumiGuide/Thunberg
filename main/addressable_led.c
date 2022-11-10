/* written for LED SK6812 a.k.a. built-in LED of ESP32 stamp pico:
https://cdn-shop.adafruit.com/product-files/1138/SK6812+LED+datasheet+.pdf*/

#include "addressable_led.h"

#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"

#define TAG "rmt & encoder"

#define RMT_RESOLUTION_HZ 10000000 // 10 MHz resolution
#define RMT_GPIO_NUM      27

#define AMOUNT_OF_BYTES 3

typedef struct {
    uint32_t duration_high;
    uint32_t duration_low;
} input_t;

typedef struct {
    uint32_t resolution;
} encoder_config_t;

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t *copy_encoder;
    uint32_t resolution;
} rmt_encoder_struct_t;

void rmt_led(input_t *colour_in);
esp_err_t rmt_new_encoder(const encoder_config_t *config, rmt_encoder_handle_t *ret_encoder);


/* RMT PART */

void set_led(uint8_t red, uint8_t green, uint8_t blue)
{
    input_t colour[(AMOUNT_OF_BYTES * 8)];
    uint8_t copy_colour = green;
    for (int i = 0; i < (AMOUNT_OF_BYTES * 8); i++)
    {
        colour[i] = copy_colour & 1000000 ? (input_t){6, 6} : (input_t){3, 9};
        copy_colour <<= 1;
        if(i == 8)
        {
            copy_colour = red;
        }
        if(i == 16)
        {
            copy_colour = blue;
        }
    }

    rmt_led(colour);
}

void rmt_led(input_t *colour_in)
{
    input_t *colour = colour_in;

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t my_rmt_channel = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = RMT_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 10, // set the maximum number of transactions that can pend in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &my_rmt_channel));

    ESP_LOGI(TAG, "Install encoder");
    rmt_encoder_handle_t this_encoder = NULL;
    encoder_config_t encoder_config = {
        .resolution = RMT_RESOLUTION_HZ
    };
    ESP_ERROR_CHECK(rmt_new_encoder(&encoder_config, &this_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(my_rmt_channel));
    ESP_LOGI(TAG, "Starting transmit");

    for (size_t i = 0; i < (AMOUNT_OF_BYTES * 8); i++)
    {
        rmt_transmit_config_t tx_config = {
            .loop_count = 0,
        };
        ESP_ERROR_CHECK(rmt_transmit(my_rmt_channel, this_encoder, &colour[i], sizeof(input_t), &tx_config));
    }
}



/* ENCODER PART */

size_t rmt_encode(rmt_encoder_t *encoder, rmt_channel_handle_t channel, const void *primary_data, size_t data_size, rmt_encode_state_t *ret_state)
{
    rmt_encoder_struct_t *score_encoder = __containerof(encoder, rmt_encoder_struct_t, base);
    rmt_encoder_handle_t copy_encoder = score_encoder->copy_encoder;
    rmt_encode_state_t session_state = 0;
    input_t *input_data = (input_t *)primary_data;
    rmt_symbol_word_t rmt_symbol = {
        .level0 = 1,
        .duration0 = input_data->duration_high,
        .level1 = 0,
        .duration1 = input_data->duration_low,
    };
    size_t encoded_symbols = copy_encoder->encode(copy_encoder, channel, &rmt_symbol, sizeof(rmt_symbol), &session_state);
    *ret_state = session_state;
    return encoded_symbols;
}

esp_err_t rmt_del_my_encoder(rmt_encoder_t *encoder)
{
    rmt_encoder_struct_t *score_encoder = __containerof(encoder, rmt_encoder_struct_t, base);
    rmt_del_encoder(score_encoder->copy_encoder);
    free(score_encoder);
    return ESP_OK;
}

esp_err_t rmt_reset_my_encoder(rmt_encoder_t *encoder)
{
    rmt_encoder_struct_t *score_encoder = __containerof(encoder, rmt_encoder_struct_t, base);
    rmt_encoder_reset(score_encoder->copy_encoder);
    return ESP_OK;
}

esp_err_t rmt_new_encoder(const encoder_config_t *config, rmt_encoder_handle_t *ret_encoder)
{
    esp_err_t ret = ESP_OK;
    rmt_encoder_struct_t *score_encoder = NULL;
    ESP_GOTO_ON_FALSE(config && ret_encoder, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    score_encoder = calloc(1, sizeof(rmt_encoder_struct_t));
    ESP_GOTO_ON_FALSE(score_encoder, ESP_ERR_NO_MEM, err, TAG, "no mem for encoder");
    score_encoder->base.encode = rmt_encode;
    score_encoder->base.del = rmt_del_my_encoder;
    score_encoder->base.reset = rmt_reset_my_encoder;
    score_encoder->resolution = config->resolution;
    rmt_copy_encoder_config_t copy_encoder_config = {};
    ESP_GOTO_ON_ERROR(rmt_new_copy_encoder(&copy_encoder_config, &score_encoder->copy_encoder), err, TAG, "create copy encoder failed");
    *ret_encoder = &score_encoder->base;
    return ESP_OK;
err:
    if (score_encoder) {
        if (score_encoder->copy_encoder) {
            rmt_del_encoder(score_encoder->copy_encoder);
        }
        free(score_encoder);
    }
    return ret;
}