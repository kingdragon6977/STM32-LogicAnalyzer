#include "stm32f10x.h"
#include "gpio.h"
#include "uart.h"
#include "i2c_master.h"
#include "board.h"

/*
 * Conservative bit-banged I2C master.
 *
 * PC9  = SDA
 * PC12 = SCL
 *
 * The external bus must have 3.3 V pull-up resistors. The STM32 pins are
 * open-drain: writing a logic 1 releases the line rather than driving it high.
 *
 * This implementation is intentionally slow and includes SCL clock-stretch
 * checking so it is suitable for probing an unknown 3.3 V touchpad bus.
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

/*
 * Release SCL and wait for the slave to let it go high.
 * Returns 1 on success, 0 if the bus is stuck low.
 */
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

    /* ACK is active low. */
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

/*
 * Scan the normal 7-bit address range used by I2C devices.
 * Reserved addresses below 0x08 and above 0x77 are skipped.
 */
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

        /* Address byte: 7-bit address + write bit. */
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

/* Write register + one data byte. Returns 1 on complete ACK sequence. */
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

void i2c_master_set_delay(uint32_t d)
{
    if(d < 10)
        d = 10;

    i2c_delay_count = d;
}
