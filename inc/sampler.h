#ifndef SAMPLER_H
#define SAMPLER_H

#include <stdint.h>

#define CAPTURE_SAMPLES 8192

void sampler_init(void);

void sampler_start(
    uint8_t *buf,
    uint32_t count
);

void sampler_start_triggered(
    uint8_t *buf,
    uint32_t count,
    uint8_t channel,
    uint8_t rising
);

uint8_t sampler_done(void);

void sampler_set_rate(uint32_t hz);

uint8_t *sampler_get_buffer(void);

#endif
