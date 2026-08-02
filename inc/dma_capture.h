#ifndef DMA_CAPTURE_H
#define DMA_CAPTURE_H

#include <stdint.h>


void dma_capture_init(void);


void dma_capture_start(
    uint8_t *buffer,
    uint32_t length
);


uint8_t dma_capture_done(void);


#endif
