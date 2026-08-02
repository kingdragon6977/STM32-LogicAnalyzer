#ifndef __STM32F10x_CONF_H
#define __STM32F10x_CONF_H

#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#endif
#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line);

#else

#define assert_param(expr) ((void)0)

#endif
