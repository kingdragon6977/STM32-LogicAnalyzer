#ifndef BOARD_H
#define BOARD_H

#include "stm32f10x.h"


/*
 * STM32F103RBT6 pin map
 */


/*
 * UART
 */

#define UART_USART       USART1
#define UART_GPIO_PORT   GPIOA

#define UART_TX_PIN      GPIO_Pin_9
#define UART_RX_PIN      GPIO_Pin_10


/*
 * Logic analyzer inputs
 *
 * CH0 PA0
 * CH1 PA1
 * CH2 PA2
 * CH3 PA3
 */

#define LOGIC_GPIO_PORT GPIOA

#define CH0_PIN GPIO_Pin_0
#define CH1_PIN GPIO_Pin_1
#define CH2_PIN GPIO_Pin_2
#define CH3_PIN GPIO_Pin_3


/*
 * Hardware test signal
 *
 * TIM3_CH1 output on PA6
 */

#define TEST_SIGNAL_PORT GPIOA
#define TEST_SIGNAL_PIN  GPIO_Pin_6
#define TEST_SIGNAL_TIMER TIM3


/*
 * Button
 *
 * PB0 active low
 */

#define BUTTON_PORT GPIOB
#define BUTTON_PIN  GPIO_Pin_0


/*
 * Status LED
 *
 * PB2
 */

#define LED_PORT GPIOB
#define LED_PIN  GPIO_Pin_2


#endif
