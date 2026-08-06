#include "stm32f10x.h"
#include "stm32f10x_tim.h"

#include "sampler.h"
#include "gpio.h"
#include "uart.h"


static uint8_t *sample_buffer;
static volatile uint32_t sample_index;
static volatile uint8_t complete = 0;
static uint32_t sample_length;

static uint8_t trigger_enabled = 0;
static uint8_t trigger_channel = 0;
static uint8_t trigger_rising = 1;
static uint8_t trigger_seen = 0;
static uint8_t last_sample = 0;
static uint32_t post_trigger_count = 0;

volatile uint32_t irq_count = 0;


void sampler_init(void)
{
    uart_print("sampler_init: start\r\n");

    RCC_APB1PeriphClockCmd(
        RCC_APB1Periph_TIM2,
        ENABLE
    );

    uart_print("sampler_init: clock enabled\r\n");

    NVIC_InitTypeDef nvic;
    TIM_TimeBaseInitTypeDef tim;

    tim.TIM_Prescaler = 0;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 71;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;

    TIM_TimeBaseInit(TIM2, &tim);

    uart_print("sampler_init: tim base init done\r\n");

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    uart_print("sampler_init: tim it config done\r\n");

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);

    uart_print("sampler_init: NVIC init done\r\n");

    TIM_Cmd(TIM2, DISABLE);

    uart_print("sampler_init: end\r\n");
}


void sampler_start(uint8_t *buf, uint32_t count)
{
    sample_buffer = buf;
    sample_length = count;
    sample_index = 0;
    complete = 0;
    trigger_enabled = 0;
    irq_count = 0;

    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);
}


void sampler_start_triggered(uint8_t *buf, uint32_t count, uint8_t channel, uint8_t rising)
{
    sample_buffer = buf;
    sample_length = count;
    sample_index = 0;
    complete = 0;
    irq_count = 0;

    trigger_channel = channel & 3;
    trigger_rising = rising ? 1 : 0;
    trigger_enabled = 1;
    trigger_seen = 0;
    post_trigger_count = 0;
    last_sample = logic_read();

    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);
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

    if(hz == 0)
        return;

    period = (72000000 / hz) - 1;

    TIM_SetAutoreload(TIM2, period);
}


void TIM2_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        uint8_t sample;

        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        irq_count++;

        sample = logic_read();

        sample_buffer[sample_index++] = sample;

        if(trigger_enabled && !trigger_seen)
        {
            uint8_t mask = 1 << trigger_channel;

            if(trigger_rising)
            {
                if(!(last_sample & mask) && (sample & mask))
                    trigger_seen = 1;
            }
            else
            {
                if((last_sample & mask) && !(sample & mask))
                    trigger_seen = 1;
            }
        }

        if(trigger_enabled && trigger_seen)
            post_trigger_count++;

        last_sample = sample;

        if((!trigger_enabled && sample_index >= sample_length) ||
           (trigger_enabled && trigger_seen && post_trigger_count >= sample_length/2))
        {
            TIM_Cmd(TIM2, DISABLE);
            complete = 1;
        }

        if(sample_index >= sample_length)
        {
            TIM_Cmd(TIM2, DISABLE);
            complete = 1;
        }
    }
}
