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
#define BUFFER_LENGTH    (20)
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
/* Semaphore for waiting until a symbol has been received */
xSemaphoreHandle xSpinnRxSemaphore = NULL;


/*******************************************************************************
 * Local Variable Declarations
 ******************************************************************************/
/* Queue for channel packets */
static xQueueHandle spinn_txq;

/* Flag/semaphore for PC forwarding */
static xSemaphoreHandle spinFwdSemaphore = NULL;
static uint8_t spinn_fwd_pc_flag = false;

static xSemaphoreHandle spinFwdRxSemaphore = NULL;
static uint8_t spinn_fwd_rx_pc_flag = false;

/* Timer for resetting forwarding after timeout */
static TimerHandle_t spinn_reset_timer = NULL;
static TimerHandle_t spinn_rx_reset_timer = NULL;

/* Symbol encoding table for 2-of-7 format */
static uint8_t symbol_table[] = 
{
    0x11, 0x12, 0x14, 0x18, 0x21, 0x22, 0x24, 0x28,
    0x41, 0x42, 0x44, 0x48, 0x03, 0x06, 0x0C, 0x09, 0x60
};

/* Current mode of sending data */
static dvs_res_t dvs_res = DVS_RES_128;

/* Virtual chip address symbols to queue */
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
static void spinn_rx_task(void *pvParameters);

static void spinn_reset_fwd_rx_flag(TimerHandle_t timer);
static uint8_t spinn_lookup_sym(uint8_t sym);

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
    uint8_t pkt_buf[SPINN_SHORT_SYMS];
    uint8_t* pkt_buf_p = pkt_buf;

    /* Queue each item in order of sending */
    uint8_t odd_parity = 0;
    uint8_t xor_all = 0;
    uint8_t tmp_byte = 0;
    uint16_t mapped_event;

    /* If queue is not empty, delete earliest entry */
    if (uxQueueSpacesAvailable(spinn_txq) == 0)
    {
        xQueueReceive(spinn_txq, pkt_buf, portMAX_DELAY);
    }

    mapped_event = 0x8000 + ((p_data->polarity & 0x1) << 14);

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

    /* Add header to buffer */
    *pkt_buf_p = symbol_table[odd_parity];
    pkt_buf_p++;
    *pkt_buf_p = symbol_table[0];
    pkt_buf_p++;

    /* Add data to buffer */
    tmp_byte =  (mapped_event & 0x000F);
    *pkt_buf_p = symbol_table[tmp_byte];
    pkt_buf_p++;
    tmp_byte = ((mapped_event & 0x00F0) >> 4);
    *pkt_buf_p = symbol_table[tmp_byte];
    pkt_buf_p++;
    tmp_byte = ((mapped_event & 0x0F00) >> 8);
    *pkt_buf_p = symbol_table[tmp_byte];
    pkt_buf_p++;
    tmp_byte = ((mapped_event & 0xF000) >> 12);
    *pkt_buf_p = symbol_table[tmp_byte];
    pkt_buf_p++;

    /* Fill the remaining buffer space with chip address and EOP */
    for (int8_t i = 3; i >= 0; i--)
    {
        *pkt_buf_p = virtual_chip_symbols[i];
        pkt_buf_p++;
    }

    *pkt_buf_p = symbol_table[EOP_IDX];
    pkt_buf_p++;

    /* Add entire buffer to queue as a packet */
    xQueueSendToBack(spinn_txq, pkt_buf, portMAX_DELAY);

}

void spinn_set_mode(dvs_res_t mode)
{
    dvs_res = mode;
}

void spinn_forward_rx_pc(uint8_t forward, uint16_t timeout_ms)
{
    if (forward == true)
    {
        /* Set forwarding to true */
        if (xSemaphoreTake(spinFwdRxSemaphore, portMAX_DELAY) == pdTRUE)
        {
            spinn_fwd_rx_pc_flag = true;
            if (timeout_ms > 0)
            {
                /* Set up a timeout to cancel the forwarding */
                xTimerChangePeriod(spinn_rx_reset_timer, timeout_ms, 
                                   portMAX_DELAY);

            }
            xSemaphoreGive(spinFwdRxSemaphore);
        }
    }
    else
    {
        /* Cancel reset timer and reset flag */
        if (xTimerIsTimerActive(spinn_rx_reset_timer))
        {
            xTimerStop(spinn_rx_reset_timer, portMAX_DELAY);
        }
        spinn_reset_fwd_rx_flag(NULL);
    }
}

void spinn_use_data(uint8_t *buf)
{
    uint16_t speed = 0;
    uint8_t speed_syms[4];

    /* Decode buffer and get motor data */
    speed_syms[0] = spinn_lookup_sym(buf[2]);
    speed_syms[1] = spinn_lookup_sym(buf[3]);
    speed_syms[2] = spinn_lookup_sym(buf[4]);
    speed_syms[3] = spinn_lookup_sym(buf[5]);
    if (speed_syms[0] >= sizeof(symbol_table) || 
        speed_syms[1] >= sizeof(symbol_table) || 
        speed_syms[2] >= sizeof(symbol_table) || 
        speed_syms[3] >= sizeof(symbol_table))
    {
        /* Received a symbol in error; return early */
        return;
    }
    speed = speed_syms[0] +
            (speed_syms[1] << 4) +
            (speed_syms[2] << 8) +
            (speed_syms[3] << 12);

    /* Thread-safe check of whether to forward */
    if (pdTRUE == xSemaphoreTake(spinFwdRxSemaphore, portMAX_DELAY))
    {
        /* If forwarding, send to PC */
        if (spinn_fwd_rx_pc_flag)
        {
            pc_send_byte((speed & 0xFF00) >> 8);
            pc_send_byte(speed & 0x00FF);
            pc_send_byte('\r');
        }
        else
        {
            /* Currently no motor PWM signal set up, so ignore */
        }
        xSemaphoreGive(spinFwdRxSemaphore);
    }
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
                         GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_15;
    port_init.GPIO_Mode = GPIO_Mode_OUT; /* Output Mode */
    port_init.GPIO_Speed = GPIO_Speed_50MHz; /* Highest speed pins */
    port_init.GPIO_OType = GPIO_OType_PP;   /* Output type push pull */
    port_init.GPIO_PuPd = GPIO_PuPd_UP;     /* Pull up */
    GPIO_Init( GPIOB, &port_init );

    /* Set up B7 as an input */
    port_init.GPIO_Pin = GPIO_Pin_7  | GPIO_Pin_8  | GPIO_Pin_9  | GPIO_Pin_10 |
                         GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14;
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

    /* Map all pins into interrupt */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource7);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource8);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource9);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource10);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource11);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource12);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource13);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource14);

    exti.EXTI_Line = EXTI_Line7  | EXTI_Line8  | EXTI_Line9  | EXTI_Line10 | 
                     EXTI_Line11 | EXTI_Line12 | EXTI_Line13 | EXTI_Line14;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Signals are transitions, so rising or falling edge */
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
    spinn_rx_reset_timer = xTimerCreate("spn_rx_timer",  /* timer name */
                                     100,               /* timer period */
                                     pdFALSE,           /* auto-reload */
                                     (void*) 0,         /* no id specified */
                                     spinn_reset_fwd_rx_flag);   /* callback function */

    /* Create semaphore for forwarding */
    spinFwdSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(spinFwdSemaphore);
    spinFwdRxSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(spinFwdRxSemaphore);

    /* Create semaphore for transmission and give to transmit first symbol */
    xSpinnTxSemaphore = xSemaphoreCreateBinary();
    /* First give so that we wait on the queue, not the semaphore, as the
       semaphore will only be given once the first symbol is acknowledged */
    xSemaphoreGive(xSpinnTxSemaphore);

    /* Create semaphore to count transitions on the GPIO */
    xSpinnRxSemaphore = xSemaphoreCreateCounting(8, 0);

    /* Create task for acting upon queued data */
    xTaskCreate(spinn_tx_task, (char const *)"txSpn", configMINIMAL_STACK_SIZE, 
                (void *)NULL, tskIDLE_PRIORITY + SPINN_TX_PRIORITY, NULL);

    /* Create a task to listen for new SpiNNaker data */
    xTaskCreate(spinn_rx_task, (char const *)"rxSpn", configMINIMAL_STACK_SIZE, 
                (void *)NULL, tskIDLE_PRIORITY, NULL);

    /* Create queue for SpiNN packet data */
    spinn_txq = xQueueCreate(BUFFER_LENGTH, SPINN_SHORT_SYMS * sizeof(uint8_t));

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
    uint8_t pkt_buf[SPINN_SHORT_SYMS];
    uint8_t idx = 0;

    for (;;)
    {

        /* Wait for data in the queue to transmit */
        idx = 0;
        if (pdPASS == xQueueReceive(spinn_txq, pkt_buf, portMAX_DELAY))
        {
            while (idx < SPINN_SHORT_SYMS)
            {
                data = pkt_buf[idx++];
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
                        GPIO_Write(GPIOB, 
                            (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_15) << 14)
                             || data ^ prev_data);
                        prev_data = data ^ prev_data;
                    }
                }
            }
        }
    }
}

/**
 * DESCRIPTION
 * Task to wait for entire packet, then handle result somehow
 * 
 * INPUTS
 * pvParameters (void*) : FreeRTOS struct with task information
 *
 * RETURNS
 * Nothing
 */
static void spinn_rx_task(void *pvParameters)
{
    uint8_t rx_buf[SPINN_SHORT_SYMS*2];
    uint8_t rx_buf_idx = 0;
    uint8_t previous_data = 0, current_data = 0;

    /* Reset previous data to ensure state is correct */
    previous_data =  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8 ) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9 ) << 1) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) << 2) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) << 3) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) << 4) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) << 5) +
                    (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) << 6);

    for (;;)
    {

        while (rx_buf_idx < sizeof(rx_buf))
        {
            /* Get semaphore twice */
            xSemaphoreTake(xSpinnRxSemaphore, portMAX_DELAY);
            xSemaphoreTake(xSpinnRxSemaphore, portMAX_DELAY);
            /* Read data into buffer */
            current_data =  GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8 ) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9 ) << 1) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10) << 2) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11) << 3) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_12) << 4) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) << 5) +
                           (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_14) << 6);
            rx_buf[rx_buf_idx] = current_data ^ previous_data;
            previous_data = current_data;
            /* Transmit acknowledge as transition */
            if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_15) > 0)
            {
                GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
            }
            else
            {
                GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
            }
            if (rx_buf[rx_buf_idx] == symbol_table[sizeof(symbol_table)-1])
            {
                break;
            }
            rx_buf_idx++;
        }

        /* Handle data using method */
        spinn_use_data(&rx_buf[0]);

        /* Reset buffer */
        rx_buf_idx = 0;
    }
}

/**
 * DESCRIPTION
 * Performs safe reset of received forwarding flag
 * 
 * INPUTS
 * timer (TimerHandle_t) : Expired timer or NULL
 *
 * RETURNS
 * Nothing
 */
static void spinn_reset_fwd_rx_flag(TimerHandle_t timer)
{
    if (xSemaphoreTake(spinFwdRxSemaphore, portMAX_DELAY) == pdTRUE)
    {
        spinn_fwd_rx_pc_flag = false;
        xSemaphoreGive(spinFwdRxSemaphore);
    }
}

/**
 * DESCRIPTION
 * Returns index of symbol from lookup table
 * 
 * INPUTS
 * sym (uint8_t) : Symbol to look up
 *
 * RETURNS
 * Index of symbol or 17 if symbol not found
 */
static uint8_t spinn_lookup_sym(uint8_t sym)
{
    for (uint8_t idx = 0; idx < sizeof(symbol_table); idx++)
    {
        if (symbol_table[idx] == sym)
        {
            return idx;
        }
    }
    return sizeof(symbol_table);
}

/*******************************************************************************
 * End of file
 ******************************************************************************/