#include "stm32f10x.h"

#include "timer.h"

#include "stm32f10x_tim.h"

static uint32_t timer_rate = 1000000;



void timer_init(void)
{
    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2,
        ENABLE
    );


    TIM_TimeBaseInitTypeDef tim;


    tim.TIM_Prescaler = 0;

    tim.TIM_CounterMode =
        TIM_CounterMode_Up;


    tim.TIM_Period = 71;


    tim.TIM_ClockDivision =
        TIM_CKD_DIV1;


    TIM_TimeBaseInit(
        TIM2,
        &tim
    );


    TIM_Cmd(
        TIM2,
        DISABLE
    );
}




void timer_set_rate(uint32_t hz)
{
    timer_rate = hz;


    /*
     * STM32F103 clock:
     *
     * APB1 timer clock = 72MHz
     *
     * period =
     * 72000000 / hz - 1
     */


    uint32_t period;


    period =
        (72000000 / hz) - 1;



    TIM_SetAutoreload(
        TIM2,
        period
    );
}




void timer_start(void)
{
    TIM_SetCounter(
        TIM2,
        0
    );


    TIM_Cmd(
        TIM2,
        ENABLE
    );
}




void timer_stop(void)
{
    TIM_Cmd(
        TIM2,
        DISABLE
    );
}





void timer_wait_tick(void)
{
    while(
        !(TIM2->SR & TIM_SR_UIF)
    );


    TIM2->SR &= ~TIM_SR_UIF;
}
