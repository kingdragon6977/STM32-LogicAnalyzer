#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>


/*
 * Initialize:
 *
 * PA0-PA3  logic inputs
 * PB0      capture button
 * PB2      status LED
 */

void gpio_init(void);



/*
 * Logic analyzer input
 *
 * Returns:
 *
 * bit0 = CH0 PA0
 * bit1 = CH1 PA1
 * bit2 = CH2 PA2
 * bit3 = CH3 PA3
 */

uint8_t logic_read(void);

void test_pin_toggle(void);

/*
 * Button
 *
 * Returns 1 once per press
 */

int button_pressed(void);



/*
 * LED control
 */

void led_on(void);

void led_off(void);

void led_toggle(void);



#endif
