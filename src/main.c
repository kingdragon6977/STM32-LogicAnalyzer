#include "stm32f10x.h"
#include "sampler.h"
#include "uart.h"
#include "gpio.h"
#include "timer.h"
#include "capture.h"
#include "cli.h"



static void delay(volatile uint32_t d)
{
    while(d--);
}



int main(void)
{
    SystemInit();


    /*
     * Hardware init
     */

    uart_init();

    gpio_init();

    timer_init();
    
    sampler_init();

    timer_set_rate(1000000);

    capture_init();

	static uint32_t count = 0;

	count++;

	if(count >= 50000)
	{
		test_pin_toggle();
		count = 0;
	}

    uart_print_banner();



    while(1)
    {

        /*
         * Serial command interface
         */

        cli_task();



        /*
         * Heartbeat
         */

        led_toggle();



        /*
         * Hardware capture button
         */

        if(button_pressed())
        {
            capture_run();


            /*
             * Wait for release
             */

            while(button_pressed());


            delay(50000);
        }



        delay(500000);
    }
}
