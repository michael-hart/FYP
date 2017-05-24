/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "string.h"
#include <stdbool.h>

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "dvs_usart.h"
#include "pc_usart.h"
#include "spinn_channel.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
#define USART_GPIO GPIOA
#define BUFFER_LENGTH 8    //length of TX, RX and command buffers
#define USART_BAUD_RATE 4000000
#define DATA_LENGTH 3

#define RESET_TIMER_NAME "rst_dvs"

/* Definitions to assist in downscaling resolution */
#define DVS_TOTAL_PIXELS   (16384) /* 128 x 128 pixels */
/* 2 bits per pixel */
#define DVS_WIDTH_128       (1) /* 1x1 pixel blocks */
#define DVS_WIDTH_64        (2) /* 2x2 pixel blocks */
#define DVS_WIDTH_32        (4) /* 4x4 pixel blocks */
#define DVS_WIDTH_16        (8) /* 8x8 pixel blocks */

#define DVS_PIXELS_PER_BYTE (4)
#define DVS_EVENT_BUF_SIZE  (DVS_TOTAL_PIXELS / DVS_PIXELS_PER_BYTE )
/* Each y value is multiplied by 32 to get array index */
#define DVS_BYTES_PER_ROW   (32)

#define DVS_BUFFER_LENGTH   (500)

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
typedef struct dvs_buf_s {
    bool filled;
    dvs_data_t data;
} dvs_buf_t;

/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
static xQueueHandle dvs_rxq;
static xQueueHandle dvs_dataq;
static xSemaphoreHandle xFwdSemaphore = NULL;
static TimerHandle_t reset_timer = NULL;
static uint8_t forward_pc_flag = false;

/* Current resolution mode */
static dvs_res_t dvs_res;

/* Static array to act as storage for DVS events */
static dvs_buf_t dvs_events[DVS_BUFFER_LENGTH];
static int dvs_max_idx = -1;

/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void hal_init(void);
static void irq_init(void);
static void tasks_init(void);

static void usart_rx_task(void *pvParameters);
static void decoded_tx_task(void *pvParameters);

static void reset_fwd_flag(TimerHandle_t timer);

static bool update_events(dvs_data_t* p_in_data, dvs_data_t* p_out_data);

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void dvs_config(void)
{
    hal_init();
    irq_init();
    tasks_init();
}

void dvs_forward_pc(uint8_t forward, uint16_t timeout_ms)
{
    if (forward == true)
    {
        /* Set forwarding to true */
        if (xSemaphoreTake(xFwdSemaphore, portMAX_DELAY) == pdTRUE)
        {
            forward_pc_flag = true;
            if (timeout_ms > 0)
            {
                /* Set up a timeout to cancel the forwarding */
                xTimerChangePeriod(reset_timer, timeout_ms, portMAX_DELAY);

            }
            xSemaphoreGive(xFwdSemaphore);
        }
    }
    else
    {
        /* Cancel reset timer and reset flag */
        if (xTimerIsTimerActive(reset_timer))
        {
            xTimerStop(reset_timer, portMAX_DELAY);
        }
        reset_fwd_flag(NULL);
    }
}

void dvs_put_sim(dvs_data_t data)
{
    uint8_t first, second;
    first = 0x80 + data.y;
    second = ((data.polarity & 0x1) << 7) + data.x;
    xQueueSendToBack(dvs_rxq, &first, portMAX_DELAY);
    xQueueSendToBack(dvs_rxq, &second, portMAX_DELAY);
}

void dvs_set_mode(dvs_res_t res)
{
    dvs_res = res;
    /* Clear array to start new mode of operation */
    memset(dvs_events, 0, DVS_BUFFER_LENGTH * sizeof(dvs_buf_t));
    spinn_set_mode(res);
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
    xFwdSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xFwdSemaphore);
    dvs_rxq = xQueueCreate(BUFFER_LENGTH, sizeof(uint8_t));
    dvs_dataq = xQueueCreate(DATA_LENGTH, sizeof(dvs_data_t));
    xTaskCreate(usart_rx_task, (char const *)"DVS_Rx", 
                configMINIMAL_STACK_SIZE, (void *)NULL, 
                tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(decoded_tx_task, (char const *)"DVS_Tx", 
                configMINIMAL_STACK_SIZE + 40, (void *)NULL, 
                tskIDLE_PRIORITY + 1, NULL);
    reset_timer = xTimerCreate(RESET_TIMER_NAME,  /* timer name */
                               100,               /* timer period */
                               pdFALSE,           /* auto-reload */
                               (void*) 0,         /* no id specified */
                               reset_fwd_flag);   /* callback function */
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
    dvs_data_t tmp_data;

    for (;;) {
        if (pdPASS == xQueueReceive(dvs_rxq, &data_buf[i++], portMAX_DELAY)) {

            if (i >= 2)
            {
                /* Copy out the data bytes */
                memcpy(coord_buf, data_buf, 2);

                /* Check that the byte pair is valid by checking for 0 in first byte */
                if ((coord_buf[0] & 0x80) == 0)
                {
                    /* Copy byte in data buffer and decrement i */
                    data_buf[0] = data_buf[1];
                    i--;
                }
                else
                {
                    tmp_data.x = coord_buf[1] & 0x7F;
                    tmp_data.y = coord_buf[0] & 0x7F;
                    tmp_data.polarity = (coord_buf[1] & 0x80) > 0 ? 1 : 0;
                    i = 0;

                    /* Place new struct into queue */
                    xQueueSendToBack(dvs_dataq, &tmp_data, portMAX_DELAY);
                }
            }

            /* If buffer is overflowing, clear it */
            if (i == BUFFER_LENGTH)
            {
                i = 0;
            }

        }
    }
}


/**
 * DESCRIPTION
 * Task to wait for decoded struct and send to SpiNN and possibly PC
 * 
 * INPUTS
 * pvParameters (void*) : FreeRTOS struct with task information
 *
 * RETURNS
 * Nothing
 */
static void decoded_tx_task(void *pvParameters)
{
    dvs_data_t data;

    for (;;)
    {
        /* Wait on queue */
       if (pdPASS == xQueueReceive(dvs_dataq, &data, portMAX_DELAY)) {

            /* Update stored events and only submit event if required */
            /* Note that by passing in same struct, less copying is required */
            if (update_events(&data, &data) == true)
            {
                /* Send decoded data to SpiNNaker */
                spinn_send_dvs(&data);

                if (xSemaphoreTake(xFwdSemaphore, portMAX_DELAY) == pdTRUE)
                {
                    if (forward_pc_flag)
                    {
                        pc_send_byte(data.x);
                        pc_send_byte(data.y);
                        pc_send_byte(data.polarity);
                        pc_send_string(PC_EOL);
                    }
                    xSemaphoreGive(xFwdSemaphore);
                }
            }
       }
    }
}


/**
 * DESCRIPTION
 * Performs safe reset of forwarding flag
 * 
 * INPUTS
 * timer (TimerHandle_t) : Expired timer or NULL
 *
 * RETURNS
 * Nothing
 */
static void reset_fwd_flag(TimerHandle_t timer)
{
    if (xSemaphoreTake(xFwdSemaphore, portMAX_DELAY) == pdTRUE)
    {
        forward_pc_flag = false;
        xSemaphoreGive(xFwdSemaphore);
    }
}

/**
 * DESCRIPTION
 * Updates static block of memory with DVS events and returns whether
 * new event must be sent to SpiNNaker
 * 
 * INPUTS
 * p_in_data (dvs_data_t*) : Input data structure with data from DVS
 * p_out_data (dvs_data_t*) : Output data structure with data to be sent over
 *                            link, or no change if returning false
 *
 * RETURNS
 * true if new event must be sent
 * false otherwise
 */
static bool update_events(dvs_data_t* p_in_data, dvs_data_t* p_out_data)
{

    uint16_t pos_count = 0, neg_count = 0;
    uint8_t width;
    bool event_detected = false;
    int free_idx = -1;
    int min_x, min_y, max_x, max_y;
    /* Max indices per block is size of largest block */
    int16_t relevant[DVS_WIDTH_16*DVS_WIDTH_16];
    uint8_t relevant_idx = 0;
    dvs_data_t* p_current_event;

    /* Copy data and return true immediately if at full resolution */
    if (dvs_res == DVS_RES_128)
    {
        if (p_out_data != p_in_data)
        {
            memcpy(p_out_data, p_in_data, sizeof(dvs_data_t));
        }
        return true;
    }

    /* Log event in the buffer in the first free slot */
    for (int i = 0; i < DVS_BUFFER_LENGTH; i++)
    {
        /* Check if slot is filled */
        if (dvs_events[i].filled == false)
        {
            free_idx = i;
            break;
        }
    }

    /* Check free_idx is valid - still -1 if no free elements */
    if (free_idx < 0)
    {
        /* Dump event by immediately returning false */
        return false;
    }

    /* Copy data in */
    memcpy(&dvs_events[free_idx].data, p_in_data, sizeof(dvs_data_t));
    dvs_events[free_idx].filled = true;

    /* Update max filled slot if necessary */
    if (free_idx > dvs_max_idx)
    {
        dvs_max_idx = free_idx;
    }

    /* Work out the block size from the current mode */
    switch (dvs_res)
    {
        case DVS_RES_64:
            width = DVS_WIDTH_64;
            break;
        case DVS_RES_32:
            width = DVS_WIDTH_32;
            break;
        case DVS_RES_16:
            width = DVS_WIDTH_16;
            break;
        default:
            /* Unheard of mode, so return false */
            return false;
    }

    /* Work out the min, max x, y */
    min_x = p_in_data->x - (p_in_data->x % width);
    min_y = p_in_data->y - (p_in_data->y % width);
    max_x = min_x + width - 1;
    max_y = min_y + width - 1;

    /* Iterate over events inside buffer and store relevant indices */
    memset(relevant, -1, DVS_WIDTH_16*DVS_WIDTH_16);
    for (int i = 0; i <= dvs_max_idx; i++)
    {
        if (dvs_events[i].filled == true)
        {
            p_current_event = &dvs_events[i].data;
            if (p_current_event->x <= max_x && p_current_event->x >= min_x &&
                p_current_event->y <= max_y && p_current_event->y >= min_y)
            {
                /* Save that pixel is relevant to current operation */
                relevant[relevant_idx++] = i;

                if (p_current_event->polarity == 1)
                {
                    pos_count++;
                }
                else
                {
                    neg_count++;
                }
            }
        }
    }

    /* Check to see if a new event must be sent */

    /* Compare the counts and decide if event must be sent */
    if ((pos_count + neg_count) >= ((width*width) >> 1))
    {
        if (pos_count > neg_count)
        {
            event_detected = true;
            p_out_data->polarity = 1;
        }
        else if (neg_count > pos_count)
        {
            event_detected = true;
            p_out_data->polarity = 0;
        }
        /* Otherwise, they are equal, and output is zero */
    }

    /* If event is detected, delete relevant entries from buffer */
    if (event_detected)
    {
        /* Delete data in buffer */
        for (int16_t idx = 0; idx < relevant_idx; idx++)
        {
            dvs_events[relevant[idx]].filled = false;
        }

        /* Update maximum index */
        for (int16_t idx = dvs_max_idx; idx >= 0; idx--)
        {
            if (dvs_events[idx].filled == true)
            {
                dvs_max_idx = idx;
                break;
            }
        }

        /* If event is to be submitted, fill in the struct with information */
        p_out_data->x = min_x;
        p_out_data->y = min_y;
        /* Polarity set during mode check */
    }

    return event_detected;
}

/*******************************************************************************
 * End of file
 ******************************************************************************/