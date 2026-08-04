#include "stm32f10x.h"

#include "gpio.h"
#include "board.h"



void gpio_init(void)
{
    GPIO_InitTypeDef gpio;


    /*
     * Enable GPIO clocks
     */

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB,
        ENABLE
    );

	/* GPIOC clock */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

	

	gpio.GPIO_Pin   = GPIO_Pin_9;
	gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;

	

   // PA0-PA3 analyzer inputs

	gpio.GPIO_Pin =
		GPIO_Pin_0 |
		GPIO_Pin_1 |
		GPIO_Pin_2 |
		GPIO_Pin_3;

	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_Init(GPIOA,&gpio);


    /*
     * Capture button
     *
     * Active low
     */

    gpio.GPIO_Pin =
        BUTTON_PIN;

    gpio.GPIO_Mode =
        GPIO_Mode_IPU;


    GPIO_Init(
        BUTTON_PORT,
        &gpio
    );



    /*
     * Status LED
     */

    gpio.GPIO_Pin =
        LED_PIN;

    gpio.GPIO_Mode =
        GPIO_Mode_Out_PP;

    gpio.GPIO_Speed =
        GPIO_Speed_2MHz;


    GPIO_Init(
        LED_PORT,
        &gpio
    );


    led_off();
}




uint8_t logic_read(void)
{
    return (uint8_t)(
        LOGIC_GPIO_PORT->IDR & 0x0F
    );
}




int button_pressed(void)
{
    static uint8_t lock = 0;


    /*
     * Button pressed = pin low
     */

    if(!(BUTTON_PORT->IDR & BUTTON_PIN))
    {
        if(!lock)
        {
            lock = 1;
            return 1;
        }
    }
    else
    {
        lock = 0;
    }


    return 0;
}


void test_pin_toggle(void)
{
    GPIOC->ODR ^= GPIO_Pin_9;
}


void led_on(void)
{
    LED_PORT->BRR = LED_PIN;
}



void led_off(void)
{
    LED_PORT->BSRR = LED_PIN;
}



void led_toggle(void)
{
    LED_PORT->ODR ^= LED_PIN;
}
