#ifndef TEST_SIGNAL_H
#define TEST_SIGNAL_H

#include <stdint.h>

void test_signal_init(void);
void test_signal_enable(void);
void test_signal_disable(void);
void test_signal_set_rate(uint32_t hz);

#endif
