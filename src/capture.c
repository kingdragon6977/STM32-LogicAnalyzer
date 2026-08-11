#include "stm32f10x.h"
#include "sampler.h"
#include "capture.h"
#include "gpio.h"
#include "uart.h"
#include "cli.h"
#include "dma_capture.h"

extern volatile uint32_t irq_count;

static uint8_t buffer[CAPTURE_SAMPLES];
static uint8_t trigger_channel = 0;
static uint8_t trigger_rising = 1;
static analyzer_mode_t mode = MODE_EDGE;
static uint32_t sample_rate = 1000000;
static uint32_t edge_count[4];
static volatile uint8_t backend_use_dma = 1;

static void normalize_samples(void)
{
    uint32_t i;
    for(i = 0; i < CAPTURE_SAMPLES; i++)
        buffer[i] &= 0x0F;
}

void capture_init(void)
{
    capture_set_trigger(0,1);
    dma_capture_init();
}

void capture_set_mode(analyzer_mode_t new_mode) { mode = new_mode; }
analyzer_mode_t capture_get_mode(void) { return mode; }

void capture_set_rate(uint32_t hz) { sample_rate = hz; }
uint32_t capture_get_rate(void) { return sample_rate; }

void capture_set_rate_enum(capture_rate_t rate)
{
    switch(rate)
    {
        case RATE_1K:    capture_set_rate(1000); break;
        case RATE_100K:  capture_set_rate(100000); break;
        case RATE_500K:  capture_set_rate(500000); break;
        case RATE_1M:    capture_set_rate(1000000); break;
        case RATE_2M:    capture_set_rate(2000000); break;
    }
    uart_print("Sample rate set.\r\n");
}

void capture_set_trigger(uint8_t channel,uint8_t rising)
{
    trigger_channel = channel & 3;
    trigger_rising = rising ? 1 : 0;
}

void capture_set_backend_dma(uint8_t enable) { backend_use_dma = enable ? 1 : 0; }
uint8_t capture_get_backend_dma(void) { return backend_use_dma; }

static uint8_t wait_for_trigger(void)
{
    uint8_t mask = 1 << trigger_channel;
    uint8_t last = logic_read();
    uint32_t timeout = 10000000;

    uart_print("Waiting for trigger...\r\n");
    while(timeout--)
    {
        uint8_t now = logic_read();
        if(trigger_rising)
        {
            if(!(last & mask) && (now & mask))
            {
                uart_print("Trigger detected\r\n");
                return 1;
            }
        }
        else
        {
            if((last & mask) && !(now & mask))
            {
                uart_print("Trigger detected\r\n");
                return 1;
            }
        }
        last = now;
    }
    uart_print("Trigger timeout\r\n");
    return 0;
}

static void decode_edges(void)
{
    uint32_t i;
    uint8_t last = buffer[0];

    uart_print("\r\nEdges Found:\r\n");
    for(i=1;i<CAPTURE_SAMPLES;i++)
    {
        uint8_t diff = last ^ buffer[i];
        if(diff & 0x01) edge_count[0]++;
        if(diff & 0x02) edge_count[1]++;
        if(diff & 0x04) edge_count[2]++;
        if(diff & 0x08) edge_count[3]++;
        if(diff)
        {
            uart_print("S="); uart_print_uint(i); uart_print(" [");
            uart_putc((buffer[i]&8)?'1':'0');
            uart_putc((buffer[i]&4)?'1':'0');
            uart_putc((buffer[i]&2)?'1':'0');
            uart_putc((buffer[i]&1)?'1':'0');
            uart_print("] ");
            if(diff&1) uart_print((buffer[i]&1)?"CH0↑ ":"CH0↓ ");
            if(diff&2) uart_print((buffer[i]&2)?"CH1↑ ":"CH1↓ ");
            if(diff&4) uart_print((buffer[i]&4)?"CH2↑ ":"CH2↓ ");
            if(diff&8) uart_print((buffer[i]&8)?"CH3↑ ":"CH3↓ ");
            uart_print("\r\n");
        }
        last = buffer[i];
    }

    uart_print("\r\nSummary\r\n");
    uart_print("CH0 edges: "); uart_print_uint(edge_count[0]); uart_print("\r\n");
    uart_print("CH1 edges: "); uart_print_uint(edge_count[1]); uart_print("\r\n");
    uart_print("CH2 edges: "); uart_print_uint(edge_count[2]); uart_print("\r\n");
    uart_print("CH3 edges: "); uart_print_uint(edge_count[3]); uart_print("\r\n");
}

/*
 * Passive I2C decoder.
 *
 * Analyzer wiring:
 *   CH0 = SDA
 *   CH1 = SCL
 *
 * The decoder never drives either bus line. It reconstructs I2C from the
 * captured samples by looking at START/STOP conditions and SCL rising edges.
 * SDA is sampled on each SCL rising edge, as required by I2C.
 *
 * Output example:
 *   START @ 123
 *   ADDR 0x50 W ACK
 *   DATA 0x12 ACK
 *   DATA 0x34 NACK
 *   STOP @ 456
 *
 * This is intentionally a passive decoder for observing the SA-SD35 bus.
 */
static void decode_i2c(void)
{
    uint32_t i;
    uint32_t starts = 0;
    uint32_t stops = 0;
    uint32_t bytes = 0;
    uint32_t acks = 0;
    uint32_t nacks = 0;
    uint8_t in_frame = 0;
    uint8_t bit_count = 0;
    uint8_t shift = 0;
    uint8_t first_byte = 1;
    uint8_t byte_value = 0;
    uint8_t prev = buffer[0];

    uart_print("\r\nI2C PASSIVE DECODE\r\n");
    uart_print("CH0=SDA CH1=SCL\r\n");

    for(i = 1; i < CAPTURE_SAMPLES; i++)
    {
        uint8_t curr = buffer[i];
        uint8_t prev_scl = (prev >> 1) & 1;
        uint8_t curr_scl = (curr >> 1) & 1;
        uint8_t prev_sda = prev & 1;
        uint8_t curr_sda = curr & 1;

        /* START: SDA falls while SCL is high. */
        if(prev_sda && !curr_sda && prev_scl && curr_scl)
        {
            starts++;
            in_frame = 1;
            bit_count = 0;
            shift = 0;
            first_byte = 1;

            uart_print("START @ ");
            uart_print_uint(i);
            uart_print("\r\n");
        }

        /* STOP: SDA rises while SCL is high. */
        if(!prev_sda && curr_sda && prev_scl && curr_scl)
        {
            if(in_frame)
            {
                stops++;
                uart_print("STOP @ ");
                uart_print_uint(i);
                uart_print("\r\n");
            }

            in_frame = 0;
            bit_count = 0;
            shift = 0;
            first_byte = 1;
        }

        /* I2C data is valid at the rising edge of SCL. */
        if(!prev_scl && curr_scl && in_frame)
        {
            uint8_t sda = curr_sda;

            if(bit_count < 8)
            {
                shift = (uint8_t)((shift << 1) | sda);
                bit_count++;
            }
            else
            {
                /* Ninth clock is ACK/NACK. ACK is SDA low. */
                byte_value = shift;

                if(first_byte)
                {
                    uint8_t address = (uint8_t)(byte_value >> 1);
                    uint8_t read = byte_value & 1;

                    uart_print("ADDR 0x");
                    uart_print_hex8(address);
                    uart_print(read ? " R " : " W ");
                }
                else
                {
                    uart_print("DATA 0x");
                    uart_print_hex8(byte_value);
                    uart_print(" ");
                }

                if(!sda)
                {
                    acks++;
                    uart_print("ACK");
                }
                else
                {
                    nacks++;
                    uart_print("NACK");
                }

                uart_print("\r\n");
                bytes++;
                first_byte = 0;
                bit_count = 0;
                shift = 0;
            }
        }

        prev = curr;
    }

    uart_print("\r\nI2C SUMMARY\r\n");
    uart_print("STARTs : "); uart_print_uint(starts); uart_print("\r\n");
    uart_print("STOPs  : "); uart_print_uint(stops); uart_print("\r\n");
    uart_print("Bytes  : "); uart_print_uint(bytes); uart_print("\r\n");
    uart_print("ACKs   : "); uart_print_uint(acks); uart_print("\r\n");
    uart_print("NACKs  : "); uart_print_uint(nacks); uart_print("\r\n");
}

static void raw_stats(void)
{
    uint32_t i;
    uint32_t edges = 0;
    uint8_t last = buffer[0];
    for(i=1;i<CAPTURE_SAMPLES;i++)
    {
        if(buffer[i] != last)
        {
            edges++;
            last = buffer[i];
        }
    }
    uart_print("Transitions: "); uart_print_uint(edges); uart_print("\r\n");
}

/* Print a compact transition diagnostic instead of dumping all 8192 samples. */
static void raw_transition_report(void)
{
    uint32_t i;
    uint32_t transitions = 0;
    uint32_t first = 0;
    uint32_t min_gap = 0xFFFFFFFF;
    uint32_t max_gap = 0;
    uint32_t last_transition = 0;
    uint8_t last = buffer[0];

    uart_print("\r\nTransition report\r\n");
    uart_print("First transitions (max 32):\r\n");

    for(i=1; i<CAPTURE_SAMPLES; i++)
    {
        if(buffer[i] != last)
        {
            uint32_t gap = i - last_transition;
            transitions++;

            if(transitions == 1)
            {
                first = i;
                last_transition = i;
            }
            else
            {
                if(gap < min_gap) min_gap = gap;
                if(gap > max_gap) max_gap = gap;
                last_transition = i;
            }

            if(transitions <= 32)
            {
                uart_print("S=");
                uart_print_uint(i);
                uart_print(" ");
                uart_print_hex8(last);
                uart_print(" -> ");
                uart_print_hex8(buffer[i]);
                uart_print(" ");

                if((last & 1) == 0 && (buffer[i] & 1)) uart_print("CH0↑");
                else if((last & 1) && !(buffer[i] & 1)) uart_print("CH0↓");
                else if((last & 2) == 0 && (buffer[i] & 2)) uart_print("CH1↑");
                else if((last & 2) && !(buffer[i] & 2)) uart_print("CH1↓");
                else if((last & 4) == 0 && (buffer[i] & 4)) uart_print("CH2↑");
                else if((last & 4) && !(buffer[i] & 4)) uart_print("CH2↓");
                else if((last & 8) == 0 && (buffer[i] & 8)) uart_print("CH3↑");
                else if((last & 8) && !(buffer[i] & 8)) uart_print("CH3↓");
                else uart_print("multiple");

                uart_print("\r\n");
            }
            last = buffer[i];
        }
    }

    uart_print("\r\nTransition summary\r\n");
    uart_print("Total : "); uart_print_uint(transitions); uart_print("\r\n");
    uart_print("First : "); uart_print_uint(first); uart_print(" samples\r\n");

    if(transitions > 2)
    {
        uart_print("Min gap: "); uart_print_uint(min_gap); uart_print(" samples\r\n");
        uart_print("Max gap: "); uart_print_uint(max_gap); uart_print(" samples\r\n");
    }
}

void capture_raw(void)
{
    uart_print("\r\nRAW_CAPTURE\r\n");
    uart_print("Samples : "); uart_print_uint(CAPTURE_SAMPLES); uart_print("\r\n");
    uart_print("Rate : "); uart_print_uint(sample_rate); uart_print(" Hz\r\n");
    uart_print("Capture starts immediately\r\n");

    if(backend_use_dma)
    {
        uart_print("Hardware DMA sampler running...\r\n");
        sampler_set_rate(sample_rate);
        dma_capture_start(buffer, CAPTURE_SAMPLES);
        while(!dma_capture_done()) { }
        uart_print("DMA sampling complete\r\n");
    }
    else
    {
        uart_print("Hardware IRQ sampler running...\r\n");
        sampler_set_rate(sample_rate);
        sampler_start(buffer, CAPTURE_SAMPLES);
        while(!sampler_done()) { }
        uart_print("IRQ sampling complete\r\n");
    }

    normalize_samples();
    raw_stats();
    raw_transition_report();

    uart_print("\r\nFirst 128 samples:\r\n");
    {
        uint32_t i;
        for(i=0; i<128 && i<CAPTURE_SAMPLES; i++)
        {
            uart_print_hex8(buffer[i]);
            uart_putc(((i & 0x0F) == 0x0F) ? '\n' : ' ');
        }
    }

    uart_print("\r\nRAW_DONE\r\n");
}

void capture_run(void)
{
    uint32_t i;
    for(i=0;i<4;i++) edge_count[i]=0;

    uart_print("\r\n====================================\r\n");
    uart_print("STM32 Logic Analyzer v1.0\r\n");
    uart_print("====================================\r\n");
    uart_print("Samples : "); uart_print_uint(CAPTURE_SAMPLES); uart_print("\r\n");
    uart_print("Rate : "); uart_print_uint(sample_rate); uart_print(" Hz\r\n");
    uart_print("Analyzer Armed\r\n");

    if(!wait_for_trigger()) return;
    uart_print("Capture Started\r\n");

    if(backend_use_dma)
    {
        uart_print("Hardware DMA sampler running...\r\n");
        sampler_set_rate(sample_rate);
        dma_capture_start(buffer, CAPTURE_SAMPLES);
        while(!dma_capture_done()) cli_task();
        uart_print("DMA sampling complete\r\n");
    }
    else
    {
        uart_print("Hardware IRQ sampler running...\r\n");
        sampler_set_rate(sample_rate);
        sampler_start(buffer, CAPTURE_SAMPLES);
        while(!sampler_done()) cli_task();
        uart_print("IRQ sampling complete\r\n");
    }

    normalize_samples();
    if(mode==MODE_EDGE) decode_edges();
    if(mode==MODE_I2C) decode_i2c();
    raw_stats();
    uart_print("\r\nDONE\r\n");
}
