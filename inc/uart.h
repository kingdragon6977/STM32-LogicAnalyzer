#ifndef UART_H
#define UART_H

#include <stdint.h>


/*
 * UART Driver
 *
 * USART1:
 * TX -> PA9
 * RX -> PA10
 *
 * 115200 8N1
 */


/* Initialize USART1 */
void uart_init(void);


/* Send one character */
void uart_putc(char c);


/* Receive one character */
char uart_getc(void);


/* Check if a character is waiting */
uint8_t uart_available(void);


/* Print a string */
void uart_print(const char *s);


/* Print unsigned integer */
void uart_print_uint(uint32_t value);


/* Print hexadecimal values */
void uart_print_hex8(uint8_t value);
void uart_print_hex16(uint16_t value);
void uart_print_hex32(uint32_t value);


/* Print analyzer startup banner */
void uart_print_banner(void);


#endif
