/*******************************************************************************
 * Global Includes
 ******************************************************************************/
/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

#include "stm32f0xx.h"

#include <stdbool.h>

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "spinn_channel.h"
#include "pc_usart.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
#define SPINN_SHORT_SYMS (11)
#define SPINN_LONG_SYMS  (19)
#define BUFFER_LENGTH    (5)
#define EOP_IDX          (16)

#define SPINN_TIMER_NAME "rst_spinn"

#define SPINN_TX_PRIORITY (1)

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Global Variable Declarations
 ******************************************************************************/
/* Semaphore for transmitting next symbol */
xSemaphoreHandle xSpinnTxSemaphore = NULL;


/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
/* Queue for channel packets */
static xQueueHandle spinn_txq;

/* Flag/semaphore for PC forwarding */
static xSemaphoreHandle spinFwdSemaphore = NULL;
static uint8_t spinn_fwd_pc_flag = false;

/* Timer for resetting forwarding after timeout */
static TimerHandle_t spinn_reset_timer = NULL;

/* Symbol encoding table for 2-of-7 format */
static uint8_t symbol_table[] = 
{
    0x11, 0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
    0x41, 0x42, 0x44, 0x48, 0x03, 0x06, 0x0C, 0x09, 0x60
};

/* Current mode of sending data */
static dvs_res_t dvs_res = DVS_RES_128;

/* Virtual chip address symbols to queue */
/* TODO assign actual address instead of guessing it */
static uint8_t virtual_chip_address[] = {0x00, 0x02, 0x00, 0x00};
static uint8_t virtual_chip_symbols[] = {0x11, 0x14, 0x11, 0x11};

/* Previous data byte for XORing due to lack of ToggleBits function */
static uint8_t prev_data = 0x00;

/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void hal_init(void);
static void irq_init(void);
static void tasks_init(void);

static void spinn_reset_fwd_flag(TimerHandle_t timer);

static void spinn_tx_task(void *pvParameters);

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void spinn_config(void)
{
    hal_init();
    irq_init();
    tasks_init();
}

void spinn_forward_pc(uint8_t forward, uint16_t timeout_ms)
{
    if (forward == true)
    {
        /* Set forwarding to true */
        if (xSemaphoreTake(spinFwdSemaphore, portMAX_DELAY) == pdTRUE)
        {
            spinn_fwd_pc_flag = true;
            if (timeout_ms > 0)
            {
                /* Set up a timeout to cancel the forwarding */
                xTimerChangePeriod(spinn_reset_timer, timeout_ms, 
                                   portMAX_DELAY);

            }
            xSemaphoreGive(spinFwdSemaphore);
        }

        /* Give semaphore such that task knows to continue */
        xSemaphoreGive(xSpinnTxSemaphore);
    }
    else
    {
        /* Cancel reset timer and reset flag */
        if (xTimerIsTimerActive(spinn_reset_timer))
        {
            xTimerStop(spinn_reset_timer, portMAX_DELAY);
        }
        spinn_reset_fwd_flag(NULL);
        /* Take semaphore such that task waits for interrupt */
        /* Only wait 100ms in case semaphore has not previously been given */
        xSemaphoreTake(xSpinnTxSemaphore, 100);
    }
}

void spinn_send_dvs(dvs_data_t* p_data)
{
    /* Queue each item in order of sending */
    uint8_t odd_parity = 0;
    uint8_t xor_all = 0;

    uint8_t tmp_byte = 0;
    uint16_t mapped_event = 0x8000 + ((p_data->polarity & 0x1) << 14);

    switch (dvs_res)
    {
        case DVS_RES_64:
            mapped_event += ((p_data->y & 0x7E) << 5);
            mapped_event += ((p_data->x & 0x7E) >> 1);
            break;
        case DVS_RES_32:
            mapped_event += ((p_data->y & 0x7C) << 3);
            mapped_event += ((p_data->x & 0x7C) >> 2);
            break;
        case DVS_RES_16:
            mapped_event += ((p_data->y & 0x78) << 1);
            mapped_event += ((p_data->x & 0x78) >> 3);
            break;
        case DVS_RES_128:
        default:
            mapped_event += ((p_data->y & 0x7F) << 7);
            mapped_event +=  (p_data->x & 0x7F);
            break;
    }

    /* Calculate parity */
    xor_all = virtual_chip_address[3] ^ virtual_chip_address[2] ^ 
              virtual_chip_address[1] ^ virtual_chip_address[0] ^
              ((mapped_event & 0xFF00) >> 8) ^ (mapped_event & 0xFF);
    odd_parity = 1 ^ ((xor_all & 0x80) >> 7) ^ ((xor_all & 0x40) >> 6) ^
                     ((xor_all & 0x20) >> 5) ^ ((xor_all & 0x10) >> 4) ^
                     ((xor_all & 0x08) >> 3) ^ ((xor_all & 0x04) >> 2) ^
                     ((xor_all & 0x02) >> 1) ^ (xor_all & 0x01);

    /* Add header to queue */
    xQueueSendToBack(spinn_txq, &symbol_table[odd_parity], portMAX_DELAY);
    xQueueSendToBack(spinn_txq, &symbol_table[0], portMAX_DELAY);

    /* Add data to queue */
    tmp_byte =  (mapped_event & 0x000F);
    xQueueSendToBack(spinn_txq, &symbol_table[tmp_byte], portMAX_DELAY);
    tmp_byte = ((mapped_event & 0x00F0) >> 4);
    xQueueSendToBack(spinn_txq, &symbol_table[tmp_byte], portMAX_DELAY);
    tmp_byte = ((mapped_event & 0x0F00) >> 8);
    xQueueSendToBack(spinn_txq, &symbol_table[tmp_byte], portMAX_DELAY);
    tmp_byte = ((mapped_event & 0xF000) >> 12);
    xQueueSendToBack(spinn_txq, &symbol_table[tmp_byte], portMAX_DELAY);

    /* Fill the remaining queue space with chip address and EOP */
    for (int8_t i = 3; i >= 0; i--)
    {
        xQueueSendToBack(spinn_txq, &virtual_chip_symbols[i], portMAX_DELAY);
    }
    xQueueSendToBack(spinn_txq, &symbol_table[EOP_IDX], portMAX_DELAY);

}

void spinn_set_mode(dvs_res_t mode)
{
    dvs_res = mode;
}


/*******************************************************************************
 * Private Function Definitions (static)
 ******************************************************************************/


/**
 * DESCRIPTION
 * Configure hardware for SpiNNaker connection
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

    /* Enable clock on GPIO pins */
    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_GPIOB, ENABLE );

    /* Configure pins with given properties */
    port_init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | 
                         GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6;
    port_init.GPIO_Mode = GPIO_Mode_OUT; /* Output Mode */
    port_init.GPIO_Speed = GPIO_Speed_50MHz; /* Highest speed pins */
    port_init.GPIO_OType = GPIO_OType_PP;   /* Output type push pull */
    port_init.GPIO_PuPd = GPIO_PuPd_UP;     /* Pull up */
    GPIO_Init( GPIOB, &port_init );

    /* Set up B7 as an input */
    port_init.GPIO_Pin = GPIO_Pin_7;
    port_init.GPIO_Mode = GPIO_Mode_IN; /* Input mode */
    port_init.GPIO_Speed = GPIO_Speed_50MHz; /* Highest speed */
    /* Other parameters remain the same */
    GPIO_Init(GPIOB, &port_init);

    /* Ensure bits are all reset */
    GPIO_Write(GPIOB, 0);

}

/**
 * DESCRIPTION
 * Configure ISRs for SpiNNaker connection
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void irq_init(void)
{

    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    /* Configure external interrupt */

    /* Ensure peripheral clock is configured */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); 

    /* Map pin 7 into interrupt */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource7);

    exti.EXTI_Line = EXTI_Line7;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Acknowledge signal is transition, so rising or falling edge */
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    /* Configure interrupt register */
    nvic.NVIC_IRQChannel = EXTI4_15_IRQn;
    nvic.NVIC_IRQChannelPriority = 5;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

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
    /* Create timer for resetting to task */
    spinn_reset_timer = xTimerCreate(SPINN_TIMER_NAME,  /* timer name */
                                     100,               /* timer period */
                                     pdFALSE,           /* auto-reload */
                                     (void*) 0,         /* no id specified */
                                     spinn_reset_fwd_flag);   /* callback function */

    /* Create semaphore for forwarding */
    spinFwdSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(spinFwdSemaphore);

    /* Create semaphore for transmission and give to transmit first symbol */
    xSpinnTxSemaphore = xSemaphoreCreateBinary();
    /* First give so that we wait on the queue, not the semaphore, as the
       semaphore will only be given once the first symbol is acknowledged */
    xSemaphoreGive(xSpinnTxSemaphore);

    /* Create task for acting upon queued data */
    xTaskCreate(spinn_tx_task, (char const *)"txSpn", configMINIMAL_STACK_SIZE, 
                (void *)NULL, tskIDLE_PRIORITY + SPINN_TX_PRIORITY, NULL);

    /* Create queue for SpiNN packet data */
    spinn_txq = xQueueCreate(BUFFER_LENGTH * SPINN_SHORT_SYMS, sizeof(uint8_t));

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
static void spinn_reset_fwd_flag(TimerHandle_t timer)
{
    if (xSemaphoreTake(spinFwdSemaphore, portMAX_DELAY) == pdTRUE)
    {
        spinn_fwd_pc_flag = false;
        xSemaphoreGive(spinFwdSemaphore);
    }
}

/**
 * DESCRIPTION
 * Task to wait for interrupt and transmit next symbol
 * 
 * INPUTS
 * pvParameters (void*) : FreeRTOS struct with task information
 *
 * RETURNS
 * Nothing
 */
static void spinn_tx_task(void *pvParameters)
{
    uint8_t data = 0;
    uint8_t check_flag = false;
    uint8_t sent_bytes = 0;

    for (;;)
    {

        /* Wait for data in the queue to transmit */
        if (pdPASS == xQueueReceive(spinn_txq, &data, portMAX_DELAY))
        {
        
            /* Wait for interrupt on pin to transmit next symbol */
            if (xSemaphoreTake(xSpinnTxSemaphore, portMAX_DELAY) == pdTRUE)
            {

                if (xSemaphoreTake(spinFwdSemaphore, portMAX_DELAY) == pdTRUE)
                {
                    /* Copy to prevent holding while doing large task */
                    check_flag = spinn_fwd_pc_flag;
                    xSemaphoreGive(spinFwdSemaphore);
                }

                if (check_flag)
                {
                    pc_send_byte(data);
                    prev_data = data;
                    /* Send carriage return to signify EOP */ 
                    if (++sent_bytes == SPINN_SHORT_SYMS)
                    {
                        pc_send_string(PC_EOL);
                        sent_bytes = 0;
                    }
                    /* If forwarding to PC, do not wait for interrupt */
                    xSemaphoreGive(xSpinnTxSemaphore);
                }
                else
                {
                    /* Toggle bits in port for next transition */
                    GPIO_Write(GPIOB, data ^ prev_data);
                    prev_data = data ^ prev_data;
                }
            }
        }
    }
}

/*******************************************************************************
 * End of file
 ******************************************************************************/