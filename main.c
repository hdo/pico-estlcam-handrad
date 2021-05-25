#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "rotaryencoder.h"
#include "buttondebounce.h"


/*
 
Datenframe Modus 1:
Byte 1: 255 (Altlast - muss 255 zurückgeben, sonst ggf. Probleme!)
Byte 2:
Bit 0 = Start / Stopp Taste Programmstart (1 = nicht gedrückt / 0 = gedrückt)
Bit 1 = Start / Stopp Taste Fräsmotor (1 = nicht gedrückt / 0 = gedrückt)
Bit 2 = 1 (sonst ggf. Probleme!)
Bit 3 = "OK" Taste (1 = nicht gedrückt / 0 = gedrückt)
Bit 4 bis 7 = 1 (sonst ggf. Probleme!)
Byte 3: Low Byte Vorschub Poti
Byte 4: High Byte Vorschub Poti
Byte 5: Low Byte Drehzahl Poti
Byte 6: High Byte Drehzahl Poti
Byte 7: Low Byte X Joystick
Byte 8: High Byte X Joystick
Byte 9: Low Byte Y Joystick
Byte 10: High Byte Y Joystick
Byte 11: Low Byte Z Joystick
Byte 12: High Byte Z Joystick
Byte 13: 1 (Modus)
Byte 14: Frame (ständig durchlaufend 0-255)
Byte 15: Prüfsumme (siehe Berechnung der Prüfsumme)
Poti und Joystickstellungen sind 16 Bit Unsigned Integer von 0 bis 65535.
   
 */



#define I2C_PORT i2c0
//#define I2C_ADDR 0b00000100
#define I2C_ADDR 0b0000010


#define READ_SIZE 9
#define WRITE_SIZE 15

#define MODEID_NORMAL    1 
#define MODEID_IDENTIFY  3
#define MODEID_CHALLENGE 4
#define MODEID_SERIAL    5

#define BUTTON_SHIFT_PROG    0
#define BUTTON_SHIFT_SPINDLE 1
#define BUTTON_SHIFT_OK      3


// define the hardware registers used
volatile uint32_t * const I2C0_DATA_CMD       = (volatile uint32_t * const)(I2C0_BASE + 0x10);
volatile uint32_t * const I2C0_INTR_STAT      = (volatile uint32_t * const)(I2C0_BASE + 0x2c);
volatile uint32_t * const I2C0_INTR_MASK      = (volatile uint32_t * const)(I2C0_BASE + 0x30);
volatile uint32_t * const I2C0_CLR_RD_REQ     = (volatile uint32_t * const)(I2C0_BASE + 0x50);
volatile uint32_t * const I2C0_CLR_STOP_DET   = (volatile uint32_t * const)(I2C0_BASE + 0x60);
volatile uint32_t * const I2C0_CLR_START_DET  = (volatile uint32_t * const)(I2C0_BASE + 0x64);
volatile uint32_t * const I2C0_RAW_INTR_STAT  = (volatile uint32_t * const)(I2C0_BASE + 0x34);
volatile uint32_t * const I2C0_IC_CLR_INTR    = (volatile uint32_t * const)(I2C0_BASE + 0x40);
volatile uint32_t * const I2C0_CLR_TX_ABRT    = (volatile uint32_t * const)(I2C0_BASE + 0x54);
volatile uint32_t * const I2C0_TX_ABRT_SOURCE = (volatile uint32_t * const)(I2C0_BASE + 0x80);




// Declare the bits in the registers we use
#define I2C_DATA_CMD_FIRST_BYTE  0x00000800
#define I2C_DATA_CMD_DATA        0x000000ff
#define I2C_INTR_STAT_READ_REQ   0x00000020
#define I2C_INTR_STAT_RX_FULL    0x00000004
#define I2C_INTR_STAT_START_DET  (1 << 10)   // bit 10
#define I2C_INTR_MASK_READ_REQ   0x00000020  // bit 5 (1 << 5)
#define I2C_INTR_MASK_RX_FULL    0x00000004
#define I2C_INTR_MASK_START_DET  (1 << 10)   // bit 10

#define I2C_INTR_STAT_STOP_DET  (1 << 9)   
#define I2C_INTR_MASK_STOP_DET  (1 << 9)   

#define I2C_INTR_STAT_TX_ABRT    (1 << 6)    // bit 6
#define I2C_INTR_MASK_TX_ABRT    (1 << 6)    // bit 6




#define GPIO_OK         22  // BLUE
#define GPIO_SPINDLE    21  // YELLOW
#define GPIO_PROG       20  // GREEN


#define GPIO_ENCODER_SPINDLE_A  16  // GREEN
#define GPIO_ENCODER_SPINDLE_B  17  // BLUE

#define GPIO_ENCODER_FEED_A  18     // GREEN 
#define GPIO_ENCODER_FEED_B  19     // BLUE


const uint8_t LED_PIN = 25;
const uint8_t IDENT_STRING[] = "Handrad 0001";
const uint8_t SERIAL_STRING[] = "ABCD12345678";
const uint8_t CHALLENGE_VALUE[] = {0x3B, 0x59, 0xE8, 0x2A, 0xE9, 0xB1, 0xBE, 0xD8, 0x00, 0x00, 0x00, 0x00};

uint8_t rx_head = 0;
uint8_t tx_head = 0;
uint8_t rxdata[16];
uint8_t txdata[16];
uint8_t frame_num = 0;

uint8_t mode_selected = 0;
uint8_t crc_checksum = 0;

uint16_t adc_x_raw, adc_y_raw, adc_z_raw, adc_feed_raw, adc_spindle_raw;


rotary_encoder_t encoder_spindle, encoder_feed;
int16_t last_value_spindle, last_value_feed;
button_debounce_t button_ok, button_prog, button_spindle;


uint32_t current_tick()  {
    return time_us_32() / 1000;
}


void printhex(uint8_t *buffer, uint8_t length) {
    for(uint8_t i = 0; i < length; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\r\n");
}

void calc_checksum() {
    uint8_t checksum = 0;
    for(uint8_t i=0; i < 14; i++) {
        checksum ^= txdata[i];
        checksum++;
    }
    txdata[14] = checksum;    
}


void update_adc_values() {
    txdata[2] = 0xFF & adc_feed_raw;
    txdata[3] = adc_feed_raw >> 8;
    txdata[4] = 0xFF & adc_spindle_raw;
    txdata[5] = adc_spindle_raw >> 8;
    txdata[6] = 0xFF & adc_x_raw;
    txdata[7] = adc_x_raw >> 8;
    txdata[8] = 0xFF & adc_y_raw;
    txdata[9] = adc_y_raw >> 8;
    txdata[10] = 0xFF & adc_z_raw;
    txdata[11] = adc_z_raw >> 8;          
}

void update_button_states() {
    txdata[1] = 0b11110100 
        | button_prog.state << BUTTON_SHIFT_PROG 
        | button_spindle.state << BUTTON_SHIFT_SPINDLE 
        | button_ok.state << BUTTON_SHIFT_OK;
} 

void mode_normal() {
    // reset
    for(uint8_t i; i < 12; i++) {
        txdata[i] = 0;
    }    
    txdata[0] = 0xFF;
    txdata[1] = 0b11111111;
    txdata[12] = MODEID_NORMAL;
}

void mode_identify() {
    for(uint8_t i=0; i < 12; i++) {
        txdata[i] = IDENT_STRING[i];
    }
    txdata[12] = MODEID_IDENTIFY;
}

void mode_challenge() {
    for(uint8_t i=0; i < 12; i++) {
        txdata[i] = CHALLENGE_VALUE[i];
    }    
    txdata[12] = MODEID_CHALLENGE;
}

void mode_serial() {
    for(uint8_t i=0; i < 12; i++) {
        txdata[i] = SERIAL_STRING[i];
    }
    txdata[12] = MODEID_SERIAL;
}


void setup_mock() {
    for(uint8_t i=0; i < sizeof(txdata); i++) {
        txdata[i] = i;
    }
}


void i2c0_irq_handler() {
    // Get interrupt status
    uint32_t status = *I2C0_INTR_STAT;


    if (status & I2C_INTR_STAT_TX_ABRT) {
        // reset heads
        tx_head = 0;
        rx_head = 0;
        *I2C0_CLR_TX_ABRT;
    }

    // check for start condition
    if (status & I2C_INTR_STAT_START_DET) {
        // reset heads
        tx_head = 0;
        rx_head = 0;
        *I2C0_CLR_START_DET;
        //puts("A");
        *I2C0_CLR_TX_ABRT;
    }

    // check for stop condition
    if (status & I2C_INTR_STAT_STOP_DET) {
        // reset heads
        tx_head = 0;
        rx_head = 0;
        *I2C0_CLR_STOP_DET;
        //puts("O");
    }

    // Check to see if we have received data from the I2C master
    if (status & I2C_INTR_STAT_RX_FULL) {
        gpio_put(LED_PIN, 1);

        // Read the data (this will clear the interrupt)
        uint32_t value = *I2C0_DATA_CMD;

        // Check if this is the 1st byte we have received
        if (value & I2C_DATA_CMD_FIRST_BYTE) {
            // 
            rx_head = 0;
            // If so treat it as the address to use          
        } 

        rxdata[rx_head++] = (uint8_t)(value & I2C_DATA_CMD_DATA);

        if (rx_head == READ_SIZE) {
            printhex(rxdata, READ_SIZE);
            mode_selected = rxdata[0];
            //printf("mode: %d \r\n", mode_selected);

            switch(mode_selected) {
                case 1 : mode_normal(); break;
                case 3 : mode_identify(); break;
                case 4 : mode_challenge(); break;
                case 5 : mode_serial(); break;
                default: mode_selected = MODEID_NORMAL; mode_normal(); break;
            }

        }

        if (rx_head > READ_SIZE) {
            rx_head = 0;
            puts("rx ->0");
        }
        gpio_put(LED_PIN, 0);
    }

    // Check to see if the I2C master is requesting data from us
    if (status & I2C_INTR_STAT_READ_REQ) {
        gpio_put(LED_PIN, 1);
        //puts("R");

        if (mode_selected == MODEID_NORMAL) {
            update_adc_values();
            update_button_states();
        }

        txdata[13] = frame_num++;
        calc_checksum();
        
        for(uint8_t i = 0; i < WRITE_SIZE+1; i++) {
            *I2C0_DATA_CMD = (uint32_t) txdata[i];
        }
        //printhex(txdata, 15);

        gpio_put(LED_PIN, 0);

        // Clear the interrupt
        *I2C0_CLR_RD_REQ;
    }
}

void init_gpio() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(GPIO_OK);
    gpio_set_dir(GPIO_OK, GPIO_IN);
    gpio_pull_up(GPIO_OK);

    gpio_init(GPIO_PROG);
    gpio_set_dir(GPIO_PROG, GPIO_IN);
    gpio_pull_up(GPIO_PROG);

    gpio_init(GPIO_SPINDLE);
    gpio_set_dir(GPIO_SPINDLE, GPIO_IN);
    gpio_pull_up(GPIO_SPINDLE);


    gpio_init(GPIO_ENCODER_SPINDLE_A);
    gpio_set_dir(GPIO_ENCODER_SPINDLE_A, GPIO_IN);
    gpio_pull_up(GPIO_ENCODER_SPINDLE_A);

    gpio_init(GPIO_ENCODER_SPINDLE_B);
    gpio_set_dir(GPIO_ENCODER_SPINDLE_B, GPIO_IN);
    gpio_pull_up(GPIO_ENCODER_SPINDLE_B);

    gpio_init(GPIO_ENCODER_FEED_A);
    gpio_set_dir(GPIO_ENCODER_FEED_A, GPIO_IN);
    gpio_pull_up(GPIO_ENCODER_FEED_A);

    gpio_init(GPIO_ENCODER_FEED_B);
    gpio_set_dir(GPIO_ENCODER_FEED_B, GPIO_IN);
    gpio_pull_up(GPIO_ENCODER_FEED_B);


}

int main() {
    stdio_init_all();

    init_gpio();



    // init ADC
    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);
    adc_gpio_init(27);
    adc_gpio_init(28);

    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    i2c_init(I2C_PORT, 10 * 1000);
    i2c_set_slave_mode(I2C_PORT, true, I2C_ADDR);
    
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    encoder_spindle.gpio_a = GPIO_ENCODER_SPINDLE_A;
    encoder_spindle.gpio_b = GPIO_ENCODER_SPINDLE_B;
    encoder_spindle.min_value = 0;
    encoder_spindle.max_value = 200;
    encoder_spindle.factor = 5;
    encoder_spindle.current_value = 100;

    encoder_feed.gpio_a = GPIO_ENCODER_FEED_A;
    encoder_feed.gpio_b = GPIO_ENCODER_FEED_B;
    encoder_feed.min_value = 0;    
    encoder_feed.max_value = 100;
    encoder_feed.factor = 2;
    encoder_feed.current_value = 100;

    button_ok.gpio_a = GPIO_OK;
    button_spindle.gpio_a = GPIO_SPINDLE;
    button_prog.gpio_a = GPIO_PROG;


    // blinky
    for(int i=0; i < 3; i++) {
        gpio_put(LED_PIN, 0);
        sleep_ms(100);
        gpio_put(LED_PIN, 1);
        sleep_ms(100);
    }

    //setup_mock();
    //mode_normal();
    mode_identify();


    puts("Started ...\n");
    printhex(txdata, 15);


    // Enable the interrupts we want
    *I2C0_INTR_MASK = (I2C_INTR_MASK_READ_REQ | I2C_INTR_MASK_RX_FULL | I2C_INTR_MASK_START_DET | I2C_INTR_MASK_STOP_DET | I2C_INTR_MASK_TX_ABRT);

    // Set up the interrupt handlers
    irq_set_exclusive_handler(I2C0_IRQ, i2c0_irq_handler);
    // Enable I2C interrupts
    irq_set_enabled(I2C0_IRQ, true);

    uint8_t last_mode = 0;

    uint32_t last_trigger = current_tick();

    adc_feed_raw = 32000;
    adc_spindle_raw = 32000;

    uint8_t b1, b2 = 0;

    while (1) {

        rotary_task2(&encoder_spindle);
        rotary_task2(&encoder_feed);
        button_task(&button_ok);
        button_task(&button_spindle);
        button_task(&button_prog);

        if (last_value_spindle != encoder_spindle.current_value) {
            last_value_spindle = encoder_spindle.current_value;
            adc_spindle_raw = (last_value_spindle * 65500) / encoder_spindle.max_value;
        }

        if (last_value_feed != encoder_feed.current_value) {
            last_value_feed = encoder_feed.current_value;
            adc_feed_raw = (last_value_feed * 65500) / encoder_feed.max_value;
        }

        // read adc values       
        adc_select_input(0);
        adc_x_raw = adc_read() * 65500/4096;        
        adc_select_input(1);
        adc_y_raw = adc_read() * 65500/4096;
        adc_select_input(2);
        adc_z_raw = adc_read() * 65500/4096;

    }
}