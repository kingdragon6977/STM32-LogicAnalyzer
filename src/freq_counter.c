#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "freq_counter.h"
#include "sampler.h"
#include "uart.h"

/*
 * TIM2 external clock mode 2 uses PA0/ETR. A /1024 timer prescaler keeps
 * both an 8 MHz HSE signal and a 36 MHz PLL/2 MCO signal inside the 16-bit
 * TIM2 counter during the 100 ms measurement window.
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
    uart_print("Disconnect normal CH0 signal during this test.\r\n");

    /* Stop the normal sample timer and remove its update interrupt. */
    TIM_Cmd(TIM2, DISABLE);
    TIM_ITConfig(TIM2, TIM_IT_Update, DISABLE);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);

    /* TIM2: external clock mode 2, rising edges on PA0/ETR, /1024 PSC. */
    tim.TIM_Prescaler = FREQ_COUNTER_PRESCALER;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 0xFFFFu;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_ETRClockMode2Config(TIM2,
                            TIM_ExtTRGPSC_OFF,
                            TIM_ExtTRGPolarity_NonInverted,
                            0);
    TIM_SetCounter(TIM2, 0);

    /* TIM4 supplies a 100 ms measurement gate from the analyzer clock. */
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
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);

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
