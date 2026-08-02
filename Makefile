PROJECT = LogicAnalyzer

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

CFLAGS = \
-mcpu=cortex-m3 \
-mthumb \
-DSTM32F10X_MD \
-DUSE_STDPERIPH_DRIVER \
-I./CMSIS \
-I./STM32F10x_StdPeriph_Driver/inc \
-I./CMSIS/CM3/DeviceSupport/ST/STM32F10x \
-I./CMSIS/CM3/CoreSupport \
-I./inc \
-O2 \
-g3

LDFLAGS = \
-mcpu=cortex-m3 \
-mthumb \
-Tstm32f103rb.ld \
-Wl,--gc-sections


SRC = \
src/main.c \
src/uart.c \
src/sampler.c \
STM32F10x_StdPeriph_Driver/src/stm32f10x_tim.c \
STM32F10x_StdPeriph_Driver/src/stm32f10x_dma.c \
STM32F10x_StdPeriph_Driver/src/misc.c \
src/timer.c \
src/gpio.c \
src/cli.c \
src/capture.c \
system_stm32f10x.c \
STM32F10x_StdPeriph_Driver/src/stm32f10x_gpio.c \
STM32F10x_StdPeriph_Driver/src/stm32f10x_rcc.c \
STM32F10x_StdPeriph_Driver/src/stm32f10x_usart.c


STARTUP = startup/startup_stm32f10x_md.s


OBJ = $(SRC:.c=.o)


all: $(PROJECT).elf $(PROJECT).bin


$(PROJECT).elf: $(OBJ) $(STARTUP)
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(OBJ) $(STARTUP) \
	-o $@


$(PROJECT).bin: $(PROJECT).elf
	$(OBJCOPY) -O binary $< $@


size:
	$(SIZE) $(PROJECT).elf


clean:
	rm -f $(OBJ)
	rm -f $(PROJECT).elf
	rm -f $(PROJECT).bin
