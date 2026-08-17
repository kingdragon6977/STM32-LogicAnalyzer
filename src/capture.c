#include "stm32f10x.h"
#include "sampler.h"
#include "capture.h"
#include "gpio.h"
#include "uart.h"
#include "dma_capture.h"

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
    for(i = 0; i < CAPTURE_SAMPLES; ++i)
        buffer[i] &= 0x0F;
}

void capture_init(void)
{
    capture_set_trigger(0, 1);
    dma_capture_init();
}

void capture_set_mode(analyzer_mode_t new_mode) { mode = new_mode; }
analyzer_mode_t capture_get_mode(void) { return mode; }
void capture_set_rate(uint32_t hz) { if(hz) sample_rate = hz; }
uint32_t capture_get_rate(void) { return sample_rate; }

void capture_set_rate_enum(capture_rate_t rate)
{
    switch(rate)
    {
        case RATE_1K:  capture_set_rate(1000); break;
        case RATE_100K: capture_set_rate(100000); break;
        case RATE_500K: capture_set_rate(500000); break;
        case RATE_1M:  capture_set_rate(1000000); break;
        case RATE_2M:  capture_set_rate(2000000); break;
        case RATE_4M:  capture_set_rate(4000000); break;
        default: break;
    }
    uart_print("Sample rate: ");
    uart_print_uint(sample_rate);
    uart_print(" Hz\r\n");
}

void capture_set_trigger(uint8_t channel, uint8_t rising)
{
    trigger_channel = channel & 3;
    trigger_rising = rising ? 1 : 0;
}

void capture_set_backend_dma(uint8_t enable) { backend_use_dma = enable ? 1 : 0; }
uint8_t capture_get_backend_dma(void) { return backend_use_dma; }

static uint8_t wait_for_trigger(void)
{
    uint8_t last = logic_read();
    uint32_t timeout = 10000000u;

    if(mode == MODE_I2C)
    {
        uart_print("Waiting for I2C START: SDA(CH0) falling while SCL(CH1) HIGH...\r\n");
        gpio_i2c_trigger_arm();
        while(!gpio_i2c_trigger_seen())
        {
            /* EXTI0 catches the START edge; this loop does not poll SDA. */
        }
        uart_print("I2C START detected\r\n");
        return 1;
    }

    uart_print("Waiting for trigger...\r\n");
    while(timeout--)
    {
        uint8_t now = logic_read();
        uint8_t mask = (uint8_t)(1u << trigger_channel);

        if(trigger_rising)
        {
            if(!(last & mask) && (now & mask))
                return 1;
        }
        else
        {
            if((last & mask) && !(now & mask))
                return 1;
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
    for(i = 1; i < CAPTURE_SAMPLES; ++i)
    {
        uint8_t diff = last ^ buffer[i];
        if(diff & 0x01) ++edge_count[0];
        if(diff & 0x02) ++edge_count[1];
        if(diff & 0x04) ++edge_count[2];
        if(diff & 0x08) ++edge_count[3];

        if(diff)
        {
            uart_print("S="); uart_print_uint(i); uart_print(" [");
            uart_putc((buffer[i] & 8) ? '1' : '0');
            uart_putc((buffer[i] & 4) ? '1' : '0');
            uart_putc((buffer[i] & 2) ? '1' : '0');
            uart_putc((buffer[i] & 1) ? '1' : '0');
            uart_print("] ");
            if(diff & 1) uart_print((buffer[i] & 1) ? "CH0^ " : "CH0v ");
            if(diff & 2) uart_print((buffer[i] & 2) ? "CH1^ " : "CH1v ");
            if(diff & 4) uart_print((buffer[i] & 4) ? "CH2^ " : "CH2v ");
            if(diff & 8) uart_print((buffer[i] & 8) ? "CH3^ " : "CH3v ");
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
 * CH0 = SDA, CH1 = SCL. CH2/CH3 are captured but ignored by the decoder.
 * The analyzer never drives either I2C line in this mode.
 *
 * I2C data is sampled on SCL rising edges. START/STOP are recognized from
 * SDA transitions while SCL is high. The first byte is printed as a 7-bit
 * address plus R/W; following bytes are printed as DATA. ACK/NACK is sampled
 * on the ninth clock.
 */
static void decode_i2c(void)
{
    uint32_t i;
    uint32_t starts = 0, stops = 0, bytes = 0, acks = 0, nacks = 0;
    uint8_t prev = buffer[0];
    uint8_t in_frame = 0;
    uint8_t bit_count = 0;
    uint8_t shift = 0;
    uint8_t first_byte = 1;
    uint32_t transaction = 0;

    uart_print("\r\nI2C PASSIVE SNOOP\r\n");
    uart_print("CH0=SDA PA0  CH1=SCL PA1  CH2/CH3=aux\r\n");
    uart_print("Capture rate: "); uart_print_uint(sample_rate); uart_print(" Hz\r\n");
    uart_print("No SDA/SCL drive is performed.\r\n\r\n");

    /* Capture is intentionally started just after the START trigger. */
    if(((prev & 0x03) == 0x02) || ((prev & 0x03) == 0x00))
    {
        in_frame = 1;
        ++starts;
        ++transaction;
        bit_count = 0;
        shift = 0;
        first_byte = 1;
        uart_print("START #"); uart_print_uint(transaction); uart_print(" @ trigger\r\n");
    }

    for(i = 1; i < CAPTURE_SAMPLES; ++i)
    {
        uint8_t curr = buffer[i];
        uint8_t prev_scl = (prev >> 1) & 1u;
        uint8_t curr_scl = (curr >> 1) & 1u;
        uint8_t prev_sda = prev & 1u;
        uint8_t curr_sda = curr & 1u;

        if(prev_sda && !curr_sda && curr_scl)
        {
            ++starts;
            ++transaction;
            in_frame = 1;
            bit_count = 0;
            shift = 0;
            first_byte = 1;
            uart_print("START #"); uart_print_uint(transaction);
            uart_print(" @ "); uart_print_uint(i); uart_print("\r\n");
        }

        if(!prev_sda && curr_sda && curr_scl)
        {
            if(in_frame)
            {
                ++stops;
                uart_print("STOP @ "); uart_print_uint(i); uart_print("\r\n");
            }
            in_frame = 0;
            bit_count = 0;
            shift = 0;
            first_byte = 1;
        }

        if(!prev_scl && curr_scl && in_frame)
        {
            if(bit_count < 8)
            {
                shift = (uint8_t)((shift << 1) | curr_sda);
                ++bit_count;
            }
            else
            {
                uint8_t byte_value = shift;
                uint8_t ack = curr_sda ? 0u : 1u;

                if(first_byte)
                {
                    uart_print("ADDR 0x");
                    uart_print_hex8((uint8_t)(byte_value >> 1));
                    uart_print((byte_value & 1u) ? " R " : " W ");
                }
                else
                {
                    uart_print("DATA 0x");
                    uart_print_hex8(byte_value);
                    uart_print(" ");
                }

                if(ack) { ++acks; uart_print("ACK"); }
                else    { ++nacks; uart_print("NACK"); }
                uart_print("\r\n");

                ++bytes;
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
    uint32_t i, edges = 0;
    uint8_t last = buffer[0];
    for(i = 1; i < CAPTURE_SAMPLES; ++i)
    {
        if(buffer[i] != last) { ++edges; last = buffer[i]; }
    }
    uart_print("Transitions: "); uart_print_uint(edges); uart_print("\r\n");
}

static void do_sample(void)
{
    sampler_set_rate(sample_rate);

    if(backend_use_dma)
    {
        uart_print("Hardware DMA sampler running...\r\n");
        dma_capture_start(buffer, CAPTURE_SAMPLES);
        while(!dma_capture_done()) { }
        uart_print("DMA sampling complete\r\n");
    }
    else
    {
        uart_print("Hardware IRQ sampler running...\r\n");
        sampler_start(buffer, CAPTURE_SAMPLES);
        while(!sampler_done()) { }
        uart_print("IRQ sampling complete\r\n");
    }
    normalize_samples();
}

void capture_raw(void)
{
    uart_print("\r\nRAW_CAPTURE\r\n");
    uart_print("Samples: "); uart_print_uint(CAPTURE_SAMPLES); uart_print("\r\n");
    uart_print("Rate: "); uart_print_uint(sample_rate); uart_print(" Hz\r\n");
    do_sample();
    if(mode == MODE_I2C) decode_i2c(); else raw_stats();
    uart_print("\r\nRAW_DONE\r\n");
}

void capture_run(void)
{
    uint32_t i;
    for(i = 0; i < 4; ++i) edge_count[i] = 0;

    uart_print("\r\n====================================\r\n");
    uart_print("STM32 Logic Analyzer v1.0\r\n");
    uart_print("====================================\r\n");
    uart_print("Samples: "); uart_print_uint(CAPTURE_SAMPLES); uart_print("\r\n");
    uart_print("Rate: "); uart_print_uint(sample_rate); uart_print(" Hz\r\n");

    if(!wait_for_trigger()) return;

    uart_print("Capture Started\r\n");
    do_sample();

    if(mode == MODE_EDGE)
        decode_edges();
    else
        decode_i2c();

    raw_stats();
    uart_print("\r\nDONE\r\n");
}
