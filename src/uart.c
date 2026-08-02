#include "stm32f10x.h"

#include "uart.h"
#include "board.h"


void uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;


    /*
     * Enable clocks
     */

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_USART1,
        ENABLE
    );


    /*
     * PA9 USART1 TX
     */

    gpio.GPIO_Pin = UART_TX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(UART_GPIO_PORT, &gpio);



    /*
     * PA10 USART1 RX
     */

    gpio.GPIO_Pin = UART_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;

    GPIO_Init(UART_GPIO_PORT, &gpio);



    /*
     * USART configuration
     */

    USART_StructInit(&uart);

    uart.USART_BaudRate = 115200;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;

    uart.USART_Mode =
        USART_Mode_Tx |
        USART_Mode_Rx;


    USART_Init(UART_USART, &uart);

    USART_Cmd(UART_USART, ENABLE);
}



/*
 * Send character
 */

void uart_putc(char c)
{
    while(!(UART_USART->SR & USART_SR_TXE));

    UART_USART->DR = c;
}



/*
 * Check RX buffer
 */

uint8_t uart_available(void)
{
    return (UART_USART->SR & USART_SR_RXNE) ? 1 : 0;
}



/*
 * Receive character
 */

char uart_getc(void)
{
    while(!uart_available());

    return (char)(UART_USART->DR & 0xFF);
}



/*
 * Print string
 */

void uart_print(const char *s)
{
    while(*s)
    {
        uart_putc(*s++);
    }
}



/*
 * Print decimal integer
 */

void uart_print_uint(uint32_t value)
{
    char buf[11];

    int i = 0;


    if(value == 0)
    {
        uart_putc('0');
        return;
    }


    while(value)
    {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }


    while(i)
    {
        uart_putc(buf[--i]);
    }
}



/*
 * Hex helpers
 */

void uart_print_hex8(uint8_t value)
{
    static const char hex[] =
        "0123456789ABCDEF";


    uart_putc(hex[(value >> 4) & 0x0F]);
    uart_putc(hex[value & 0x0F]);
}



void uart_print_hex16(uint16_t value)
{
    uart_print_hex8(value >> 8);
    uart_print_hex8(value);
}



void uart_print_hex32(uint32_t value)
{
    uart_print_hex16(value >> 16);
    uart_print_hex16(value);
}



/*
 * Startup message
 */

void uart_print_banner(void)
{
    uart_print("\r\n");
    uart_print("==============================\r\n");
    uart_print(" STM32 Logic Analyzer v1.0\r\n");
    uart_print("==============================\r\n");

    uart_print("CPU : STM32F103RBT6\r\n");
    uart_print("Clock : 72 MHz\r\n");
    uart_print("UART : USART1 115200\r\n");

    uart_print("\r\nReady\r\n\r\n");
}
