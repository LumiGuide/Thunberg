#include "ir_sender.h"
#include <esp_log.h>
#include "driver/ledc.h"
#include "circular_buffer.h"
#include "esp32/rom/ets_sys.h"


const int ANNOUNCE = 365;
const int ZERO = 427;
const int ONE = 1220;
const int START_A = 3160;
const int START_B = 1610;

const uint8_t AUTO_MODE_S = 0b00001000;
const uint8_t COOL_S = 0b00001001;
const uint8_t DRY_S = 0b00001010;
const uint8_t FAN_S = 0b00001011;
const uint8_t HEAT_S = 0b00001100;
const uint8_t OFF_S = 0b00000000;
const uint8_t AUTO_FAN_S = 0b00000000;
const uint8_t ONE_S = 0b00000001;
const uint8_t TWO_S = 0b00000010;
const uint8_t THREE_S = 0b00000011;
const uint8_t FOUR_S = 0b00000100;

static const char *TAG = "mitsubishi program";

void IR_sender(void* rowan)
{
    struct signal_settings settings;

    uint8_t signal[19];
    signal[0] = 0b01010010;
    signal[1] = 0b10101110;
    signal[2] = 0b11000011;

    signal[4] = 0b11100101;
    signal[3] = ~signal[4];

    signal[12] = 0b11000000;
    signal[11] = ~signal[12];

    signal[14] = 0b11001000;
    signal[13] = ~signal[14];

    signal[16] = 0b00000000;
    signal[15] = ~signal[16];

    signal[18] = 0b10000000;
    signal[17] = ~signal[18];

	while(1)
	{
		while (buffer_pop((struct circ_buf*) rowan, &settings) < 0)
        {
            continue;
        }

        printf("airco: %i\n", settings.airco);
        printf("temp: %i\n", settings.temp);
        printf("mode: %i\n", settings.mode);
        printf("strength: %i\n", settings.strength);
        printf("status: %i\n", settings.status);
        send_signal(settings, signal);
        send_inverted(signal, 19);
        //TODO: airco nummer toevoegen en alleen daar uitzenden
	}
}

void send_signal(struct signal_settings settings, uint8_t* signal)
{
    switch(settings.mode){
        case automode:
            signal[6] = AUTO_MODE_S;
        break;

        case cool:
            signal[6] = COOL_S;
        break;

        case dry:
            signal[6] = DRY_S;
        break;

        case fan:
            signal[6] = FAN_S;
        break;

        case heat:
            signal[6] = HEAT_S;
        break;

        // case off:
        //     signal[6] = OFF_S;
        // break;

        default:
            ESP_LOGI(TAG, "ERROR: Invalid mode value");
    }

    signal[5] = ~signal[6];

    signal[8] = settings.temp - 17;
    signal[7] = ~signal[8];

    switch(settings.strength){
        case autostrength:
            signal[10] = AUTO_FAN_S;
        break;
        
        case quiet:
            signal[10] = ONE_S;
        break;
        
        case low:
            signal[10] = TWO_S;
        break;
        
        case med:
            signal[10] = THREE_S;
        break;
        
        case high:
            signal[10] = FOUR_S;
        break;
        
        default:
            ESP_LOGI(TAG, "ERROR: Invalid fan speed value");
    }

    signal[9] = ~signal[10];

    //TODO: economy en powerful erbij
}

void send_inverted(uint8_t* signal, size_t length)
{
    portMUX_TYPE test = portMUX_INITIALIZER_UNLOCKED;

    taskENTER_CRITICAL(&test);

    // SEND STARTSIGNAL
    ir_led(true, START_A);
    ir_led(false, START_B);

    // SEND INVERTED BYTES
    for (size_t x = 0; x < length; ++x)
    {
        uint8_t byte = signal[x];
        for (uint8_t y = 0; y < 8; ++y)
        {
            ir_led(true, ANNOUNCE);
            ir_led(false, (byte & 00000001) > 0 ? ONE : ZERO);
            byte = byte >> 1;
        }
    }

    // SEND End announcement and turn off LED
    ir_led(true, ANNOUNCE);
    ir_led(false, 0);

    taskEXIT_CRITICAL(&test);
    
    printf("! signal sent\n");
}

void ir_led(bool x, int t)
{
    uint32_t duty = x ? LEDC_DUTY50 : LEDC_DUTYOFF;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    // Update duty to apply the new value
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    ets_delay_us(t);
}

void ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 38 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = LEDC_DUTY50, // Set duty to 50%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}