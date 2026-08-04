#include "stm32f10x.h"
#include "test_signal.h"
#include "sampler.h"
#include "uart.h"
#include "gpio.h"
#include "capture.h"
#include "cli.h"


static void delay(volatile uint32_t d)
{
    while(d--);
}



int main(void)
{
    SystemInit();


    uart_init();

    gpio_init();

    sampler_init();
	
	test_signal_init();
	
    capture_init();


    uart_print_banner();


    while(1)
    {

        cli_task();


        if(button_pressed())
        {
            capture_run();


            while(button_pressed());

            delay(50000);
        }


        led_toggle();

        delay(500000);
    }
}
