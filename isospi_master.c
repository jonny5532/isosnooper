#include "isospi_master.pio.h"

#include "pico/stdlib.h"

#include <stdio.h>

#define ISOSPI_MASTER_PIO pio0
#define ISOSPI_MASTER_SM 0

void isospi_master_setup(uint tx_pin_base, uint rx_pin_base) {
    // tx_pin_base      is the driver enable pin (active high)
    // tx_pin_base + 1  is the tx data pin

    // rx_pin_base      is the high rx data pin
    // rx_pin_base + 1  is the low rx data pin

    isospi_master_program_init(ISOSPI_MASTER_PIO, tx_pin_base, rx_pin_base);
}

void isospi_master_flush() {
    // flush any remaining data in the PIO RX FIFO
    while(!pio_sm_is_rx_fifo_empty(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM)) {
        pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
    }

    // empty the ISR
    pio_sm_exec(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, pio_encode_mov(pio_isr, pio_null));
}

void isospi_tune(
    uint32_t prescaler,
    uint8_t cs_pulse_length,
    uint8_t data_pulse_length,
    uint8_t pre_rx_delay,
    uint8_t sample_pos_1,
    uint8_t sample_pos_2
) {
    //pio_sm_set_enabled(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, false);
    //pio_sm_set_clkdiv_int_frac8(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, 1 + (prescaler>>8), prescaler & 0xff);

#define SET_DELAY(offset, value) \
   ISOSPI_MASTER_PIO->instr_mem[master_offset + offset] = \
       (isospi_master_program_instructions[offset] & 0xe0ff) | ((value & 0x1f) << 8);

    // SET_DELAY(isospi_master_offset_cs_high_1, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_high_2, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_low_1, cs_pulse_length - 1);
    // SET_DELAY(isospi_master_offset_cs_low_2, cs_pulse_length - 1);

    SET_DELAY(isospi_master_offset_data_high_1, data_pulse_length - 1);
    SET_DELAY(isospi_master_offset_data_high_2, data_pulse_length - 1);
    //printf("old func was %x\n", isospi_master_program_instructions[isospi_master_offset_data_low_1]);
    //ISOSPI_MASTER_PIO->instr_mem[master_offset + isospi_master_offset_data_low_1] = 0xEE02;
    SET_DELAY(isospi_master_offset_data_low_1, data_pulse_length - 1);
    SET_DELAY(isospi_master_offset_data_low_2, data_pulse_length - 1);

    // hmm
    // printf(
    //     "setting from %x to %x\n", 
    //     isospi_master_program_instructions[isospi_master_offset_data_low_1], 
    //     (isospi_master_program_instructions[isospi_master_offset_data_low_1] & 0xe0ff) | (((data_pulse_length - 1) & 0x1f) << 8)
    // );
    // SET_DELAY(isospi_master_offset_data_low_1, data_pulse_length - 1);
    
    // SET_DELAY(isospi_master_offset_data_low_2, data_pulse_length - 1);

    SET_DELAY(isospi_master_offset_pre_rx, pre_rx_delay - 1);
    SET_DELAY(isospi_master_offset_sample_1, sample_pos_1 - 1);
    SET_DELAY(isospi_master_offset_sample_2, sample_pos_2 - sample_pos_1 - 2);

    //pio_sm_restart(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
}

bool isospi_write_read_blocking(char* out_buf, char* in_buf, size_t len) {

    isospi_master_cs(true);

    sleep_us(3);

    bool valid = true;
    int carry_bit = 0;
    for(size_t i=0; i<len; i++) {
        // We write 8 bits at a time
        pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, out_buf[i] << 24);

        // Each response bit is encoded as a nibble in a 32 bit word
        uint32_t v = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);
        for(int r=0; r<8; r++) {
            uint8_t nibble = (v >> 28) & 0xf;
            v <<= 4;
            if(nibble==0b1001) {
                // bit 1
                in_buf[i] = (in_buf[i] << 1) | 0x1;
            } else if(nibble==0b0110) {
                // bit 0
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            } else {
                // invalid
                valid = false;
                in_buf[i] = (in_buf[i] << 1) | 0x0;
            }
        }

        int new_carry = in_buf[i] & 0x1;
        in_buf[i] = (in_buf[i] >> 1) | (carry_bit << 7);
        carry_bit = new_carry;

        sleep_us(1);
    }

    sleep_us(1);

    // perform final ending chip select
    isospi_master_cs(false);

    // flush any remaining data
    isospi_master_flush();

    sleep_us(10);

    return valid;
}

bool isospi_write_single_test() {
    // snapshot command (final bit will return something)
    char tx[] = {0x2b, 0xfb};
    char rx[2] = {0};

    //isospi_master_cs(true);
    sleep_us(5);

    //printf("put1\n");

    // Write first byte
    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx[0] << 24);

    //printf("get1\n");

    // First reply should be zeros (no reply)
    uint32_t reply1 = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);

    sleep_us(1);
    //isospi_master_cs(false);
    sleep_us(1);

    //isospi_master_cs(true);
    sleep_us(3);

    //printf("put2\n");

    pio_sm_put_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM, tx[0] << 24);

    //printf("get2\n");

    // Second reply should be nonzero
    uint32_t reply2 = pio_sm_get_blocking(ISOSPI_MASTER_PIO, ISOSPI_MASTER_SM);

    sleep_us(1);
    //isospi_master_cs(false);
    sleep_us(1);

    // flush any remaining data
    isospi_master_flush();
    
    //return (reply1 == 0) && (reply2 != 0);
    return reply1 != 0 && reply2 != 0;
}

int isospi_write_tests(int count) {
    int score = 0;
    for(int i=0; i<count; i++) {
        if(isospi_write_single_test()) {
            score++;
        }
    }
    return score;
}

#define PRESCALER_MIN 75
#define PRESCALER_MAX 125
#define CS_PULSE_LENGTH_MIN 20
#define CS_PULSE_LENGTH_MAX 32
#define DATA_PULSE_LENGTH_MIN 5
#define DATA_PULSE_LENGTH_MAX 25
#define PRE_RX_DELAY_MIN 1
#define PRE_RX_DELAY_MAX 32




void isospi_calibrate() {
    int tests = 20;
    int pass_threshold = 10;

    int prescaler = 0; //120;
    int cs_pulse_length = 30;
    int data_pulse_length = 15;
    int pre_rx_delay = 16;
    int sample_pos_1 = 10;
    int sample_pos_2 = 25;

    int prescaler_min=PRESCALER_MIN;
    int prescaler_max=PRESCALER_MAX;
    int cs_pulse_length_min=CS_PULSE_LENGTH_MIN;
    int cs_pulse_length_max=CS_PULSE_LENGTH_MAX;
    int data_pulse_length_min=DATA_PULSE_LENGTH_MIN;
    int data_pulse_length_max=DATA_PULSE_LENGTH_MAX;
    int pre_rx_delay_min=PRE_RX_DELAY_MIN;
    int pre_rx_delay_max=PRE_RX_DELAY_MAX;

    void _calibrate_value(int *value, int *min, int *max) {
        int cur = *value;
        for(*value=cur+1; *value <= *max; (*value)++) {
            isospi_tune(
                prescaler,
                cs_pulse_length,
                data_pulse_length,
                pre_rx_delay,
                sample_pos_1,
                sample_pos_2
            );
            int score = isospi_write_tests(tests);
            if(score < pass_threshold) {
                *max = *min < *value ? *value - 1 : *min;
                break;
            }
        }
        for(*value=cur-1; *value>=*min; (*value)--) {
            isospi_tune(
                prescaler,
                cs_pulse_length,
                data_pulse_length,
                pre_rx_delay,
                sample_pos_1,
                sample_pos_2
            );
            int score = isospi_write_tests(tests);
            if(score < pass_threshold) {
                *min = *max > *value ? *value + 1 : *max;
                break;
            }
        }
        *value = ( *min + *max ) / 2;
    }

    for(int round=0; round<10; round++) {
        // printf("Calibration round %d\n", round+1);
        // printf("  current range: prescaler %d - %d (%d)\n", prescaler_min, prescaler_max, prescaler);
        // _calibrate_value(&prescaler, &prescaler_min, &prescaler_max);
        // printf("    new range: prescaler %d - %d (%d)\n", prescaler_min, prescaler_max, prescaler);
        // printf("  current range: cs_pulse_length %d - %d (%d)\n", cs_pulse_length_min, cs_pulse_length_max, cs_pulse_length);
        // _calibrate_value(&cs_pulse_length, &cs_pulse_length_min, &cs_pulse_length_max);
        // printf("    new range: cs_pulse_length %d - %d (%d)\n", cs_pulse_length_min, cs_pulse_length_max, cs_pulse_length);

        // printf("  current range: data_pulse_length %d - %d (%d)\n", data_pulse_length_min, data_pulse_length_max, data_pulse_length);
        // _calibrate_value(&data_pulse_length, &data_pulse_length_min, &data_pulse_length_max);
        // printf("    new range: data_pulse_length %d - %d (%d)\n", data_pulse_length_min, data_pulse_length_max, data_pulse_length);

        int sample_pos_1_min = 5;
        int sample_pos_1_max = 15;
        int sample_pos_2_min = sample_pos_1 + 2;
        int sample_pos_2_max = sample_pos_2_min + 32;

        printf("  current range: sample_pos_1 %d - %d (%d)\n", sample_pos_1_min, sample_pos_1_max, sample_pos_1);
        _calibrate_value(&sample_pos_1, &sample_pos_1_min, &sample_pos_1_max);
        printf("    new range: sample_pos_1 %d - %d (%d)\n", sample_pos_1_min, sample_pos_1_max, sample_pos_1);

        printf("  current range: sample_pos_2 %d - %d (%d)\n", sample_pos_2_min, sample_pos_2_max, sample_pos_2);
        _calibrate_value(&sample_pos_2, &sample_pos_2_min, &sample_pos_2_max);
        printf("    new range: sample_pos_2 %d - %d (%d)\n", sample_pos_2_min, sample_pos_2_max, sample_pos_2);

        // printf("  current range: pre_rx_delay %d - %d (%d)\n", pre_rx_delay_min, pre_rx_delay_max, pre_rx_delay);
        // _calibrate_value(&pre_rx_delay, &pre_rx_delay_min, &pre_rx_delay_max);
        // printf("    new range: pre_rx_delay %d - %d (%d)\n", pre_rx_delay_min, pre_rx_delay_max, pre_rx_delay);
    }

    //printf("new range: prescaler %d - %d\n", prescaler_min, prescaler_max);
}
