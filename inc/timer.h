#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>


void timer_init(void);
void timer_set_rate(uint32_t hz);

/*
 * Set sampling frequency
 */
void timer_set_rate(uint32_t hz);



/*
 * Start / stop sampling clock
 */
void timer_start(void);

void timer_stop(void);



/*
 * Wait for one sample period
 * (temporary software sampler)
 */
void timer_wait_tick(void);


#endif
