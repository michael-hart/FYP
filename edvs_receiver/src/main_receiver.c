/*******************************************************************************
 * Global Includes
 ******************************************************************************/
/* FreeRTOS */
#include "FreeRTOS.h"
#include "timers.h"

/* Hardware */
#include "stm32f0xx.h"

/* C Standard Libraries */
#include <stdbool.h>


/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "main_receiver.h"
#include "pc_usart.h"
#include "dvs_usart.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
/* IWDG Timer Definitions */
#define IWDG_TIMER_NAME "LEDTimer"
#define IWDG_TIMER_PERIOD (100u / portTICK_PERIOD_MS)

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void iwdg_init(void);
static void iwdg_kick(TimerHandle_t timer);
static void error_loop(void);


/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void main_receiver(void)
{

    /* Create timer for telling watchdog not to reset */
    TimerHandle_t iwdg_kicker = NULL;
    iwdg_kicker = xTimerCreate(IWDG_TIMER_NAME,   /* timer name */
                               IWDG_TIMER_PERIOD, /* timer period */
                               pdTRUE,            /* auto-reload */
                               (void*) 0,         /* no id specified */
                               iwdg_kick);        /* callback function */
 
    /* Check timer created successfully */
    if (iwdg_kicker == NULL)
    {
        error_loop();
    }

    /* Set up USART tasks */
    PC_Config();
    DVS_Config();
    
    /* Set up Watchdog Timer */
    iwdg_init();

    /* Start timer running in non-blocking mode */
    xTimerStart(iwdg_kicker, 0L);

    vTaskStartScheduler();

    /* vTaskStartScheduler blocks further execution, so use infinite loop in
    case of error with WDT */
    error_loop();
}


/*******************************************************************************
 * Private Function Definitions (static)
 ******************************************************************************/

/**
 * DESCRIPTION
 * Static function to set up independent watchdog timer with window disabled
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void iwdg_init(void)
{
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(IWDG_Prescaler_16);
    IWDG_SetReload(2500);
    IWDG_ReloadCounter();
    IWDG_Enable();
}

/**
 * DESCRIPTION
 * Callback function for timer to kick watchdog
 * 
 * INPUTS
 * timer (TimerHandle_t) : FreeRTOS Timer Handle that hit 
 *
 * RETURNS
 * Nothing
 */
static void iwdg_kick(TimerHandle_t timer)
{
    /* Reset IWDG counter to reset value */
    IWDG_ReloadCounter();
}

/**
 * DESCRIPTION
 * Infinite loop in case any errors have occurred
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
static void error_loop(void)
{
    for( ;; );
}

/*******************************************************************************
 * End of file
 ******************************************************************************/