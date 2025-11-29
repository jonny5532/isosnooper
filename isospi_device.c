#include "isospi_device.pio.h"

#include "pico/stdlib.h"

#include <stdio.h>

#define ISOSPI_DEVICE_PIO pio2
#define ISOSPI_DEVICE_RX_SM 0
#define ISOSPI_DEVICE_TX_SM 0
#define ISOSPI_DEVICE_PIO_IRQ_RX 0
#define ISOSPI_DEVICE_PIO_IRQ_CS1 1
#define PIO_IRQ_PRIORITY PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY

void isospi_device_flush();

uint8_t rx[256];
int rx_byte_index = 0;

// the 7 bit-nibbles that get carried over into the next send (weird 1-bit offset)
uint32_t carried_over_output = 0;

uint8_t read_a_response[] = {0xea, 0xb6, 0x2f, 0xb7, 0xfe, 0xb6, 0x6a, 0xf9, 0xff, 0xff, 0xff, 0xff};

static uint32_t encode_byte(uint8_t byte) {
    uint32_t output = 0;
    for(int i=0;i<8;i++) {
        output <<= 4;
        output |= (byte & 0x80 ) ? 0b1110 : 0b1011;
        byte <<= 1;
    }
    return output;
}

void __isr __not_in_flash_func(isospi_device_rx_available)() {
    // Handle the IRQ (e.g., read data from RX FIFO)
    if(pio_sm_is_rx_fifo_empty(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_RX_SM)) {
        printf("irq with empty fifo!\n");
    } else {
        uint32_t data = pio_sm_get(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_RX_SM);
        uint32_t output = 0;
        rx[rx_byte_index] = data & 0xff;

        //printf("data: %02x @ %d\n", data, rx_byte_index);

        // possible bit-nibble values
        // 0x0 sends nothing (idle)
        // 0xe sends a one
        // 0xb sends a zero

        if(rx_byte_index==0 && data==0x2B) {
            // first half of a SNAPSHOT, respond with a single 0 in the next byte
            output = 0xb0000000;
        } else if(rx_byte_index>=1 && rx[0]==0x47 && rx[1]==0x00) {
            // we're replying to a READ_A
            output = encode_byte(read_a_response[rx_byte_index-1]);
        }

        // combine carried over bits from last time with MSB of this
        uint32_t to_send = carried_over_output | (output >> (7*4));
        // store other 7 bit-nibbles of this to be carried over next time
        carried_over_output = (output & 0x0FFFFFFF) << 4;

        pio_sm_put(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_TX_SM, to_send);

        rx_byte_index++;
    }

    // no need to clear the interrupt, reading from the RX FIFO does that??
}

void __isr __not_in_flash_func(isospi_device_irq2)() {
    // got a CS1, which indicates either the start or end of a transfer
    isospi_device_flush();

    rx_byte_index = 0;
    carried_over_output = 0;

    pio_interrupt_clear(ISOSPI_DEVICE_PIO, 2);
}

inline void isospi_device_flush() {
    // flush any remaining data in the PIO RX FIFO
    // while(!pio_sm_is_rx_fifo_empty(ISOSPI_DEVICE_PIO, 0)) {
    //     pio_sm_get_blocking(ISOSPI_DEVICE_PIO, 0);
    // }

    pio_sm_clear_fifos(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_RX_SM);


    // empty the ISR
    pio_sm_exec(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_RX_SM, pio_encode_mov(pio_isr, pio_null));

    // empty the OSR
    pio_sm_exec(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_TX_SM, pio_encode_mov(pio_osr, pio_null));
}

void isospi_device_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the tx data pin (noninverting)
    // tx_pin_base + 1  is the driver enable pin (active high)

    // rx_pin_base      is the low rx data pin
    // rx_pin_base + 1  is the high rx data pin

    isospi_device_program_init(ISOSPI_DEVICE_PIO, tx_pin_base, rx_pin_base);

    // set up rx-available interrupt
    irq_add_shared_handler(
        pio_get_irq_num(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_PIO_IRQ_RX),
        isospi_device_rx_available,
        PIO_IRQ_PRIORITY
    );
    pio_set_irqn_source_enabled(
        ISOSPI_DEVICE_PIO, 
        ISOSPI_DEVICE_PIO_IRQ_RX, 
        pio_get_rx_fifo_not_empty_interrupt_source(0),
        true
    );
    irq_set_enabled(pio_get_irq_num(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_PIO_IRQ_RX), true);

    // set up cs1 interrupt
    irq_add_shared_handler(
        pio_get_irq_num(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_PIO_IRQ_CS1), 
        isospi_device_irq2, 
        PIO_IRQ_PRIORITY
    );
    pio_set_irqn_source_enabled(
        ISOSPI_DEVICE_PIO, 
        ISOSPI_DEVICE_PIO_IRQ_CS1,
        pis_interrupt2,
        true
    );
    irq_set_enabled(pio_get_irq_num(ISOSPI_DEVICE_PIO, ISOSPI_DEVICE_PIO_IRQ_CS1), true);

}
