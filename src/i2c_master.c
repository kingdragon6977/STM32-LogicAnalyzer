#include "stm32f10x.h"
#include "gpio.h"
#include "uart.h"
#include "i2c_master.h"
#include "board.h"
#include "sampler.h"
#include "dma_capture.h"

/*
 * Conservative bit-banged I2C master.
 *
 * PC9  = SDA
 * PC12 = SCL
 *
 * The external bus must have 3.3 V pull-up resistors. The STM32 pins are
 * open-drain: writing a logic 1 releases the line rather than driving it high.
 */

#define I2C_PORT I2C_MASTER_GPIO_PORT
#define SDA_PIN  I2C_MASTER_SDA_PIN
#define SCL_PIN  I2C_MASTER_SCL_PIN

static volatile uint32_t i2c_delay_count = 300;

static void i2c_delay(void)
{
    volatile uint32_t i = i2c_delay_count;
    while(i--)
        __asm volatile("nop");
}

static void i2c_sda_open_drain(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(I2C_PORT, &gpio);
}

static void i2c_scl_open_drain(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(I2C_PORT, &gpio);
}

static void i2c_lines_release(void)
{
    I2C_PORT->BSRR = SDA_PIN | SCL_PIN;
}

static void sda_write(uint8_t high)
{
    if(high)
        I2C_PORT->BSRR = SDA_PIN;
    else
        I2C_PORT->BRR = SDA_PIN;
}

static void scl_write(uint8_t high)
{
    if(high)
        I2C_PORT->BSRR = SCL_PIN;
    else
        I2C_PORT->BRR = SCL_PIN;
}

static uint8_t sda_read(void)
{
    return (I2C_PORT->IDR & SDA_PIN) ? 1 : 0;
}

static uint8_t scl_read(void)
{
    return (I2C_PORT->IDR & SCL_PIN) ? 1 : 0;
}

static uint8_t scl_release_wait(void)
{
    uint32_t timeout = 10000;

    scl_write(1);

    while(!scl_read())
    {
        if(--timeout == 0)
            return 0;
    }

    return 1;
}

static uint8_t i2c_start(void)
{
    sda_write(1);

    if(!scl_release_wait())
        return 0;

    i2c_delay();

    sda_write(0);
    i2c_delay();

    scl_write(0);
    i2c_delay();

    return 1;
}

static void i2c_stop(void)
{
    sda_write(0);
    i2c_delay();

    if(scl_release_wait())
        i2c_delay();

    sda_write(1);
    i2c_delay();
}

/* Write one byte, MSB first. Returns 1 for ACK, 0 for NACK/bus failure. */
static uint8_t i2c_write_byte_raw(uint8_t value)
{
    int bit;

    for(bit = 7; bit >= 0; bit--)
    {
        sda_write((value >> bit) & 1);
        i2c_delay();

        if(!scl_release_wait())
            return 0;

        i2c_delay();
        scl_write(0);
        i2c_delay();
    }

    /* ACK bit: release SDA and let the slave drive it. */
    sda_write(1);
    i2c_delay();

    if(!scl_release_wait())
        return 0;

    i2c_delay();
    value = !sda_read();

    scl_write(0);
    i2c_delay();

    return value;
}

void i2c_master_init(void)
{
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOC,
        ENABLE
    );

    i2c_sda_open_drain();
    i2c_scl_open_drain();
    i2c_lines_release();
    i2c_delay();

    uart_print("I2C master: PC9=SDA PC12=SCL\r\n");

    if(sda_read() && scl_read())
        uart_print("I2C bus idle/high\r\n");
    else
        uart_print("WARNING: I2C bus not idle\r\n");
}

void i2c_master_scan(void)
{
    uint8_t address;
    uint8_t found = 0;

    uart_print("I2C scan start\r\n");
    uart_print("Addresses 0x08-0x77\r\n");

    if(!scl_read() || !sda_read())
    {
        uart_print("I2C BUS BUSY/STUCK LOW\r\n");
        uart_print("Check 3.3V pull-ups and SDA/SCL wiring\r\n");
        return;
    }

    for(address = 0x08; address <= 0x77; address++)
    {
        uint8_t ack;

        if(!i2c_start())
        {
            uart_print("I2C START failed / SCL stuck low\r\n");
            break;
        }

        ack = i2c_write_byte_raw((uint8_t)(address << 1));
        i2c_stop();

        if(ack)
        {
            uart_print("ACK 0x");
            uart_print_hex8(address);
            uart_print("\r\n");
            found = 1;
        }

        i2c_delay();
    }

    if(!found)
        uart_print("No I2C devices ACKed\r\n");

    uart_print("I2C scan done\r\n");
}

int i2c_master_write_byte(uint8_t addr7, uint8_t reg, uint8_t value)
{
    uint8_t ack;

    if(!i2c_start())
        return 0;

    ack = i2c_write_byte_raw((uint8_t)(addr7 << 1));
    if(!ack)
    {
        i2c_stop();
        return 0;
    }

    ack = i2c_write_byte_raw(reg);
    if(!ack)
    {
        i2c_stop();
        return 0;
    }

    ack = i2c_write_byte_raw(value);
    if(!ack)
    {
        i2c_stop();
        return 0;
    }

    i2c_stop();
    return 1;
}

/* Generate known I2C traffic for a simple electrical test. */
int i2c_master_test_transaction(void)
{
    uint8_t ack_addr;
    uint8_t ack_reg;
    uint8_t ack_data1;
    uint8_t ack_data2;

    uart_print("I2C loopback test: START 0x50 12 34 56 STOP\r\n");

    if(!i2c_start())
    {
        uart_print("I2C test failed: START/SCL\r\n");
        return 0;
    }

    ack_addr  = i2c_write_byte_raw(0x50 << 1);
    ack_reg   = i2c_write_byte_raw(0x12);
    ack_data1 = i2c_write_byte_raw(0x34);
    ack_data2 = i2c_write_byte_raw(0x56);

    i2c_stop();

    uart_print("ACKs: addr="); uart_print_uint(ack_addr);
    uart_print(" reg="); uart_print_uint(ack_reg);
    uart_print(" data1="); uart_print_uint(ack_data1);
    uart_print(" data2="); uart_print_uint(ack_data2);
    uart_print("\r\n");

    return 1;
}

/*
 * Synchronized self-test:
 *
 *   PA0 <- PC9  (SDA)
 *   PA1 <- PC12 (SCL)
 *
 * DMA is armed BEFORE the master starts the transaction, so there is no
 * race between "capture" and "generate". The captured waveform is then
 * decoded locally. This validates the complete path without requiring a
 * second I2C device.
 */
int i2c_master_capture_test(void)
{
    static uint8_t capture_buffer[CAPTURE_SAMPLES];
    uint32_t i;
    uint32_t starts = 0;
    uint32_t stops = 0;
    uint32_t bytes = 0;
    uint32_t acks = 0;
    uint32_t nacks = 0;
    uint8_t in_frame = 0;
    uint8_t bit_count = 0;
    uint8_t shift = 0;
    uint8_t first_byte = 1;
    uint8_t prev;

    uart_print("\r\nI2C ANALYZER SELF-TEST\r\n");
    uart_print("Wiring: PC9->PA0 (SDA), PC12->PA1 (SCL)\r\n");
    uart_print("Capture rate forced to 1 MHz\r\n");

    /* TIM2 is the DMA sample clock. */
    sampler_set_rate(1000000);

    /* Make sure the master pins are released before arming DMA. */
    i2c_master_init();

    dma_capture_start(capture_buffer, CAPTURE_SAMPLES);

    /* Generate the known transaction while DMA is already running. */
    if(!i2c_start())
    {
        uart_print("SELF-TEST ERROR: I2C START failed\r\n");
        while(!dma_capture_done()) { }
        return 0;
    }

    (void)i2c_write_byte_raw(0x50 << 1);
    (void)i2c_write_byte_raw(0x12);
    (void)i2c_write_byte_raw(0x34);
    (void)i2c_write_byte_raw(0x56);
    i2c_stop();

    while(!dma_capture_done()) { }

    uart_print("DMA capture complete\r\n");
    uart_print("Passive decode: CH0=SDA CH1=SCL\r\n");

    prev = (uint8_t)(capture_buffer[0] & 0x03);

    for(i = 1; i < CAPTURE_SAMPLES; i++)
    {
        uint8_t curr = (uint8_t)(capture_buffer[i] & 0x03);
        uint8_t prev_sda = prev & 0x01;
        uint8_t curr_sda = curr & 0x01;
        uint8_t prev_scl = (prev >> 1) & 0x01;
        uint8_t curr_scl = (curr >> 1) & 0x01;

        /* START: SDA falls while SCL is high. */
        if(prev_sda && !curr_sda && prev_scl && curr_scl)
        {
            starts++;
            in_frame = 1;
            bit_count = 0;
            shift = 0;
            first_byte = 1;
            uart_print("START @ ");
            uart_print_uint(i);
            uart_print("\r\n");
        }

        /* STOP: SDA rises while SCL is high. */
        if(!prev_sda && curr_sda && prev_scl && curr_scl)
        {
            if(in_frame)
            {
                stops++;
                uart_print("STOP @ ");
                uart_print_uint(i);
                uart_print("\r\n");
            }
            in_frame = 0;
            bit_count = 0;
            shift = 0;
            first_byte = 1;
        }

        /* Sample SDA on every rising SCL edge. */
        if(!prev_scl && curr_scl && in_frame)
        {
            if(bit_count < 8)
            {
                shift = (uint8_t)((shift << 1) | curr_sda);
                bit_count++;
            }
            else
            {
                uint8_t byte_value = shift;

                if(first_byte)
                {
                    uart_print("ADDR 0x");
                    uart_print_hex8((uint8_t)(byte_value >> 1));
                    uart_print((byte_value & 1) ? " R " : " W ");
                }
                else
                {
                    uart_print("DATA 0x");
                    uart_print_hex8(byte_value);
                    uart_print(" ");
                }

                if(curr_sda)
                {
                    nacks++;
                    uart_print("NACK\r\n");
                }
                else
                {
                    acks++;
                    uart_print("ACK\r\n");
                }

                bytes++;
                first_byte = 0;
                bit_count = 0;
                shift = 0;
            }
        }

        prev = curr;
    }

    uart_print("\r\nI2C SELF-TEST SUMMARY\r\n");
    uart_print("STARTs : "); uart_print_uint(starts); uart_print("\r\n");
    uart_print("STOPs  : "); uart_print_uint(stops); uart_print("\r\n");
    uart_print("Bytes  : "); uart_print_uint(bytes); uart_print("\r\n");
    uart_print("ACKs   : "); uart_print_uint(acks); uart_print("\r\n");
    uart_print("NACKs  : "); uart_print_uint(nacks); uart_print("\r\n");

    return (starts > 0 && stops > 0 && bytes >= 4) ? 1 : 0;
}

void i2c_master_set_delay(uint32_t d)
{
    if(d < 10)
        d = 10;

    i2c_delay_count = d;
}
