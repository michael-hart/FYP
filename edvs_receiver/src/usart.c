/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"

//#include "stdio.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "usart.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
#define USART_GPIO GPIOA
#define BUFFER_LENGTH 40    //length of TX, RX and command buffers
#define USART_ECHO
#define USART_BAUD_RATE 1000000

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
xQueueHandle usart_txq, usart_rxq;


/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void hal_init(void);
static void irq_init(void);
static void tasks_init(void);

static void usart_tx_task(void *pvParameters);
static void usart_rx_task(void *pvParameters);

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void USART_Config(void)
{
    hal_init();
    irq_init();
    tasks_init();

}

void USART_SendByte(uint8_t data)
{
//    xQueueSend(usart_txq, &data, 100/portTICK_RATE_MS);
//    xQueueSend(usart_txq, &data, 100);
    xQueueSend(usart_txq, &data, portMAX_DELAY);
}

void USART_SendString(char * str)
{
    while(* str) {
        USART_SendByte(*str);
        str++;
    }
}

void USART1_IRQHandler(void)
{
    uint8_t data;
    static portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART1, USART_IT_RXNE)==SET) {
        data = USART_ReceiveData(USART1);
        xQueueSendFromISR(usart_rxq, &data, &xHigherPriorityTaskWoken);
    //  if (xHigherPriorityTaskWoken==pdTRUE)
    //      portSWITCH_CONTEXT();

        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}


/*******************************************************************************
 * Private Function Definitions (static)
 ******************************************************************************/
static void hal_init(void)
{
    GPIO_InitTypeDef port_init;
    USART_InitTypeDef usart_init;
  
    //GPIO init: USART1 PA9 as OUT, PA10 as IN
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_GPIOA, ENABLE );
    
    port_init.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
    port_init.GPIO_Mode = GPIO_Mode_AF;
    port_init.GPIO_Speed = GPIO_Speed_2MHz;
    port_init.GPIO_OType = GPIO_OType_PP;
    port_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init( GPIOA, &port_init );
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource9, GPIO_AF_1);
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource10, GPIO_AF_1);


    //USART init: USART1 3M 8n1
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    usart_init.USART_BaudRate = USART_BAUD_RATE;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, (USART_InitTypeDef*) &usart_init);
    USART_Cmd(USART1, ENABLE);

}

static void irq_init(void)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPriority = 3;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

}

static void tasks_init(void)
{
    usart_txq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));
    usart_rxq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));

    xTaskCreate(usart_tx_task, (char const *)"USART_T", configMINIMAL_STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(usart_rx_task, (char const *)"USART_R", configMINIMAL_STACK_SIZE, (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
}

static void usart_tx_task(void *pvParameters)
{
    uint8_t data;
    for (;;) {
        
        if (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
            vTaskDelay(1);
        } else {
            GPIO_ResetBits(GPIOC, GPIO_Pin_8);
            if (pdPASS == xQueueReceive(usart_txq, &data, portMAX_DELAY)) {
                GPIO_SetBits(GPIOC, GPIO_Pin_8);
                USART_SendData(USART1, data);
            }
        }
    }
}

static void usart_rx_task(void *pvParameters)
{
    uint8_t i = 0;
    char data_buf[BUFFER_LENGTH];
    char cmd_buf[5];

    for (;;) {
        if (pdPASS == xQueueReceive(usart_rxq, &data_buf[i++], portMAX_DELAY)) {

#ifdef USART_ECHO
            USART_SendByte(data_buf[i-1]);
#endif

            if (data_buf[i-1] == '\r')
            {
                /* Copy out the command characters */
                memcpy(cmd_buf, data_buf, 4);
                cmd_buf[4] = 0;

                /* Switch based on the command */
                if (strcmp(cmd_buf, "id  ") == 0)
                {
                    USART_SendString("Interface\r");
                }

                i = 0;
            }

            /* If buffer is overflowing, reset to 0 */
            if (i == BUFFER_LENGTH-1)
            {
                i = 0;
            }

        }
        else
        {
            i = 5;
        }
    }
}

/*******************************************************************************
 * End of file
 ******************************************************************************/