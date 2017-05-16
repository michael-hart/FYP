/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "dvs_usart.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
#define USART_GPIO GPIOA
#define BUFFER_LENGTH 40    //length of TX, RX and command buffers
#define USART_BAUD_RATE 500000


/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
xQueueHandle dvs_rxq;


/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void hal_init(void);
static void irq_init(void);
static void tasks_init(void);

static void usart_rx_task(void *pvParameters);

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void DVS_Config(void)
{
    hal_init();
    irq_init();
    tasks_init();

}

void USART1_IRQHandler(void)
{
    uint8_t data;
    static portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART1, USART_IT_RXNE)==SET) {
        data = USART_ReceiveData(USART1);
        xQueueSendFromISR(dvs_rxq, &data, &xHigherPriorityTaskWoken);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}


/*******************************************************************************
 * Private Function Definitions (static)
 ******************************************************************************/

/**
 * DESCRIPTION
 * Configure hardware to allow USART connection
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void hal_init(void)
{
    GPIO_InitTypeDef port_init;
    USART_InitTypeDef usart_init;
  
    //GPIO init: USART1 PA10 as IN, no OUT
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_GPIOA, ENABLE );
    
    port_init.GPIO_Pin = GPIO_Pin_10;
    port_init.GPIO_Mode = GPIO_Mode_AF;
    port_init.GPIO_Speed = GPIO_Speed_2MHz;
    port_init.GPIO_OType = GPIO_OType_PP;
    port_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init( GPIOA, &port_init );
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource10, GPIO_AF_1);


    //USART init: USART1 500K 8n1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    usart_init.USART_BaudRate = USART_BAUD_RATE;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_Mode = USART_Mode_Rx;

    USART_Init(USART1, (USART_InitTypeDef*) &usart_init);
    USART_Cmd(USART1, ENABLE);

}

/**
 * DESCRIPTION
 * Initialise and register interrupt routines
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void irq_init(void)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPriority = 3;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
}

/**
 * DESCRIPTION
 * Initialise task and start running
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void tasks_init(void)
{
    dvs_rxq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));
    xTaskCreate(usart_rx_task, (char const *)"DVS_R", configMINIMAL_STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
}


/**
 * DESCRIPTION
 * Task to retrieve queued DVS bytes, convert to struct format, and queue up
 * 
 * INPUTS
 * pvParameters (void*) : FreeRTOS struct with task information
 *
 * RETURNS
 * Nothing
 */
static void usart_rx_task(void *pvParameters)
{
    uint8_t i = 0;
    char data_buf[BUFFER_LENGTH];
    char coord_buf[2];

    for (;;) {
        if (pdPASS == xQueueReceive(dvs_rxq, &data_buf[i++], portMAX_DELAY)) {

            if (i >= 2)
            {
                /* Copy out the data bytes */
                memcpy(coord_buf, data_buf, 2);

                /* TODO: Act upon the co-ordinates */
                i = 0;
            }

            /* If buffer is overflowing, clear it */
            if (i == BUFFER_LENGTH)
            {
                i = 0;
            }

        }
    }
}

/*******************************************************************************
 * End of file
 ******************************************************************************/