#include <stdio.h>
#include "pico/stdlib.h"

#include "isospi_master.h"
#include "isospi_scope.h"
#include "isospi_device.h"
#include "isosnoop.h"

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

#define LED_PIN 25

int main() {
    stdio_usb_init();
    //while (!stdio_usb_connected()) {}
    sleep_ms(4000);
    
    isospi_master_setup(20, 18); // pins 20+21
    isosnoop_setup(18, true, 16);
    isospi_device_setup(26, 18);
    //isospi_scope_setup(18, true);
    
    // printf("waiting...\n");
// sleep_ms(5000);
    // printf("continuing...\n");
    // while(true) {
    //     sleep_us(10000);
    //     isosnoop_print_buffer();
    // }

    // isospi_tune(
    //     123,
    //     123,
    //     15,
    //     123,
    //     123,
    //     123
    // );


    printf("Running tests...\n");
    int passed = isospi_write_tests(50);
    printf("Tests passed: %d/50\n", passed);

    //isospi_calibrate();

    sleep_ms(4000);

    char WAKEUP[] = {0x2a, 0xd4};
    char UNMUTE[] = {0x21, 0xf2};
    char SNAPSHOT[] = {0x2B, 0xFB};
    char READ_A[] = {0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
   
    while(true) {
        //char tx[] = {0b10101010, 0b11111111, 0b00000000, 0b11001100, 0b00110011};
        char tx[] = {0x2a, 0xd4};
        char rx[sizeof(WAKEUP) + sizeof(UNMUTE) + sizeof(SNAPSHOT) + sizeof(READ_A)] = {0};

        //isospi_scope_flush();
        //bool valid = isospi_write_read_blocking(tx, rx, sizeof(tx));

        // isospi_tune(
        //     120, // prescaler (0-lots)
        //     30,  // cs_pulse_length (1-32)
        //     10,  // data_pulse_length (1-32)
        //     16,  // pre_rx_delay (1-32)
        //     30,  // reply_wait (1-32)
        //     6,   // sample_pos_1 
        //     16,  // sample_pos_2
        //     32   // post_rx_delay (1-32)
        // );
        
        //isospi_invert_first_chip_select(true);
        bool valid = isospi_write_read_blocking(WAKEUP, rx, sizeof(tx));

        //printf("rx: %02x %02x %d\n", rx[0], rx[1], valid ? 1 : 0);

        //isospi_invert_first_chip_select(false);
        valid = isospi_write_read_blocking(UNMUTE, rx+2, sizeof(UNMUTE)) && valid;
        valid = isospi_write_read_blocking(SNAPSHOT, rx+4, sizeof(SNAPSHOT)) && valid;
        valid = isospi_write_read_blocking(READ_A, rx+6, sizeof(READ_A)) && valid;

        printf("rx: %02x%02x %02x%02x %02x%02x %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %d\n",
            rx[0], rx[1],
            rx[2], rx[3],
            rx[4], rx[5],
            rx[6], rx[7], rx[8], rx[9], rx[10], rx[11], rx[12], rx[13], rx[14], rx[15], rx[16], rx[17],
            valid ? 1 : 0
        );

        sleep_us(2);

        isosnoop_print_buffer();

        //print_isospi_scope_output();

        sleep_us(1000);
    }
}
