#include "stm32f10x.h"
#include "stm32f10x_tim.h"

#include "board.h"
#include "test_signal.h"

static uint32_t test_rate = 1000;

static void test_signal_pin_pwm(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = TEST_SIGNAL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TEST_SIGNAL_PORT, &gpio);
}

static void test_signal_pin_low(void)
{
    GPIO_InitTypeDef gpio;

    /* Drive the test output to a defined 0 V state while PWM is disabled. */
    GPIO_ResetBits(TEST_SIGNAL_PORT, TEST_SIGNAL_PIN);

    gpio.GPIO_Pin = TEST_SIGNAL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(TEST_SIGNAL_PORT, &gpio);

    GPIO_ResetBits(TEST_SIGNAL_PORT, TEST_SIGNAL_PIN);
}

static uint32_t test_signal_timer_clock(void)
{
    RCC_ClocksTypeDef clocks;
    uint32_t timer_clock;

    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;

    /*
     * STM32F1 APB timer rule: when APB1 is prescaled by anything other
     * than /1, TIM2..TIM7 receive 2 x PCLK1.
     */
    if((RCC->CFGR & RCC_CFGR_PPRE1) != 0u)
        timer_clock *= 2u;

    return timer_clock;
}

static void test_signal_program_rate(uint32_t hz)
{
    uint32_t timer_clock;
    uint32_t prescaler_div;
    uint32_t timer_tick;
    uint32_t period_counts;

    if(hz == 0u)
        return;

    timer_clock = test_signal_timer_clock();

    /*
     * TIM3 is 16-bit. Choose the smallest prescaler that keeps ARR within
     * 0..65535, then derive the closest integer period from that timer tick.
     */
    prescaler_div = (timer_clock + ((hz * 65536u) - 1u)) / (hz * 65536u);
    if(prescaler_div == 0u)
        prescaler_div = 1u;
    if(prescaler_div > 65536u)
        prescaler_div = 65536u;

    timer_tick = timer_clock / prescaler_div;
    period_counts = (timer_tick + (hz / 2u)) / hz;

    if(period_counts < 2u)
        period_counts = 2u;
    if(period_counts > 65536u)
        period_counts = 65536u;

    TEST_SIGNAL_TIMER->PSC = prescaler_div - 1u;
    TEST_SIGNAL_TIMER->ARR = period_counts - 1u;
    TEST_SIGNAL_TIMER->CCR1 = period_counts / 2u;

    /* Load PSC/ARR immediately and restart the PWM phase from a known point. */
    TIM_GenerateEvent(TEST_SIGNAL_TIMER, TIM_EventSource_Update);
    TIM_SetCounter(TEST_SIGNAL_TIMER, 0u);

    test_rate = hz;
}

void test_signal_init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    test_signal_pin_pwm();

    /* Temporary safe values; test_signal_program_rate() installs final rate. */
    tim.TIM_Prescaler = 0u;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = 999u;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TEST_SIGNAL_TIMER, &tim);

    oc.TIM_OCMode = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse = 500u;
    oc.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TEST_SIGNAL_TIMER, &oc);
    TIM_OC1PreloadConfig(TEST_SIGNAL_TIMER, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TEST_SIGNAL_TIMER, ENABLE);

    test_signal_program_rate(test_rate);

    TIM_Cmd(TEST_SIGNAL_TIMER, DISABLE);
    test_signal_pin_low();
}

void test_signal_set_rate(uint32_t hz)
{
    test_signal_program_rate(hz);
}

void test_signal_enable(void)
{
    /* Restore TIM3_CH1 ownership of PA6 before starting the PWM. */
    test_signal_pin_pwm();
    TIM_SetCounter(TEST_SIGNAL_TIMER, 0u);
    TIM_Cmd(TEST_SIGNAL_TIMER, ENABLE);
}

void test_signal_disable(void)
{
    TIM_Cmd(TEST_SIGNAL_TIMER, DISABLE);
    test_signal_pin_low();
}
