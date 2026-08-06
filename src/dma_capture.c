#include "stm32f10x.h"

#include "stm32f10x_dma.h"
#include "stm32f10x_tim.h"

#include "dma_capture.h"



static volatile uint8_t finished = 0;



void dma_capture_init(void)
{

    /*
     * Enable DMA1 clock
     */

    RCC_AHBPeriphClockCmd(
        RCC_AHBPeriph_DMA1,
        ENABLE
    );


    /*
     * DMA1 Channel 2
     *
     * TIM2_UP on STM32F103
     */


    DMA_DeInit(
        DMA1_Channel2
    );



    DMA_InitTypeDef dma;



    dma.DMA_PeripheralBaseAddr =
        (uint32_t)&GPIOA->IDR;



    dma.DMA_MemoryBaseAddr =
        0;



    dma.DMA_DIR =
        DMA_DIR_PeripheralSRC;



    dma.DMA_BufferSize =
        0;



    dma.DMA_PeripheralInc =
        DMA_PeripheralInc_Disable;


    dma.DMA_MemoryInc =
        DMA_MemoryInc_Enable;



    dma.DMA_PeripheralDataSize =
        DMA_PeripheralDataSize_Byte;


    dma.DMA_MemoryDataSize =
        DMA_MemoryDataSize_Byte;



    dma.DMA_Mode =
        DMA_Mode_Normal;



    dma.DMA_Priority =
        DMA_Priority_VeryHigh;



    dma.DMA_M2M =
        DMA_M2M_Disable;



    DMA_Init(
        DMA1_Channel2,
        &dma
    );



    /*
     * Enable transfer complete interrupt
     */

    DMA_ITConfig(
        DMA1_Channel2,
        DMA_IT_TC,
        ENABLE
    );

    /*
     * Enable NVIC for DMA1 Channel2
     */
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = DMA1_Channel2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}



void dma_capture_start(
    uint8_t *buffer,
    uint32_t length
)
{

    finished = 0;


    DMA_Cmd(
        DMA1_Channel2,
        DISABLE
    );


    DMA_SetCurrDataCounter(
        DMA1_Channel2,
        length
    );


    /* Some stdperiph versions don't provide a helper to set the memory address; write directly */
    DMA1_Channel2->CMAR = (uint32_t)buffer;



    DMA_ClearFlag(
        DMA1_FLAG_TC2
    );



    DMA_Cmd(
        DMA1_Channel2,
        ENABLE
    );



    /*
     * Start TIM2 DMA requests
     */

    TIM_DMACmd(
        TIM2,
        TIM_DMA_Update,
        ENABLE
    );


    TIM_Cmd(
        TIM2,
        ENABLE
    );
}



uint8_t dma_capture_done(void)
{
    return finished;
}


/*
 * DMA transfer complete IRQ handler
 */
void DMA1_Channel2_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC2))
    {
        /* Clear interrupt flag */
        DMA_ClearITPendingBit(DMA1_IT_TC2);

        /* Stop DMA requests from TIM2 */
        TIM_DMACmd(TIM2, TIM_DMA_Update, DISABLE);

        /* Stop the timer */
        TIM_Cmd(TIM2, DISABLE);

        /* Disable the DMA channel */
        DMA_Cmd(DMA1_Channel2, DISABLE);

        /* Mark finished */
        finished = 1;
    }
}
