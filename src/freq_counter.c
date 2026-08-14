#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "freq_counter.h"
#include "sampler.h"
#include "uart.h"
#include "gpio.h"

/*
 * Frequency counter:
 *
 * TIM2 ETR is PA0/CH0 on STM32F103 with no TIM2 remap. PA0 is kept as a
 * normal floating input; the timer receives ETR from the pin in this mode.
 *
 * TIM2 counts external ETR edges. Its 16-bit counter is prescaled internally
 * by 1024 so an 8 MHz signal gives about 781 counts in 100 ms and a 36 MHz
 * signal about 3516 counts. Both are comfortably below 65535.
 */
#define FREQ_COUNTER_PRESCALER 1023u
#define FREQ_GATE_MS            100u

static uint8_t initialized = 0;

static uint32_t timer_clock_hz(void)
{
    uint32_t ppre = (RCC->CFGR >> 8) & 0x7u;
    uint32_t pclk1;

    switch (ppre)
    {
        case 4u: pclk1 = SystemCoreClock / 2u;  break;
        case 5u: pclk1 = SystemCoreClock / 4u;  break;
        case 6u: pclk1 = SystemCoreClock / 8u;  break;
        case 7u: pclk1 = SystemCoreClock / 16u; break;
        default: pclk1 = SystemCoreClock;      break;
    }

    /* STM32F1 timers run at 2 x PCLK when APB1 is prescaled. */
    return (ppre >= 4u) ? (pclk1 * 2u) : pclk1;
}

static void pa0_prepare_for_etr(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO,
                           ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* TIM2 remap = 00: CH1/ETR = PA0. */
    AFIO->MAPR &= ~(0x3u << 8);
}

void freq_counter_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2 | RCC_APB1Periph_TIM4,
                           ENABLE);
    initialized = 1;
}

void freq_counter_measure(void)
{
    TIM_TimeBaseInitTypeDef tim;
    uint32_t gate_timer_hz;
    uint32_t raw_counts;
    uint32_t frequency_hz;
    uint32_t gate_ticks;

    if (!initialized)
        freq_counter_init();

    uart_print("\r\nFREQUENCY COUNTER\r\n");
    uart_print("Input: CH0 / PA0 (TIM2 ETR)\r\n");
    uart_print("Gate: 100 ms\r\n");
    uart_print("TIM2 ETR external-clock mode, input /1024\r\n");

    /* Explicitly restore PA0 as the timer input in case another test changed
       the GPIO configuration. */
    pa0_prepare_for_etr();

    /* Stop normal sampling and its TIM2 update interrupt. */
    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    /* Reinitialize TIM2 so no sampler state remains in its clocking setup. */
    TIM_DeInit(TIM2);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    tim.TIM_Prescaler = FREQ_COUNTER_PRESCALER;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 0xFFFFu;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &tim);

    /* ETR, non-inverted, no ETR prescaler, no digital filter. */
    TIM_ETRClockMode2Config(TIM2,
                            TIM_ExtTRGPSC_OFF,
                            TIM_ExtTRGPolarity_NonInverted,
                            0);
    TIM_SetCounter(TIM2, 0);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);

    /* TIM4 supplies an independent 100 ms gate from the analyzer clock. */
    gate_timer_hz = timer_clock_hz();
    gate_ticks = gate_timer_hz / 10000u;
    if (gate_ticks == 0u)
        gate_ticks = 1u;

    tim.TIM_Prescaler = gate_ticks - 1u;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 999u;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM4, &tim);
    TIM_SetCounter(TIM4, 0);
    TIM_ClearFlag(TIM4, TIM_FLAG_Update);

    TIM_Cmd(TIM2, ENABLE);
    TIM_Cmd(TIM4, ENABLE);

    while ((TIM4->SR & TIM_SR_UIF) == 0u)
        ;

    TIM_Cmd(TIM4, DISABLE);
    TIM_Cmd(TIM2, DISABLE);
    TIM4->SR &= ~TIM_SR_UIF;

    raw_counts = TIM_GetCounter(TIM2);

    /* 100 ms gate and /1024 input prescaler => count * 10240 Hz. */
    frequency_hz = raw_counts * (FREQ_COUNTER_PRESCALER + 1u) *
                   (1000u / FREQ_GATE_MS);

    uart_print("Raw timer counts: ");
    uart_print_uint(raw_counts);
    uart_print("\r\n");
    uart_print("Measured frequency: ");
    uart_print_uint(frequency_hz);
    uart_print(" Hz\r\n");

    if (frequency_hz >= 7500000u && frequency_hz <= 8500000u)
        uart_print("Range: ~8 MHz HSE\r\n");
    else if (frequency_hz >= 34000000u && frequency_hz <= 38000000u)
        uart_print("Range: ~36 MHz PLL/2\r\n");
    else if (frequency_hz == 0u)
        uart_print("No measurable clock detected.\r\n");
    else
        uart_print("Clock detected; compare against expected MCO frequency.\r\n");

    /* Restore the normal sampler's TIM2 configuration. */
    sampler_init();
}
