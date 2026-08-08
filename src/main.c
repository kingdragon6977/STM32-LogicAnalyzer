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

    /* continuous capture flag (toggled by long press) */
    static uint8_t continuous_mode = 0;

    while(1)
    {
        cli_task();

        if(button_pressed())
        {
            uint32_t hold = 0;

            /* detect long press by counting delay loops while button held */
            while(!button_level())
            {
                /* adjust delay and threshold to tune long-press duration */
                delay(20000);
                hold++;

                /* threshold: hold >= 50 approximates ~1s on default build */
                if(hold >= 50)
                {
                    /* long press detected: toggle continuous mode */
                    continuous_mode = !continuous_mode;

                    if(continuous_mode)
                    {
                        uart_print("Continuous capture: START (press button to stop)\r\n");

                        /* loop captures until button pressed to stop */
                        while(continuous_mode)
                        {
                            capture_run();

                            /* check for stop request between captures */
                            if(button_pressed())
                            {
                                continuous_mode = 0;
                                uart_print("Continuous capture: STOPPED\r\n");
                                /* consume bounce */
                                while(button_pressed());
                                delay(50000);
                            }
                        }
                    }
                    else
                    {
                        uart_print("Continuous capture: DISABLED\r\n");
                    }

                    /* wait for release and debounce */
                    while(!button_level());
                    delay(50000);
                    break;
                }
            }

            /* short press: single capture */
            if(hold < 50)
            {
                capture_run();

                /* wait for release and debounce */
                while(button_pressed());
                delay(50000);
            }
        }

        led_toggle();

        delay(500000);
    }
}
