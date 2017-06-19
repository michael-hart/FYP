/*******************************************************************************
 * Global Includes
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "stm32f0xx_it.h"
#include "spinn_channel.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void HardFault_Handler(void)
{
    uint8_t i = 0;
    i++;
    while(1);
}

void EXTI4_15_IRQHandler(void)
{
    long lHigherPriorityTaskWoken = pdFALSE;
    if (EXTI_GetITStatus(EXTI_Line7) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnTxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line7);
    }

    if (EXTI_GetITStatus(EXTI_Line8) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line8);
    }

    if (EXTI_GetITStatus(EXTI_Line9) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line9);
    }

    if (EXTI_GetITStatus(EXTI_Line10) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line10);
    }

    if (EXTI_GetITStatus(EXTI_Line11) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line11);
    }

    if (EXTI_GetITStatus(EXTI_Line12) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line12);
    }

    if (EXTI_GetITStatus(EXTI_Line13) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line13);
    }

    if (EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
        xSemaphoreGiveFromISR( xSpinnRxSemaphore, &lHigherPriorityTaskWoken );
        EXTI_ClearITPendingBit(EXTI_Line14);
    }

    portEND_SWITCHING_ISR( lHigherPriorityTaskWoken );
}

/*******************************************************************************
 * Private Function Definitions (static)
 ******************************************************************************/
/* None */

/*******************************************************************************
 * End of file
 ******************************************************************************/