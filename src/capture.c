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
static uint8_t boot_timing_profile = 0;

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

uint8_t capture_get_trigger_channel(void) { return trigger_channel; }
uint8_t capture_get_trigger_rising(void) { return trigger_rising; }

void capture_set_backend_dma(uint8_t enable) { backend_use_dma = enable ? 1 : 0; }
uint8_t capture_get_backend_dma(void) { return backend_use_dma; }

static uint8_t wait_for_trigger(void)
{
    uint8_t last = logic_read();
    uint32_t timeout = 10000000u;

    if(mode == MODE_I2C)
    {
        uart_print("Waiting for I2C START: SDA(CH0) falling while SCL(CH1) HIGH...\r\n");
        uart_print("Reset/wake the AV6301 now; timeout is 5 seconds.\r\n");
        gpio_i2c_trigger_arm();

        /*
         * The old implementation waited forever here.  That makes the CLI
         * appear dead whenever the bus is idle, the master has already
         * finished initialization, or EXTI0 does not see a START.  A finite
         * timeout keeps the console alive and makes the wiring/traffic
         * problem visible.
         *
         * This is deliberately a coarse software timeout.  The actual
         * START detection remains interrupt driven, so SDA is never driven
         * by the analyzer.
         */
        while(!gpio_i2c_trigger_seen() && timeout--)
        {
            __asm__("nop");
        }

        if(!gpio_i2c_trigger_seen())
        {
            uart_print("I2C START timeout: no SDA falling edge with SCL HIGH.\r\n");
            uart_print("Check CH0=SDA, CH1=SCL, common GND, pull-ups, and reset/wake the master.\r\n");
            return 0;
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


static void print_sample_time(uint32_t samples)
{
    uint64_t ns;
    uint32_t us;
    uint32_t frac;

    if(sample_rate == 0u)
        return;

    ns = ((uint64_t)samples * 1000000000ull) / (uint64_t)sample_rate;
    us = (uint32_t)(ns / 1000ull);
    frac = (uint32_t)(ns % 1000ull);

    uart_print_uint(us);
    uart_putc('.');
    uart_putc((char)('0' + ((frac / 100u) % 10u)));
    uart_putc((char)('0' + ((frac / 10u) % 10u)));
    uart_putc((char)('0' + (frac % 10u)));
    uart_print(" us");
}

static int32_t find_edge_after(uint8_t channel, uint8_t rising, uint32_t start)
{
    uint32_t i;
    uint8_t mask = (uint8_t)(1u << (channel & 3u));

    if(start < 1u)
        start = 1u;

    for(i = start; i < CAPTURE_SAMPLES; ++i)
    {
        uint8_t prev = buffer[i - 1u] & mask;
        uint8_t curr = buffer[i] & mask;

        if(rising)
        {
            if(!prev && curr)
                return (int32_t)i;
        }
        else
        {
            if(prev && !curr)
                return (int32_t)i;
        }
    }

    return -1;
}

static void print_compact_waveform(void)
{
    const uint32_t columns = 80u;
    uint32_t ch;
    uint32_t col;
    static const char *boot_names[4] = {
        "PA0/Q1 ", "ESP/Q2 ", "BOOT0  ", "NRST   "
    };

    uart_print("\r\nCompressed waveform (HIGH='-' LOW='_' mixed='*')\r\n");
    uart_print("Each column ~= ");
    print_sample_time((CAPTURE_SAMPLES + columns - 1u) / columns);
    uart_print("\r\n");

    for(ch = 0u; ch < 4u; ++ch)
    {
        uint8_t mask = (uint8_t)(1u << ch);
        if(boot_timing_profile)
            uart_print(boot_names[ch]);
        else
        {
            uart_print("CH");
            uart_putc((char)('0' + ch));
            uart_print("    ");
        }
        uart_print("|");

        for(col = 0u; col < columns; ++col)
        {
            uint32_t start = (col * CAPTURE_SAMPLES) / columns;
            uint32_t end = ((col + 1u) * CAPTURE_SAMPLES) / columns;
            uint8_t first;
            uint8_t mixed = 0u;
            uint32_t i;

            if(end <= start)
                end = start + 1u;
            if(end > CAPTURE_SAMPLES)
                end = CAPTURE_SAMPLES;

            first = buffer[start] & mask;
            for(i = start + 1u; i < end; ++i)
            {
                if((buffer[i] & mask) != first)
                {
                    mixed = 1u;
                    break;
                }
            }

            if(mixed)
                uart_putc('*');
            else
                uart_putc(first ? '-' : '_');
        }
        uart_print("|\r\n");
    }
}

static void decode_boot_timing(void)
{
    int32_t pa0_rise;
    int32_t boot0_fall;
    int32_t nrst_rise;
    int32_t esp_rise;
    uint8_t initial = buffer[0];

    uart_print("\r\nBOOT AUTH / RESET TIMING\r\n");
    uart_print("------------------------\r\n");
    uart_print("Mapping: CH0=PA0/Q1 gate, CH1=ESP GPIO0/Q2 gate, CH2=BOOT0, CH3=NRST\r\n");
    uart_print("Trigger: NRST falling (reset asserted)\r\n");
    uart_print("Initial sample: PA0="); uart_putc((initial & 1u) ? '1' : '0');
    uart_print(" ESP="); uart_putc((initial & 2u) ? '1' : '0');
    uart_print(" BOOT0="); uart_putc((initial & 4u) ? '1' : '0');
    uart_print(" NRST="); uart_putc((initial & 8u) ? '1' : '0');
    uart_print("\r\n");

    pa0_rise = find_edge_after(0u, 1u, 1u);
    esp_rise = find_edge_after(1u, 1u, 1u);
    boot0_fall = find_edge_after(2u, 0u, 1u);
    nrst_rise = find_edge_after(3u, 1u, 1u);

    uart_print("PA0 release (LOW->HIGH): ");
    if(pa0_rise >= 0) { uart_print("S="); uart_print_uint((uint32_t)pa0_rise); uart_print("  t="); print_sample_time((uint32_t)pa0_rise); }
    else uart_print("not seen");
    uart_print("\r\n");

    uart_print("ESP gate release (LOW->HIGH): ");
    if(esp_rise >= 0) { uart_print("S="); uart_print_uint((uint32_t)esp_rise); uart_print("  t="); print_sample_time((uint32_t)esp_rise); }
    else uart_print("not seen");
    uart_print("\r\n");

    uart_print("BOOT0 fall (HIGH->LOW): ");
    if(boot0_fall >= 0) { uart_print("S="); uart_print_uint((uint32_t)boot0_fall); uart_print("  t="); print_sample_time((uint32_t)boot0_fall); }
    else uart_print("not seen");
    uart_print("\r\n");

    uart_print("NRST release (LOW->HIGH): ");
    if(nrst_rise >= 0) { uart_print("S="); uart_print_uint((uint32_t)nrst_rise); uart_print("  t="); print_sample_time((uint32_t)nrst_rise); }
    else uart_print("not seen");
    uart_print("\r\n");

    if(pa0_rise >= 0 && boot0_fall >= pa0_rise)
    {
        uart_print("PA0 release -> BOOT0 fall: ");
        print_sample_time((uint32_t)(boot0_fall - pa0_rise));
        uart_print("\r\n");
    }

    if(nrst_rise >= 0)
    {
        if(boot0_fall < 0)
        {
            uart_print("BOOT0 at NRST release: HIGH (fall not seen in window)\r\n");
        }
        else if(boot0_fall > nrst_rise)
        {
            uart_print("NRST release -> BOOT0 fall margin: ");
            print_sample_time((uint32_t)(boot0_fall - nrst_rise));
            uart_print("  PASS\r\n");
        }
        else
        {
            uart_print("BOOT0 fell BEFORE NRST release by ");
            print_sample_time((uint32_t)(nrst_rise - boot0_fall));
            uart_print("  FAIL for ROM-boot hold\r\n");
        }
    }

    print_compact_waveform();
}

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

    /* The capture begins immediately after the START trigger. */
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
    {
        decode_edges();
        if(boot_timing_profile)
            decode_boot_timing();
        else
            print_compact_waveform();
    }
    else
        decode_i2c();

    raw_stats();
    uart_print("\r\nDONE\r\n");
}


void capture_boot_timing(void)
{
    boot_timing_profile = 1u;
    capture_set_mode(MODE_EDGE);
    capture_set_rate_enum(RATE_100K);
    capture_set_trigger(3u, 0u);

    uart_print("\r\nBOOT TIMING PROFILE\r\n");
    uart_print("CH0 = target PA0 / Q1 authorization gate\r\n");
    uart_print("CH1 = ESP GPIO0 / Q2 boot gate\r\n");
    uart_print("CH2 = target STM32 BOOT0\r\n");
    uart_print("CH3 = target STM32 NRST\r\n");
    uart_print("Rate = 100000 Hz (10 us/sample)\r\n");
    uart_print("Window = 81.92 ms for 8192 samples\r\n");
    uart_print("Trigger = CH3 falling (NRST assert)\r\n");
    uart_print("NOTE: current ESP reset-low interval is about 25 ms; 4 MHz cannot capture release.\r\n");

    capture_run();
    boot_timing_profile = 0u;
}
