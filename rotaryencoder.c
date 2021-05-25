#include "rotaryencoder.h"
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"


//#define DEBOUNCE_EDGE   2000
//#define DEBOUNCE_REPORT 5000

#define DEBOUNCE_EDGE   100
#define DEBOUNCE_REPORT 1000

void rotary_task(rotary_encoder_t *data) {
    uint32_t current_us = time_us_32();

    uint8_t a = gpio_get(data->gpio_a);
    uint8_t b = gpio_get(data->gpio_b);

    if (data->triggered && current_us > data->wait_until) {
        data->triggered = 0;
        data->filtered_a = a;
    }

    if (a != data->last_a) {
        data->triggered = 1;
        data->wait_until = current_us + DEBOUNCE_EDGE;
    }

    // state change on filtered A
    if (data->filtered_a != data->last_filtered_a) {        
        // only allow signal everey 5ms
        if (current_us > data->wait_until_trigger) {
            data->dir = 0;
            if ((data->filtered_a) == b) {
                //puts("R");
                data->dir = 1 * data->factor;
            }
            else {
                //puts("L");
                data->dir = -1 * data->factor;
            }
            data->current_value += data->dir;
            if (data->current_value < data->min_value) {
                data->current_value = data->min_value;
            }
            if (data->current_value > data->max_value) {
                data->current_value = data->max_value;
            }
            //printf("%d \r\n", data->current_value);
        }
        data->wait_until_trigger = current_us + DEBOUNCE_REPORT;
    }

    data->last_filtered_a = data->filtered_a;    
    data->last_a = a;
}

void rotary_task2(rotary_encoder_t *data) {
    uint32_t current_us = time_us_32();

    uint8_t a = gpio_get(data->gpio_a);
    uint8_t b = gpio_get(data->gpio_b);

    if (data->triggered && current_us > data->wait_until) {
        data->triggered = 0;
        data->filtered_a = a;
    }

    if (a != data->last_a) {
        data->triggered = 1;
        data->wait_until = current_us + DEBOUNCE_EDGE;
    }

    // state change on filtered A
    if (data->filtered_a != data->last_filtered_a) {        
        // only allow signal everey 5ms
        if (current_us > data->wait_until_trigger) {
            data->dir = 0;

            // only on falling edge
            if ((data->filtered_a) == 0) {
                if (b) {   
                    data->dir = 1 * data->factor;
                }
                else {
                    data->dir = -1 * data->factor;
                }
            }
            data->current_value += data->dir;
            if (data->current_value < data->min_value) {
                data->current_value = data->min_value;
            }
            if (data->current_value > data->max_value) {
                data->current_value = data->max_value;
            }
            //printf("%d \r\n", data->current_value);
        }
        data->wait_until_trigger = current_us + DEBOUNCE_REPORT;
    }

    data->last_filtered_a = data->filtered_a;    
    data->last_a = a;
}

