#include "ir_sender.h"
#include <esp_log.h>
#include "driver/ledc.h"
#include "circular_buffer.h"
#include "esp32/rom/ets_sys.h"

const int ANNOUNCE = 405;
const int ZERO = 395;
const int ONE = 1200;
const int START_A = 3240;
const int START_B = 1625;

const uint8_t NORMAL_S = 0b11111110;
const uint8_t POWERFUL_S = 0b00111001;
const uint8_t ECONOMY_S = 0b00001001;
const uint8_t OFF_S = 0b00000010;
const uint8_t AUTO_S = 0b00000000;
const uint8_t COOL_S = 0b00000001;
const uint8_t DRY_S = 0b00000010;
const uint8_t FAN_S = 0b00000011;
const uint8_t HEAT_S = 0b00000100;
const uint8_t HIGH_S = 0b00000001;
const uint8_t MED_S = 0b00000010;
const uint8_t LOW_S = 0b00000011;
const uint8_t QUIET_S = 0b00000100;

void send_signal(struct signal_settings settings, uint8_t* signal);
uint8_t checksum(uint8_t base, uint8_t* signal, size_t len);
void send_inverted(uint8_t* signal, size_t length);
void ir_led(bool, int);

static const char *TAG = "fuji program";

void IR_sender(void* rowan)
{
    struct signal_settings settings;

    uint8_t signal[16];
    signal[0] = 0b00010100;
    signal[1] = 0b01100011;
    signal[2] = 0b00000000;
    signal[3] = 0b00010000;
    signal[4] = 0b00010000;
    signal[7] = 0b00110000;
    signal[11] = 0b00000000;
    signal[12] = 0b00000000;
    signal[13] = 0b00000000;
    signal[14] = 0b00100000;

	while(1)
	{
		while (buffer_pop((struct circ_buf*) rowan, &settings) < 0)
        {
            continue;
        }

        enum status_t repeat = settings.status;
        settings.status = normal;
        send_signal(settings, signal);

        if (settings.turnoff == false)
        {
            if (repeat == powerful)
            {
                usleep(500000);
                settings.status = economy;
                send_signal(settings, signal);

                usleep(500000);
                settings.status = powerful;
                send_signal(settings, signal);
            }
            else if (repeat == economy)
            {
                usleep(500000);
                settings.status = powerful;
                send_signal(settings, signal);

                usleep(500000);
                settings.status = economy;
                send_signal(settings, signal);
            }
            else // so (repeat == normal)
            {
                //homing sequence
                usleep(500000);
                settings.status = powerful;
                send_signal(settings, signal);

                usleep(500000);
                settings.status = economy;
                send_signal(settings, signal);

                usleep(500000);
                send_inverted(signal, 7); //2nd time economy signal to turn it off
            }
        usleep(500000);
        }
	}
}

void send_signal(struct signal_settings settings, uint8_t* signal)
{  
    uint8_t base = settings.status != normal || settings.turnoff == true ? 150 : 158;

    if (settings.turnoff == true)
    {
            signal[5] = OFF_S;
    }
    else
    {
        switch(settings.status){
            case normal:
                signal[5] = NORMAL_S;
            break;

            case powerful:
                signal[5] = POWERFUL_S;
            break;

            case economy:
                signal[5] = ECONOMY_S;
            break;

            default:
                ESP_LOGI(TAG, "ERROR: Invalid status value");
        }
    }

    signal[6] = checksum(base, signal, 6);

    if (settings.status != normal || settings.turnoff == true)
    {
        send_inverted(signal, 7);
        return;
    }

    //signal[8]
    //bit 1 tm 4 = temp - 16, flipped
    //bit 5 tm 8 = 0001
    uint8_t t = settings.temp;
    t = t - 16; //18: 00010010 -> 2: 00000010
    t = t << 4; // 00100000
    t = t + 1; // 00100001

    signal[8] = t;

    switch(settings.mode){
        case automode:
            signal[9] = AUTO_S;
        break;

        case cool:
            signal[9] = COOL_S;
        break;

        case dry:
            signal[9] = DRY_S;
        break;

        case fan:
            signal[9] = FAN_S;
        break;

        case heat:
            signal[9] = HEAT_S;
        break;

        default:
            ESP_LOGI(TAG, "ERROR: Invalid mode value");
    }

        switch(settings.strength){
        case autostrength:
            signal[10] = AUTO_S;
        break;

        case high:
            signal[10] = HIGH_S;
        break;

        case med:
            signal[10] = MED_S;
        break;

        case low:
            signal[10] = LOW_S;
        break;

        case quiet:
            signal[10] = QUIET_S;
        break;

        default:
            ESP_LOGI(TAG, "ERROR: Invalid strength value");
    }

    signal[15] = checksum(base, signal, 15);
    send_inverted(signal, 16);
}

uint8_t checksum(uint8_t base, uint8_t* signal, size_t len)
{
    uint8_t check = 0;
    for (size_t i = 0; i < len; i++)
    {
        check += signal[i];
    }

    return base - check;
}

void send_inverted(uint8_t* signal, size_t length)
{
    portMUX_TYPE criticalzone = portMUX_INITIALIZER_UNLOCKED;

    taskENTER_CRITICAL(&criticalzone);

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

    taskEXIT_CRITICAL(&criticalzone);

    ESP_LOGI(TAG, "Fuji signal sent");
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
        .freq_hz          = LEDC_FREQUENCY,
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
        .duty           = LEDC_DUTY50,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}