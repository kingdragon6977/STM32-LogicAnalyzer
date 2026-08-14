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
 * Hardware I2C START trigger
 *
 * Uses EXTI0 on CH0/PA0 (TS_SDA), with the ISR accepting the edge only
 * when CH1/PA1 (TS_SCL) is high. This is much more reliable than polling
 * for a short SDA START edge at 1 MHz.
 */
void gpio_i2c_trigger_arm(void);
uint8_t gpio_i2c_trigger_seen(void);

/*
 * Button
 *
 * Returns 1 once per press
 */

int button_pressed(void);

/*
 * Return current raw button level: 0 = pressed (active low), 1 = released
 */
int button_level(void);



/*
 * LED control
 */

void led_on(void);

void led_off(void);

void led_toggle(void);



#endif
