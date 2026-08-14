#include "stm32f10x.h"

#include "gpio.h"
#include "board.h"
#include "misc.h"


#define I2C_EXTI0 ((uint32_t)0x00000001u)

static volatile uint8_t i2c_start_seen = 0;


void gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB |
        RCC_APB2Periph_AFIO,
        ENABLE
    );

    gpio.GPIO_Pin =
        CH0_PIN |
        CH1_PIN |
        CH2_PIN |
        CH3_PIN;

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(LOGIC_GPIO_PORT, &gpio);

    /*
     * Hardware I2C START trigger:
     * PA0/CH0 is the SDA input and EXTI0 is mapped to port A.
     * Falling edges are enabled; the ISR verifies that PA1/SCL is high
     * before accepting the edge as a real I2C START condition.
     *
     * Use the EXTI0 bit directly instead of EXTI_Line0.  The project does
     * not include stm32f10x_exti.h, so the SPL EXTI_Line0 macro is not
     * available here.
     */
    AFIO->EXTICR[0] &= ~(0x0Fu << 0); /* EXTI0 = PA0 */
    EXTI->IMR &= ~I2C_EXTI0;
    EXTI->EMR &= ~I2C_EXTI0;
    EXTI->RTSR &= ~I2C_EXTI0;
    EXTI->FTSR |= I2C_EXTI0;
    EXTI->PR = I2C_EXTI0;

    {
        NVIC_InitTypeDef nvic;
        nvic.NVIC_IRQChannel = EXTI0_IRQn;
        nvic.NVIC_IRQChannelPreemptionPriority = 0;
        nvic.NVIC_IRQChannelSubPriority = 1;
        nvic.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&nvic);
    }

    gpio.GPIO_Pin = BUTTON_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;

    GPIO_Init(BUTTON_PORT, &gpio);

    gpio.GPIO_Pin = LED_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(LED_PORT, &gpio);

    led_off();
}


uint8_t logic_read(void)
{
    return (uint8_t)(
        LOGIC_GPIO_PORT->IDR & 0x0F
    );
}


void gpio_i2c_trigger_arm(void)
{
    i2c_start_seen = 0;
    EXTI->PR = I2C_EXTI0;
    EXTI->IMR |= I2C_EXTI0;
}


uint8_t gpio_i2c_trigger_seen(void)
{
    return i2c_start_seen;
}


/*
 * CH0/PA0 falling-edge ISR used only by the passive I2C trigger.
 * A falling SDA edge is an I2C START only when SCL is high.
 */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & I2C_EXTI0)
    {
        EXTI->PR = I2C_EXTI0;
        EXTI->IMR &= ~I2C_EXTI0;

        if (GPIOA->IDR & CH1_PIN)
            i2c_start_seen = 1;
    }
}


int button_pressed(void)
{
    static uint8_t lock = 0;

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


/* Return current raw button level: 0 = pressed (active low), 1 = released */
int button_level(void)
{
    return (BUTTON_PORT->IDR & BUTTON_PIN) ? 1 : 0;
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
