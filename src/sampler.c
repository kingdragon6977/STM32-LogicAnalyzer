#include "stm32f10x.h"
#include "stm32f10x_tim.h"

#include "sampler.h"
#include "gpio.h"


static uint8_t *sample_buffer;

static volatile uint32_t sample_index;

static volatile uint8_t complete = 0;

static uint32_t sample_length;


volatile uint32_t irq_count = 0;


void sampler_init(void)
{

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2,
        ENABLE
    );


    NVIC_InitTypeDef nvic;


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


    TIM_ITConfig(
        TIM2,
        TIM_IT_Update,
        ENABLE
    );


    nvic.NVIC_IRQChannel =
        TIM2_IRQn;

    nvic.NVIC_IRQChannelPreemptionPriority =
        0;

    nvic.NVIC_IRQChannelSubPriority =
        0;

    nvic.NVIC_IRQChannelCmd =
        ENABLE;


    NVIC_Init(&nvic);



    TIM_Cmd(
        TIM2,
        DISABLE
    );
}




void sampler_start(uint8_t *buf, uint32_t count)
{
    sample_buffer = buf;

    sample_length = count;

    sample_index = 0;

    complete = 0;
	
	irq_count = 0;


    TIM_SetCounter(
        TIM2,
        0
    );


    TIM_Cmd(
        TIM2,
        ENABLE
    );
}




uint8_t sampler_done(void)
{
    return complete;
}

uint8_t *sampler_get_buffer(void)
{
    return sample_buffer;
}

void sampler_set_rate(uint32_t hz)
{
    uint32_t period;

    period=(72000000/hz)-1;

    TIM_SetAutoreload(
        TIM2,
        period
    );
}

void TIM2_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

        irq_count++;

        if(sample_index >= sample_length)
        {
            TIM_Cmd(TIM2, DISABLE);
            complete = 1;
            return;
        }

        sample_buffer[sample_index++] = logic_read();
    }
}
