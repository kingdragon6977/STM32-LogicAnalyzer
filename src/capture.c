#include "stm32f10x.h"
#include "sampler.h"
#include "capture.h"
#include "gpio.h"
#include "uart.h"


extern volatile uint32_t irq_count;

//#define CAPTURE_SAMPLES 16384


static uint8_t buffer[CAPTURE_SAMPLES];



static uint8_t trigger_channel = 0;

static uint8_t trigger_rising = 1;



static analyzer_mode_t mode = MODE_EDGE;


static uint32_t sample_rate = 1000000;



static uint32_t edge_count[4];





void capture_init(void)
{
    capture_set_trigger(0,1);
}





void capture_set_mode(analyzer_mode_t new_mode)
{
    mode = new_mode;
}



analyzer_mode_t capture_get_mode(void)
{
    return mode;
}





void capture_set_rate(uint32_t hz)
{
    sample_rate = hz;
}



uint32_t capture_get_rate(void)
{
    return sample_rate;
}





void capture_set_rate_enum(capture_rate_t rate)
{
    switch(rate)
    {
		case RATE_1K:
			capture_set_rate(1000);
			break;
		
        case RATE_100K:
            capture_set_rate(100000);
            break;


        case RATE_500K:
            capture_set_rate(500000);
            break;


        case RATE_1M:
            capture_set_rate(1000000);
            break;


        case RATE_2M:
            capture_set_rate(2000000);
            break;
    }


    uart_print("Sample rate set.\r\n");
}





void capture_set_trigger(uint8_t channel,uint8_t rising)
{
    trigger_channel = channel & 3;

    trigger_rising = rising ? 1 : 0;
}







static uint8_t wait_for_trigger(void)
{
    uint8_t mask = 1 << trigger_channel;


    uint8_t last = logic_read();


    uart_print("Waiting for trigger...\r\n");



    while(1)
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
}






static void decode_edges(void)
{
    uint32_t i;


    uint8_t last = buffer[0];


    uart_print("\r\nEdges Found:\r\n");



    for(i=1;i<CAPTURE_SAMPLES;i++)
    {
        uint8_t diff = last ^ buffer[i];



        if(diff & 0x01)
            edge_count[0]++;


        if(diff & 0x02)
            edge_count[1]++;


        if(diff & 0x04)
            edge_count[2]++;


        if(diff & 0x08)
            edge_count[3]++;




        if(diff)
        {
            uart_print("S=");
            uart_print_uint(i);


            uart_print(" [");


            uart_putc(
                (buffer[i]&8)?'1':'0'
            );

            uart_putc(
                (buffer[i]&4)?'1':'0'
            );

            uart_putc(
                (buffer[i]&2)?'1':'0'
            );

            uart_putc(
                (buffer[i]&1)?'1':'0'
            );


            uart_print("] ");




            if(diff&1)
                uart_print(
                (buffer[i]&1)?
                "CH0↑ ":"CH0↓ "
                );


            if(diff&2)
                uart_print(
                (buffer[i]&2)?
                "CH1↑ ":"CH1↓ "
                );


            if(diff&4)
                uart_print(
                (buffer[i]&4)?
                "CH2↑ ":"CH2↓ "
                );


            if(diff&8)
                uart_print(
                (buffer[i]&8)?
                "CH3↑ ":"CH3↓ "
                );



            uart_print("\r\n");
        }


        last = buffer[i];
    }




    uart_print("\r\nSummary\r\n");

    uart_print("CH0 edges: ");
    uart_print_uint(edge_count[0]);
    uart_print("\r\n");

    uart_print("CH1 edges: ");
    uart_print_uint(edge_count[1]);
    uart_print("\r\n");

    uart_print("CH2 edges: ");
    uart_print_uint(edge_count[2]);
    uart_print("\r\n");

    uart_print("CH3 edges: ");
    uart_print_uint(edge_count[3]);
    uart_print("\r\n");
}







static void decode_i2c(void)
{
    uint32_t i;


    uart_print("\r\nI2C Scan\r\n");



    for(i=1;i<CAPTURE_SAMPLES;i++)
    {
        uint8_t prev = buffer[i-1];

        uint8_t curr = buffer[i];



        uint8_t scl =
            (curr >> 1) & 1;


        uint8_t sda_old =
            (prev >> 2) & 1;


        uint8_t sda_new =
            (curr >> 2) & 1;




        if(scl)
        {
            if(sda_old && !sda_new)
            {
                uart_print("START @ ");
                uart_print_uint(i);
                uart_print("\r\n");
            }


            if(!sda_old && sda_new)
            {
                uart_print("STOP @ ");
                uart_print_uint(i);
                uart_print("\r\n");
            }
        }
    }
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

    uart_print("Edges: ");
    uart_print_uint(edges);
    uart_print("\r\n");
}



void capture_run(void)
{
    uint32_t i;



    for(i=0;i<4;i++)
        edge_count[i]=0;



    uart_print("\r\n====================================\r\n");

    uart_print("STM32 Logic Analyzer v1.0\r\n");

    uart_print("====================================\r\n");


    uart_print("Samples : ");
    uart_print_uint(CAPTURE_SAMPLES);
    uart_print("\r\n");


    uart_print("Rate : ");
    uart_print_uint(sample_rate);
    uart_print(" Hz\r\n");



    uart_print("Analyzer Armed\r\n");



    if(!wait_for_trigger())
        return;



    uart_print("Capture Started\r\n");



    uart_print("Hardware sampler running...\r\n");


sampler_start(
    buffer,
    CAPTURE_SAMPLES
);


while(!sampler_done())
{
    /*
     * CPU is free here.
     * Could update LEDs,
     * handle UART,
     * etc.
     */
}


uart_print("Sampling complete\r\n");




    if(mode==MODE_EDGE)
        decode_edges();



    if(mode==MODE_I2C)
        decode_i2c();



    uart_print("\r\nDONE\r\n");
}








void capture_raw(void)
{
    uint32_t i;

    uart_print("\r\nRAW CAPTURE\r\n");

    uart_print("Starting sampler...\r\n");

    sampler_start(
        buffer,
        CAPTURE_SAMPLES
    );

    uart_print("Sampler running\r\n");


    while(!sampler_done())
    {
        /*
         * wait for DMA/timer sampler
         */
    }


    uart_print("Sampler finished\r\n");

    uart_print("IRQ Count: ");
    uart_print_uint(irq_count);
    uart_print("\r\n");


    uart_print("Searching transitions...\r\n");


    uint8_t last = buffer[0];


    for(i = 1; i < CAPTURE_SAMPLES; i++)
    {
        if(buffer[i] != last)
        {
            uart_print("Transition at sample ");
            uart_print_uint(i);

            uart_print(" : ");

            uart_print_hex8(last);

            uart_print(" -> ");

            uart_print_hex8(buffer[i]);

            uart_print("\r\n");
        }


        last = buffer[i];
    }


    raw_stats();


    uart_print("\r\nDONE\r\n");
}
