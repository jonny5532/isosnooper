#include <stdio.h>
#include "pico/stdlib.h"

#include "isospi_master.h"
#include "isosnoop.h"

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

#define LED_PIN 25

int main() {
    stdio_usb_init();
    //while (!stdio_usb_connected()) {}
    
    isospi_master_setup(20, 2); // pins 20+21
    isosnoop_setup(18, true, 16);
    
    // printf("waiting...\n");
// sleep_ms(5000);
    // printf("continuing...\n");

    while(true) {
        sleep_us(1000);
        char tx[] = {0b10101010, 0b11111111, 0b00000000, 0b11001100, 0b00110011};
        char rx[sizeof(tx)] = {0};

        bool valid = isospi_write_read_blocking(tx, rx, sizeof(tx));
        //printf("Valid: %d\n", valid);

        isosnoop_print_buffer();
    }
}
