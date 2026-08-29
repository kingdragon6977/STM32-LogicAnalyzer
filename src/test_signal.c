#include "stm32f10x.h"
#include "stm32f10x_tim.h"

#include "board.h"
#include "test_signal.h"

static uint32_t test_rate = 10000;

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

void test_signal_init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    test_signal_pin_pwm();

    tim.TIM_Prescaler = 0;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    tim.TIM_Period = (72000000 / test_rate) - 1;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TEST_SIGNAL_TIMER, &tim);

    oc.TIM_OCMode = TIM_OCMode_PWM1;
    oc.TIM_OutputState = TIM_OutputState_Enable;
    oc.TIM_Pulse = tim.TIM_Period / 2;
    oc.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TEST_SIGNAL_TIMER, &oc);
    TIM_OC1PreloadConfig(TEST_SIGNAL_TIMER, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TEST_SIGNAL_TIMER, ENABLE);

    TIM_Cmd(TEST_SIGNAL_TIMER, DISABLE);
    test_signal_pin_low();
}

void test_signal_set_rate(uint32_t hz)
{
    if(hz == 0)
        return;

    test_rate = hz;
    TEST_SIGNAL_TIMER->ARR = (72000000 / hz) - 1;
    TEST_SIGNAL_TIMER->CCR1 = TEST_SIGNAL_TIMER->ARR / 2;
}

void test_signal_enable(void)
{
    /* Restore TIM3_CH1 ownership of PA6 before starting the PWM. */
    test_signal_pin_pwm();
    TIM_SetCounter(TEST_SIGNAL_TIMER, 0);
    TIM_Cmd(TEST_SIGNAL_TIMER, ENABLE);
}

void test_signal_disable(void)
{
    TIM_Cmd(TEST_SIGNAL_TIMER, DISABLE);
    test_signal_pin_low();
}
