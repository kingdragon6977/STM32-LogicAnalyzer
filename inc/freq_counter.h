#ifndef FREQ_COUNTER_H
#define FREQ_COUNTER_H

#include <stdint.h>

/*
 * Frequency counter input: CH0 / PA0 (TIM2_CH1/ETR).
 * The counter temporarily takes PA0/TIM2 away from normal sampling while
 * a measurement is in progress, then restores the normal sampler setup.
 */
void freq_counter_init(void);
void freq_counter_measure(void);

#endif
