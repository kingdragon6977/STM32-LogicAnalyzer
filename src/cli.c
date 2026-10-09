#include "cli.h"
#include "test_signal.h"
#include "uart.h"
#include "capture.h"
#include "i2c_master.h"
#include "freq_counter.h"
#include "board.h"
#include "sampler.h"
#include <string.h>
#include <stdint.h>

#define CLI_LINE_SIZE 64
#define CLI_HISTORY_SIZE 8

static char cmd[CLI_LINE_SIZE];
static uint8_t cmd_index = 0;
static uint8_t cursor_index = 0;
static char history[CLI_HISTORY_SIZE][CLI_LINE_SIZE];
static uint8_t history_count = 0;
static uint8_t history_head = 0;
static int8_t history_pos = -1;
static uint8_t esc_state = 0;
static uint8_t cli_started = 0;

static void cli_redraw(void)
{
    uint8_t i;

    uart_print("\r\033[2K> ");
    uart_print(cmd);

    for(i = cmd_index; i > cursor_index; --i)
        uart_print("\033[D");
}

static void cli_history_store(const char *line)
{
    uint8_t last;

    if(!line[0])
        return;

    if(history_count)
    {
        last = (uint8_t)((history_head + CLI_HISTORY_SIZE - 1u) % CLI_HISTORY_SIZE);
        if(strcmp(history[last], line) == 0)
            return;
    }

    strncpy(history[history_head], line, CLI_LINE_SIZE - 1u);
    history[history_head][CLI_LINE_SIZE - 1u] = 0;
    history_head = (uint8_t)((history_head + 1u) % CLI_HISTORY_SIZE);

    if(history_count < CLI_HISTORY_SIZE)
        history_count++;
}

static void cli_history_recall(int direction)
{
    uint8_t oldest;
    uint8_t slot;

    if(!history_count)
        return;

    if(direction < 0)
    {
        if(history_pos < 0)
            history_pos = (int8_t)history_count - 1;
        else if(history_pos > 0)
            history_pos--;
    }
    else
    {
        if(history_pos < 0)
            return;

        if(history_pos < (int8_t)history_count - 1)
            history_pos++;
        else
        {
            history_pos = -1;
            cmd[0] = 0;
            cmd_index = 0;
            cursor_index = 0;
            cli_redraw();
            return;
        }
    }

    oldest = (uint8_t)((history_head + CLI_HISTORY_SIZE - history_count) % CLI_HISTORY_SIZE);
    slot = (uint8_t)((oldest + (uint8_t)history_pos) % CLI_HISTORY_SIZE);
    strcpy(cmd, history[slot]);
    cmd_index = (uint8_t)strlen(cmd);
    cursor_index = cmd_index;
    cli_redraw();
}


static void audio_test_help(void)
{
    uart_print("Audio test output: PA6 / TIM3_CH1, 1 kHz, 50% duty, 3.3 V logic square wave\r\n");
    uart_print("DO NOT connect PA6 directly to an analog codec input.\r\n");
    uart_print("Use attenuation + AC coupling before TLV320ADC3101 IN1L(P).\r\n");
    uart_print("Suggested: PA6 -> 100k -> node -> 10k to GND -> 1uF series capacitor -> IN1L(P)\r\n");
    uart_print("Tie analyzer GND and audio_controller GND together.\r\n");
}

static void process_command(void)
{
    cmd[cmd_index] = 0;
    uart_print("\r\n");

    if(strcmp(cmd,"help")==0)
    {
        uart_print("Commands:\r\n");
        uart_print(" help\r\n");
        uart_print(" capture\r\n");
        uart_print(" capture raw\r\n");
        uart_print(" boot capture       (project profile: CH0=PA0 CH1=ESP CH2=BOOT0 CH3=NRST)\r\n");
        uart_print(" boot status        (show project timing setup)\r\n");
        uart_print(" freq\r\n");
        uart_print(" mode edge\r\n");
        uart_print(" mode i2c\r\n");
        uart_print(" i2c snoop\r\n");
        uart_print(" rate 1k\r\n");
        uart_print(" rate 100k\r\n");
        uart_print(" rate 500k\r\n");
        uart_print(" rate 1m\r\n");
        uart_print(" rate 2m\r\n");
        uart_print(" rate 4m\r\n");
        uart_print(" i2c scan\r\n");
        uart_print(" i2c test\r\n");
        uart_print(" i2c capture-test\r\n");
        uart_print(" i2c pins\r\n");
        uart_print(" i2c release\r\n");
        uart_print(" i2c drive\r\n");
        uart_print(" i2c alt-sda\r\n");
        uart_print(" trigger ch0 rising\r\n");
        uart_print(" trigger ch0 falling\r\n");
        uart_print(" trigger ch1 rising\r\n");
        uart_print(" trigger ch1 falling\r\n");
        uart_print(" trigger ch2 rising\r\n");
        uart_print(" trigger ch2 falling\r\n");
        uart_print(" trigger ch3 rising\r\n");
        uart_print(" trigger ch3 falling\r\n");
        uart_print(" test on\r\n");
        uart_print(" test off\r\n");
        uart_print(" test 1k\r\n");
        uart_print(" test 10k\r\n");
        uart_print(" test 100k\r\n");
        uart_print(" test 500k\r\n");
        uart_print(" audio-test on      (set + start 1 kHz on PA6)\r\n");
        uart_print(" audio-test off\r\n");
        uart_print(" audio-test wiring\r\n");
        uart_print(" backend dma\r\n");
        uart_print(" backend irq\r\n");
        uart_print(" status\r\n");
        uart_print(" Line editing: Left/Right, Home/End, Backspace/Delete; Up/Down = 8-command history\r\n");
    }
    else if(strcmp(cmd,"capture")==0)
    {
        capture_run();
    }
    else if(strcmp(cmd,"capture raw")==0)
    {
        capture_raw();
    }
    else if(strcmp(cmd,"boot capture")==0)
    {
        capture_boot_timing();
    }
    else if(strcmp(cmd,"boot status")==0)
    {
        uart_print("BOOT TIMING PROJECT PROFILE\r\n");
        uart_print("---------------------------\r\n");
        uart_print("CH0 = target PA0 / Q1 authorization gate\r\n");
        uart_print("CH1 = ESP GPIO0 / Q2 boot gate\r\n");
        uart_print("CH2 = target STM32 BOOT0\r\n");
        uart_print("CH3 = target STM32 NRST\r\n");
        uart_print("Recommended: boot capture\r\n");
        uart_print("Boot capture uses 100 kHz = 10 us/sample, 81.92 ms total window.\r\n");
        uart_print("Trigger: CH3 falling (NRST asserted).\r\n");
        uart_print("Reason: ESP currently holds NRST low ~25 ms, so 4 MHz/2.048 ms misses release.\r\n");
    }
    else if(strcmp(cmd,"freq")==0)
    {
        freq_counter_measure();
    }
    else if(strcmp(cmd,"i2c snoop")==0)
    {
        /* Passive only: this command never initializes or drives the I2C master. */
        capture_set_mode(MODE_I2C);
        capture_set_rate_enum(RATE_4M);
        capture_set_trigger(0,0);
        uart_print("I2C SNOOP armed: CH0=SDA, CH1=SCL, CH2/CH3=aux\r\n");
        uart_print("PASSIVE ONLY - analyzer will not drive SDA/SCL\r\n");
        uart_print("Sampling at 4 MHz; waiting for SDA falling while SCL is HIGH\r\n");
        capture_run();
    }
    else if(strcmp(cmd,"i2c scan")==0)
    {
        i2c_master_init();
        i2c_master_scan();
    }
    else if(strcmp(cmd,"i2c test")==0)
    {
        i2c_master_init();
        i2c_master_test_transaction();
    }
    else if(strcmp(cmd,"i2c capture-test")==0)
    {
        i2c_master_capture_test();
    }
    else if(strcmp(cmd,"i2c pins")==0)
    {
        i2c_master_pin_diagnostic();
    }
    else if(strcmp(cmd,"i2c release")==0)
    {
        i2c_master_release_pins();
    }
    else if(strcmp(cmd,"i2c drive")==0)
    {
        i2c_master_drive_pins();
    }
    else if(strcmp(cmd,"i2c alt-sda")==0)
    {
        i2c_master_alt_sda_test();
    }
    else if(strcmp(cmd,"mode edge")==0)
    {
        capture_set_mode(MODE_EDGE);
        uart_print("Mode: EDGE\r\n");
    }
    else if(strcmp(cmd,"mode i2c")==0)
    {
        capture_set_mode(MODE_I2C);
        uart_print("Mode: I2C passive\r\n");
    }
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
    else if(strcmp(cmd,"rate 4m")==0)
    {
        capture_set_rate_enum(RATE_4M);
    }
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
    else if(strcmp(cmd,"trigger ch1 rising")==0)
    {
        capture_set_trigger(1,1);
        uart_print("Trigger CH1 rising\r\n");
    }
    else if(strcmp(cmd,"trigger ch1 falling")==0)
    {
        capture_set_trigger(1,0);
        uart_print("Trigger CH1 falling\r\n");
    }
    else if(strcmp(cmd,"trigger ch2 rising")==0)
    {
        capture_set_trigger(2,1);
        uart_print("Trigger CH2 rising\r\n");
    }
    else if(strcmp(cmd,"trigger ch2 falling")==0)
    {
        capture_set_trigger(2,0);
        uart_print("Trigger CH2 falling\r\n");
    }
    else if(strcmp(cmd,"trigger ch3 rising")==0)
    {
        capture_set_trigger(3,1);
        uart_print("Trigger CH3 rising\r\n");
    }
    else if(strcmp(cmd,"trigger ch3 falling")==0)
    {
        capture_set_trigger(3,0);
        uart_print("Trigger CH3 falling\r\n");
    }
    else if(strcmp(cmd,"backend dma")==0)
    {
        capture_set_backend_dma(1);
        uart_print("Capture backend: DMA\r\n");
    }
    else if(strcmp(cmd,"backend irq")==0)
    {
        capture_set_backend_dma(0);
        uart_print("Capture backend: IRQ\r\n");
    }
    else if(strcmp(cmd,"status")==0)
    {
        uart_print("Analyzer status\r\n");
        uart_print("----------------\r\n");
        uart_print("Mode: ");
        if(capture_get_mode()==MODE_I2C)
            uart_print("I2C PASSIVE\r\n");
        else
            uart_print("EDGE\r\n");
        uart_print("Rate: ");
        uart_print_uint(capture_get_rate());
        uart_print(" Hz\r\n");
        uart_print("Capture window: ");
        uart_print_uint((uint32_t)(((uint64_t)CAPTURE_SAMPLES * 1000000ull) / capture_get_rate()));
        uart_print(" us\r\n");
        uart_print("Trigger: CH");
        uart_putc((char)('0' + capture_get_trigger_channel()));
        uart_print(capture_get_trigger_rising() ? " rising\r\n" : " falling\r\n");
        uart_print("Backend: ");
        uart_print(capture_get_backend_dma() ? "DMA\r\n" : "IRQ\r\n");
        uart_print("Inputs: CH0=PA0 CH1=PA1 CH2=PA2 CH3=PA3\r\n");
        uart_print("Boot project: CH0=PA0/Q1 CH1=ESP/Q2 CH2=BOOT0 CH3=NRST\r\n");
        uart_print("Use 'boot capture' for the 100 kHz / 81.92 ms reset timing profile.\r\n");
        uart_print("Test output: PA6=TIM3_CH1\r\n");
        uart_print("I2C snoop: CH0=SDA CH1=SCL (no bus driving)\r\n");
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
    else if(strcmp(cmd,"audio-test on")==0)
    {
        test_signal_set_rate(1000);
        test_signal_enable();
        uart_print("AUDIO TEST ON: PA6 = 1000 Hz, 50% duty, 3.3 V logic square wave\r\n");
        uart_print("Use attenuation + AC coupling before TLV320 IN1L(P).\r\n");
    }
    else if(strcmp(cmd,"audio-test off")==0)
    {
        test_signal_disable();
        uart_print("AUDIO TEST OFF\r\n");
    }
    else if(strcmp(cmd,"audio-test wiring")==0)
    {
        audio_test_help();
    }
    else
    {
        uart_print("Unknown command\r\n");
    }

    cmd_index = 0;
    cursor_index = 0;
    cmd[0] = 0;
    history_pos = -1;
    uart_print("> ");
}

void cli_task(void)
{
    if(!cli_started)
    {
        cli_started = 1u;
        cmd[0] = 0;
        uart_print("> ");
    }

    while(uart_available())
    {
        uint8_t ch = (uint8_t)uart_getc();
        uint8_t i;

        if(esc_state == 1u)
        {
            if(ch == '[' || ch == 'O')
                esc_state = 2u;
            else
                esc_state = 0u;
            continue;
        }

        if(esc_state == 2u)
        {
            esc_state = 0u;

            if(ch == 'A')
            {
                cli_history_recall(-1);
                continue;
            }
            if(ch == 'B')
            {
                cli_history_recall(1);
                continue;
            }
            if(ch == 'C')
            {
                if(cursor_index < cmd_index)
                {
                    uart_print("\033[C");
                    cursor_index++;
                }
                continue;
            }
            if(ch == 'D')
            {
                if(cursor_index > 0u)
                {
                    uart_print("\033[D");
                    cursor_index--;
                }
                continue;
            }
            if(ch == 'H')
            {
                cursor_index = 0u;
                cli_redraw();
                continue;
            }
            if(ch == 'F')
            {
                cursor_index = cmd_index;
                cli_redraw();
                continue;
            }
            if(ch == '3')
            {
                esc_state = 3u;
                continue;
            }
            continue;
        }

        if(esc_state == 3u)
        {
            esc_state = 0u;
            if(ch == '~' && cursor_index < cmd_index)
            {
                for(i = cursor_index; i < cmd_index; ++i)
                    cmd[i] = cmd[i + 1u];
                cmd_index--;
                cli_redraw();
            }
            continue;
        }

        if(ch == 0x1Bu)
        {
            esc_state = 1u;
            continue;
        }

        if(ch == '\r' || ch == '\n')
        {
            if(ch == '\n' && cmd_index == 0u)
                continue;

            if(cmd_index)
            {
                cmd[cmd_index] = 0;
                cli_history_store(cmd);
                process_command();
            }
            else
            {
                uart_print("\r\n> ");
            }
            continue;
        }

        if(ch == 0x08u || ch == 0x7Fu)
        {
            if(cursor_index > 0u)
            {
                for(i = (uint8_t)(cursor_index - 1u); i < cmd_index; ++i)
                    cmd[i] = cmd[i + 1u];
                cursor_index--;
                cmd_index--;
                cli_redraw();
            }
            continue;
        }

        if(ch >= 0x20u && ch <= 0x7Eu && cmd_index < CLI_LINE_SIZE - 1u)
        {
            for(i = cmd_index; i > cursor_index; --i)
                cmd[i] = cmd[i - 1u];

            cmd[cursor_index] = (char)ch;
            cursor_index++;
            cmd_index++;
            cmd[cmd_index] = 0;
            cli_redraw();
        }
    }
}
