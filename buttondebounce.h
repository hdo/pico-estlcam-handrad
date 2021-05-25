#ifndef BUTTONDEBOUNCE_H
#define BUTTONDEBOUNCE_H

#include <stdint.h>

typedef struct BUTTON_DEBOUNCE_DATA
{
    uint8_t gpio_a;      // required
    uint8_t triggered;  
    uint32_t wait_until, wait_until_trigger;
    uint8_t last_a;
    uint8_t state;
} button_debounce_t;

void button_task(button_debounce_t *data);

#endif