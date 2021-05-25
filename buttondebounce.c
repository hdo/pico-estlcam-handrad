#include "buttondebounce.h"
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"


#define DEBOUNCE_EDGE   100


void button_task(button_debounce_t *data) {
    uint32_t current_us = time_us_32();

    uint8_t a = gpio_get(data->gpio_a);

    if (data->triggered && current_us > data->wait_until) {
        data->triggered = 0;
        data->state = a;
    }

    if (a != data->last_a) {
        data->triggered = 1;
        data->wait_until = current_us + DEBOUNCE_EDGE;
    }
    data->last_a = a;
}

