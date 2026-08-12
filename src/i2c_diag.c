#include "stm32f10x.h"
#include "board.h"
#include "uart.h"
#include "i2c_master.h"

#define I2C_PORT I2C_MASTER_GPIO_PORT
#define SDA_PIN  I2C_MASTER_SDA_PIN
#define SCL_PIN  I2C_MASTER_SCL_PIN

static void print_hex16(uint16_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_putc(hex[(value >> 12) & 0x0F]);
    uart_putc(hex[(value >> 8) & 0x0F]);
    uart_putc(hex[(value >> 4) & 0x0F]);
    uart_putc(hex[value & 0x0F]);
}

static void configure_input_floating(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = SDA_PIN | SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(I2C_PORT, &gpio);
}

static void configure_open_drain(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = SDA_PIN | SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(I2C_PORT, &gpio);

    /* Release both I2C lines. External pull-ups should take them high. */
    I2C_PORT->BSRR = SDA_PIN | SCL_PIN;
}

void i2c_master_release_pins(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    configure_input_floating();

    uart_print("I2C pins RELEASED (floating inputs)\r\n");
    uart_print("External pull-ups must pull SDA/SCL high.\r\n");
    i2c_master_pin_diagnostic();
}

void i2c_master_drive_pins(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    configure_open_drain();

    uart_print("I2C pins OPEN-DRAIN, both released high\r\n");
    i2c_master_pin_diagnostic();
}

void i2c_master_pin_diagnostic(void)
{
    uint8_t sda = (I2C_PORT->IDR & SDA_PIN) ? 1 : 0;
    uint8_t scl = (I2C_PORT->IDR & SCL_PIN) ? 1 : 0;
    uint8_t odr_sda = (I2C_PORT->ODR & SDA_PIN) ? 1 : 0;
    uint8_t odr_scl = (I2C_PORT->ODR & SCL_PIN) ? 1 : 0;

    uart_print("\r\nI2C PIN DIAGNOSTIC\r\n");
    uart_print("PC9  SDA: IDR=");
    uart_print_uint(sda);
    uart_print(" ODR=");
    uart_print_uint(odr_sda);
    uart_print("\r\n");

    uart_print("PC12 SCL: IDR=");
    uart_print_uint(scl);
    uart_print(" ODR=");
    uart_print_uint(odr_scl);
    uart_print("\r\n");

    uart_print("GPIOC CRH=0x");
    print_hex16((uint16_t)(I2C_PORT->CRH >> 16));
    uart_print("\r\n");

    uart_print("IDR=0x");
    print_hex16((uint16_t)I2C_PORT->IDR);
    uart_print(" ODR=0x");
    print_hex16((uint16_t)I2C_PORT->ODR);
    uart_print("\r\n");
}
