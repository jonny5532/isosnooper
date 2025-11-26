#include "isospi_device.pio.h"

#include "pico/stdlib.h"

#define ISOSPI_DEVICE_PIO pio2
#
void isospi_device_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the driver enable pin (active high)
    // tx_pin_base + 1  is the tx data pin

    // rx_pin_base      is the high rx data pin
    // rx_pin_base + 1  is the low rx data pin

    isospi_device_program_init(ISOSPI_DEVICE_PIO, tx_pin_base, rx_pin_base);
}
