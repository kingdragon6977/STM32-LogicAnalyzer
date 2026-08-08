#include "stm32f10x.h"
#include "gpio.h"
#include "uart.h"
#include "i2c_master.h"
#include "board.h"
/*
 * This bit-banged I2C uses LOGIC_GPIO_PORT PA0 = CH0 = SDA, PA1 = CH1 = SCL
 * It temporarily reconfigures these pins as open-drain outputs.
 *
 * It is intentionally minimal and conservative (low speed).
 */

#define SDA_PIN CH0_PIN
#define SCL_PIN CH1_PIN
#define PORT    LOGIC_GPIO_PORT

/* micro delay loop — tune by changing iterations */
static volatile uint32_t i2c_delay_count = 300; /* default: ~50-100 kHz depending on CPU */

static void i2c_delay(void)
{
    volatile uint32_t i = i2c_delay_count;
    while(i--) __asm volatile("nop");
}

static void sda_out(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD; /* open-drain output */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PORT, &gpio);
}

static void scl_out(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PORT, &gpio);
}

static void sda_in(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PORT, &gpio);
}

static void scl_in(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PORT, &gpio);
}

static void sda_write(int level)
{
    if(level)
        PORT->BSRR = SDA_PIN;   /* release: drive 1 (open-drain released) */
    else
        PORT->BRR = SDA_PIN;    /* drive low */
}

static void scl_write(int level)
{
    if(level)
        PORT->BSRR = SCL_PIN;
    else
        PORT->BRR = SCL_PIN;
}

static int sda_read(void)
{
    return (PORT->IDR & SDA_PIN) ? 1 : 0;
}

/* bus idle -> both released (high) */
void i2c_master_init(void)
{
    /* configure both pins as open-drain outputs and set high (released) */
    sda_out();
    scl_out();
    sda_write(1);
    scl_write(1);
    uart_print("I2C master initialized (CH0=SDA, CH1=SCL)\r\n");
}

/* start: SDA high->low while SCL high */
static void i2c_start(void)
{
    sda_write(1); i2c_delay();
    scl_write(1); i2c_delay();
    sda_write(0); i2c_delay();
    scl_write(0); i2c_delay();
}

/* stop: SDA low->high while SCL high */
static void i2c_stop(void)
{
    sda_write(0); i2c_delay();
    scl_write(1); i2c_delay();
    sda_write(1); i2c_delay();
}

/* write 8 bits MSB first. Return ack bit (0 = ACK) */
static int i2c_write_byte_raw(uint8_t b)
{
    for(int i=7;i>=0;i--)
    {
        int bit = (b >> i) & 1;
        sda_write(bit);
        i2c_delay();
        scl_write(1);
        i2c_delay();
        scl_write(0);
        i2c_delay();
    }
    /* ack bit - release SDA and pulse SCL, then read SDA */
    sda_write(1); /* release */
    i2c_delay();
    scl_write(1);
    i2c_delay();
    int ack = sda_read();
    scl_write(0);
    i2c_delay();
    return ack; /* 0 = ACK */
}

/* write register with single byte value; returns 1 if ACK seen, 0 if NACK */
int i2c_master_write_byte(uint8_t addr7, uint8_t reg, uint8_t value)
{
    i2c_start();
    uint8_t addr_rw = (addr7 << 1) | 0; /* write */
    int ack = i2c_write_byte_raw(addr_rw);
    if(ack) { i2c_stop(); return 0; }
    ack = i2c_write_byte_raw(reg);
    if(ack) { i2c_stop(); return 0; }
    ack = i2c_write_byte_raw(value);
    if(ack) { i2c_stop(); return 0; }
    i2c_stop();
    return 1;
}

/* scan all 7-bit addresses, print responders */
void i2c_master_scan(void)
{
    uart_print("I2C scan start...\r\n");
    for(uint8_t a=1; a < 0x7F; ++a)
    {
        i2c_start();
        uint8_t addr_rw = (a << 1) | 0; /* write */
        int ack = i2c_write_byte_raw(addr_rw);
        if(!ack)
        {
            uart_print("Found device at 0x");
            uart_print_hex8(a);
            uart_print("\r\n");
        }
        i2c_stop();
        for(volatile int i=0;i<2000;i++) __asm volatile("nop");
    }
    uart_print("I2C scan done\r\n");
}

/* allow speed tuning */
void i2c_master_set_delay(uint32_t d)
{
    if(d < 10) d = 10;
    i2c_delay_count = d;
}
