/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "string.h"
#include <stdbool.h>

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "pc_usart.h"
#include "dvs_usart.h"
#include "spinn_channel.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
#define USART_GPIO GPIOA
#define BUFFER_LENGTH 40    //length of TX, RX and command buffers
#define USART_ECHO
#define USART_BAUD_RATE 500000

/* PC Command Definitions */
#define PC_CMD_ID        "id  "
#define PC_CMD_ECHO      "echo"
#define PC_CMD_RESET     "rset"
#define PC_CMD_DVS_FWD   "fdvs"
#define PC_CMD_DVS_RESET "rdvs"
#define PC_CMD_DVS_USE   "udvs"
#define PC_CMD_SPN_FWD   "fspn"
#define PC_CMD_SPN_RESET "rspn"
#define PC_CMD_SPN_MODE  "mspn"


#define PC_RESP_OK        "000 Success\r"
#define PC_RESP_BAD_CMD   "001 Not recognised\r"
#define PC_RESP_BAD_LEN   "002 Wrong length\r"
#define PC_RESP_BAD_PARAM "003 Bad parameter\r"

#define PC_IDENTIFIER "Interface"


/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
static xQueueHandle pc_txq, pc_rxq;


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
void pc_config(void)
{
    hal_init();
    irq_init();
    tasks_init();

}

void pc_send_byte(uint8_t data)
{
    xQueueSend(pc_txq, &data, portMAX_DELAY);
}

void pc_send_string(char * str)
{
    while(* str) {
        pc_send_byte(*str);
        str++;
    }
}

void USART2_IRQHandler(void)
{
    uint8_t data;
    static portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(USART2, USART_IT_RXNE)==SET) {
        data = USART_ReceiveData(USART2);
        xQueueSendFromISR(pc_rxq, &data, &xHigherPriorityTaskWoken);
        USART_ClearITPendingBit(USART2, USART_IT_RXNE);
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
  
    //GPIO init: USART2 PA2 as OUT, PA3 as IN
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_GPIOA, ENABLE );
    
    port_init.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    port_init.GPIO_Mode = GPIO_Mode_AF;
    port_init.GPIO_Speed = GPIO_Speed_50MHz;
    port_init.GPIO_OType = GPIO_OType_PP;
    port_init.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init( GPIOA, &port_init );
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource2, GPIO_AF_1);
    GPIO_PinAFConfig(GPIOA,  GPIO_PinSource3, GPIO_AF_1);


    //USART init: USART1 500K 8n1
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    usart_init.USART_BaudRate = USART_BAUD_RATE;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, (USART_InitTypeDef*) &usart_init);
    USART_Cmd(USART2, ENABLE);

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

    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPriority = 3;
    nvic.NVIC_IRQChannelCmd = ENABLE;

    NVIC_Init(&nvic);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
}

/**
 * DESCRIPTION
 * Initialise tasks and start running
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void tasks_init(void)
{
    pc_txq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));
    pc_rxq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));

    xTaskCreate(usart_tx_task, (char const *)"PC_Tx", configMINIMAL_STACK_SIZE,
                (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(usart_rx_task, (char const *)"PC_Rx", configMINIMAL_STACK_SIZE,
                (void *)NULL, tskIDLE_PRIORITY + 1, NULL);
}

/**
 * DESCRIPTION
 * Task to transmit queued data
 * 
 * INPUTS
 * pvParameters (void*) : FreeRTOS struct with task information
 *
 * RETURNS
 * Nothing
 */
static void usart_tx_task(void *pvParameters)
{
    uint8_t data;
    for (;;) {
        
        if (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) {
            vTaskDelay(1);
        } else {
            if (pdPASS == xQueueReceive(pc_txq, &data, portMAX_DELAY)) {
                USART_SendData(USART2, data);
            }
        }
    }
}

/**
 * DESCRIPTION
 * Task to retrieve queued characters and act upon commands
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
    char cmd_buf[5];
    dvs_data_t dvs_data;

    for (;;) {
        if (pdPASS == xQueueReceive(pc_rxq, &data_buf[i++], portMAX_DELAY)) {

#ifdef USART_ECHO
            pc_send_byte(data_buf[i-1]);
#endif

            if (data_buf[i-1] == '\r')
            {
                /* Copy out the command characters */
                memcpy(cmd_buf, data_buf, 4);
                cmd_buf[4] = 0;

                /* Switch based on the command */
                if (strcmp(cmd_buf, PC_CMD_ID) == 0)
                {
                    pc_send_string(PC_RESP_OK);
                    pc_send_string(PC_IDENTIFIER);
                    pc_send_string(PC_EOL);
                }
                else if (strcmp(cmd_buf, PC_CMD_ECHO) == 0)
                {
                    /* Check if the number of bytes is correct */
                    uint8_t expected = data_buf[4];
                    if (expected + 6 == i)
                    {
                        pc_send_string(PC_RESP_OK);
                        data_buf[expected + 6] = 0;
                        pc_send_string(&data_buf[5]);
                        /* \r included as part of echo command */
                    }
                    else if (expected + 6 > i)
                    {
                        /* Continue to avoid buffer being cleared */
                        continue;
                    }
                    else
                    {
                        pc_send_string(PC_RESP_BAD_LEN);
                    }
                }
                else if (strcmp(cmd_buf, PC_CMD_RESET) == 0)
                {
                    /* No point writing success here as board reset will clear
                       UART buffer before it can send */
                    NVIC_SystemReset();
                }
                else if (strcmp(cmd_buf, PC_CMD_DVS_FWD) == 0)
                {
                    /* 7 bytes is 4 command, 2 data, 1 \r */
                    if (i == 7)
                    {
                        pc_send_string(PC_RESP_OK);
                        /* Find out time to forward DVS for */
                        uint16_t fwd_time = data_buf[4] << 8;
                        fwd_time += data_buf[5];
                        /* Enable forwarding - 0 for permanent, else time in 
                           ms */
                        dvs_forward_pc(true, fwd_time);
                    }
                    else if (i > 7)
                    {
                        pc_send_string(PC_RESP_BAD_LEN);
                    }
                    else
                    {
                        /* Continue to avoid buffer being cleared */
                        continue;
                    }
                }
                else if (strcmp(cmd_buf, PC_CMD_DVS_RESET) == 0)
                {
                    pc_send_string(PC_RESP_OK);
                    /* Reset the forwarding of DVS packets */
                    dvs_forward_pc(false, 0);
                }
                else if (strcmp(cmd_buf, PC_CMD_DVS_USE) == 0)
                {
                    /* Retrieve next 3 bytes and use as DVS packet */
                    /* 8 bytes is 4 command, 3 data, 1 \r */
                    if (i == 8)
                    {
                        pc_send_string(PC_RESP_OK);

                        /* Unpack received data into dvs_data */
                        dvs_data.x = data_buf[4];
                        dvs_data.y = data_buf[5];
                        dvs_data.polarity = data_buf[6];

                        /* Use as DVS packet */
                        dvs_put_sim(dvs_data);
                    }
                    else if (i > 8)
                    {
                        pc_send_string(PC_RESP_BAD_LEN);
                    }
                    else
                    {
                        /* Continue to avoid buffer being cleared */
                        continue;
                    }
                }
                else if (strcmp(cmd_buf, PC_CMD_SPN_FWD) == 0)
                {
                    /* Request forwarding of SpiNNaker packets */
                    /* 7 bytes is 4 command, 2 data, 1 \r */
                    if (i == 7)
                    {
                        pc_send_string(PC_RESP_OK);
                        /* Find out time to forward SpiNN for */
                        uint16_t fwd_time = data_buf[4] << 8;
                        fwd_time += data_buf[5];
                        /* Enable forwarding - 0 for permanent, else time in 
                           ms */
                        spinn_forward_pc(true, fwd_time);
                    }
                    else if (i > 7)
                    {
                        pc_send_string(PC_RESP_BAD_LEN);
                    }
                    else
                    {
                        /* Continue to avoid buffer being cleared */
                        continue;
                    }
                }
                else if (strcmp(cmd_buf, PC_CMD_SPN_RESET) == 0)
                {
                    pc_send_string(PC_RESP_OK);
                    /* Reset the forwarding of SpiNN packets */
                    spinn_forward_pc(false, 0);
                }
                else if (strcmp(cmd_buf, PC_CMD_SPN_MODE) == 0)
                {
                    /* Retrieve requested mode and set in spinn_channel.c */
                    /* 6 bytes is 4 command, 1 data, 1 \r */
                    if (i == 6)
                    {
                        uint8_t req_mode = data_buf[4];
                        if (req_mode < SPIN_NUM_MODES)
                        {
                            pc_send_string(PC_RESP_OK);
                            dvs_set_mode((dvs_res_t) req_mode);
                        }
                        else
                        {
                            pc_send_string(PC_RESP_BAD_PARAM);
                        }
                    }
                    else if (i > 6)
                    {
                        pc_send_string(PC_RESP_BAD_LEN);
                    }
                    else
                    {
                        /* Continue to avoid buffer being cleared */
                        continue;
                    }
                }
                else
                {
                    /* If command is not recognised, say so */
                    pc_send_string(PC_RESP_BAD_CMD);
                }

                /* Having consumed a command, reset buffer */
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