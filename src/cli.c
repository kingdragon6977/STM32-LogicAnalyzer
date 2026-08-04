#include "cli.h"
#include "test_signal.h"
#include "uart.h"
#include "capture.h"

#include <string.h>
#include <stdint.h>


static char cmd[64];

static uint8_t cmd_index = 0;



static void process_command(void)
{
    cmd[cmd_index] = 0;


    uart_print("\r\n");


    /*
     * HELP
     */

    if(strcmp(cmd,"help")==0)
    {
        uart_print("Commands:\r\n");

        uart_print(" help\r\n");

        uart_print(" capture\r\n");

        uart_print(" capture raw\r\n");

        uart_print(" mode edge\r\n");

        uart_print(" mode i2c\r\n");
		
		uart_print(" rate 1k\r\n");
		
        uart_print(" rate 100k\r\n");

        uart_print(" rate 500k\r\n");

        uart_print(" rate 1m\r\n");

        uart_print(" rate 2m\r\n");

        uart_print(" trigger ch0 rising\r\n");

        uart_print(" trigger ch0 falling\r\n");
		
		uart_print(" test on\r\n");
		
		uart_print(" test off\r\n");
		
		uart_print(" test 1k\r\n");
		
		uart_print(" test 10k\r\n");
		
		uart_print(" test 100k\r\n");
		
		uart_print(" test 500k\r\n");
		
        uart_print(" status\r\n");
    }



    /*
     * CAPTURE
     */

    else if(strcmp(cmd,"capture")==0)
    {
        capture_run();
    }



    /*
     * RAW
     */

    else if(strcmp(cmd,"capture raw")==0)
    {
        capture_raw();
    }



    /*
     * MODES
     */

    else if(strcmp(cmd,"mode edge")==0)
    {
        capture_set_mode(MODE_EDGE);

        uart_print("Mode: EDGE\r\n");
    }


    else if(strcmp(cmd,"mode i2c")==0)
    {
        capture_set_mode(MODE_I2C);

        uart_print("Mode: I2C\r\n");
    }



    /*
     * SAMPLE RATES
     */
	
	else if(strcmp(cmd,"rate 1k")==0)
	{
		capture_set_rate_enum(RATE_1K);
	}
	
	
    else if(strcmp(cmd,"rate 100k")==0)
    {
        capture_set_rate_enum(RATE_100K);
    }


    else if(strcmp(cmd,"rate 500k")==0)
    {
        capture_set_rate_enum(RATE_500K);
    }


    else if(strcmp(cmd,"rate 1m")==0)
    {
        capture_set_rate_enum(RATE_1M);
    }


    else if(strcmp(cmd,"rate 2m")==0)
    {
        capture_set_rate_enum(RATE_2M);
    }



    /*
     * TRIGGERS
     */

    else if(strcmp(cmd,"trigger ch0 rising")==0)
    {
        capture_set_trigger(0,1);

        uart_print("Trigger CH0 rising\r\n");
    }


    else if(strcmp(cmd,"trigger ch0 falling")==0)
    {
        capture_set_trigger(0,0);

        uart_print("Trigger CH0 falling\r\n");
    }



    /*
     * STATUS
     */

    else if(strcmp(cmd,"status")==0)
    {
        uart_print("Analyzer status\r\n");

        uart_print("----------------\r\n");

        uart_print("Mode: ");

        if(capture_get_mode()==MODE_I2C)
            uart_print("I2C\r\n");
        else
            uart_print("EDGE\r\n");


        uart_print("Rate: ");

        uart_print_uint(
            capture_get_rate()
        );

        uart_print(" Hz\r\n");
    }

		else if(strcmp(cmd,"test on")==0)
	{
		test_signal_enable();

		uart_print("Test output enabled\r\n");
	}


	else if(strcmp(cmd,"test off")==0)
	{
		test_signal_disable();

		uart_print("Test output disabled\r\n");
	}


	else if(strcmp(cmd,"test 1k")==0)
	{
		test_signal_set_rate(1000);

		uart_print("Test frequency: 1000 Hz\r\n");
	}


	else if(strcmp(cmd,"test 10k")==0)
	{
		test_signal_set_rate(10000);

		uart_print("Test frequency: 10000 Hz\r\n");
	}


	else if(strcmp(cmd,"test 100k")==0)
	{
		test_signal_set_rate(100000);

		uart_print("Test frequency: 100000 Hz\r\n");
	}


	else if(strcmp(cmd,"test 500k")==0)
	{
		test_signal_set_rate(500000);

		uart_print("Test frequency: 500000 Hz\r\n");
	}


    else
    {
        uart_print("Unknown command\r\n");
    }



    uart_print("> ");

    cmd_index = 0;
}




void cli_task(void)
{
    while(uart_available())
    {
        char c = uart_getc();



        if(c=='\r' || c=='\n')
        {
            if(cmd_index)
                process_command();
        }


        else
        {
            if(cmd_index < sizeof(cmd)-1)
            {
                cmd[cmd_index++] = c;

                uart_putc(c);
            }
        }
    }
}
