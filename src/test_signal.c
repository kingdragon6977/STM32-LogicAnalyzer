#include "stm32f10x.h"
#include "stm32f10x_tim.h"

#include "test_signal.h"


static uint32_t test_rate = 10000;


void test_signal_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;


    /*
     * Enable clocks
     */

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA,
        ENABLE
    );


    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM3,
        ENABLE
    );


    /*
     * PA6 = TIM3_CH1
     */

    gpio.GPIO_Pin =
        GPIO_Pin_6;

    gpio.GPIO_Mode =
        GPIO_Mode_AF_PP;

    gpio.GPIO_Speed =
        GPIO_Speed_50MHz;


    GPIO_Init(
        GPIOA,
        &gpio
    );


    /*
     * TIM3 base
     */

    tim.TIM_Prescaler = 0;

    tim.TIM_CounterMode =
        TIM_CounterMode_Up;

    tim.TIM_Period =
        (72000000 / test_rate) - 1;

    tim.TIM_ClockDivision =
        TIM_CKD_DIV1;


    TIM_TimeBaseInit(
        TIM3,
        &tim
    );


    /*
     * PWM channel 1
     */

    oc.TIM_OCMode =
        TIM_OCMode_PWM1;

    oc.TIM_OutputState =
        TIM_OutputState_Enable;

    oc.TIM_Pulse =
        tim.TIM_Period / 2;

    oc.TIM_OCPolarity =
        TIM_OCPolarity_High;


    TIM_OC1Init(
        TIM3,
        &oc
    );


    TIM_OC1PreloadConfig(
        TIM3,
        TIM_OCPreload_Enable
    );


    TIM_ARRPreloadConfig(
        TIM3,
        ENABLE
    );


    /*
     * Start disabled
     */

    TIM_Cmd(
        TIM3,
        DISABLE
    );
}



void test_signal_set_rate(uint32_t hz)
{
    if(hz == 0)
        return;


    test_rate = hz;


    TIM3->ARR =
        (72000000 / hz) - 1;


    TIM3->CCR1 =
        TIM3->ARR / 2;
}



void test_signal_enable(void)
{
    TIM_Cmd(
        TIM3,
        ENABLE
    );
}



void test_signal_disable(void)
{
    TIM_Cmd(
        TIM3,
        DISABLE
    );


    /*
     * force output low
     */

    GPIOA->BRR =
        GPIO_Pin_6;
}
