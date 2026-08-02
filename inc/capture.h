#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdint.h>


typedef enum
{
    MODE_EDGE,
    MODE_I2C

} analyzer_mode_t;



typedef enum
{
    RATE_1K,
    RATE_100K,
    RATE_500K,
    RATE_1M,
    RATE_2M
} capture_rate_t;



void capture_init(void);


void capture_run(void);


void capture_raw(void);



void capture_set_mode(
    analyzer_mode_t mode
);


analyzer_mode_t capture_get_mode(void);



void capture_set_rate(
    uint32_t hz
);


uint32_t capture_get_rate(void);



void capture_set_rate_enum(
    capture_rate_t rate
);



void capture_set_trigger(
    uint8_t channel,
    uint8_t rising
);



#endif
