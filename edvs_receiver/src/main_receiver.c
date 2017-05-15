/*******************************************************************************
 * Global Includes
 ******************************************************************************/
/* FreeRTOS */
#include "FreeRTOS.h"
#include "timers.h"

/* Hardware */
#include "stm320518_eval.h"

/* C Standard Libraries */
#include <stdbool.h>


/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "main_receiver.h"
#include "usart.h"

/*******************************************************************************
 * Local Definitions
 ******************************************************************************/
/* LED Timer Definitions */
#define LED_TIMER_NAME "LEDTimer"
#define LED_TIMER_PRIORITY (tskIDLE_PRIORITY)
#define LED_TIMER_PERIOD (1000u / portTICK_PERIOD_MS)
//#define LED_TIMER_PORT LED3_GPIO_PORT
//#define LED_TIMER_PIN LED3_PIN
#define LED_TIMER_PORT LED1_GPIO_PORT
#define LED_TIMER_PIN LED1_PIN
#define LED_TIMER_LED LED1

/*******************************************************************************
 * Local Type and Enum definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Private Function Declarations (static)
 ******************************************************************************/
static void led_timer_callback(TimerHandle_t timer);
static void error_loop(void);

static bool timer_on = 0;
TimerHandle_t led_timer = NULL;

/*******************************************************************************
 * Public Function Definitions 
 ******************************************************************************/
void main_receiver(void)
{
//    STM_EVAL_LEDInit(LED_TIMER_LED);
    // STM_EVAL_LEDInit(LED1);
    // STM_EVAL_LEDInit(LED2);
    // STM_EVAL_LEDInit(LED3);
    // STM_EVAL_LEDInit(LED4);
//    STM_EVAL_LEDOn(LED_TIMER_LED);
    // STM_EVAL_LEDOn(LED1);
    // STM_EVAL_LEDOn(LED2);
    // STM_EVAL_LEDOn(LED3);
    // STM_EVAL_LEDOn(LED4);
    
    /* Create timer */
    TimerHandle_t led_timer = NULL;
    led_timer = xTimerCreate(LED_TIMER_NAME,      /* timer name */
                             LED_TIMER_PERIOD,    /* timer period */
                             pdTRUE,              /* auto-reload */
                             (void*) 0,           /* no id specified */
                             led_timer_callback); /* callback function */

    USART_Config();
    
    /* Check timer created successfully */
    if (led_timer == NULL)
    {
        error_loop();
    }

    /* Start timer running in non-blocking mode */
    xTimerStart(led_timer, 0L);

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
 * Callback function for LED timer to toggle LED
 * 
 * INPUTS
 * timer (TimerHandle_t) : FreeRTOS Timer Handle that hit 
 *
 * RETURNS
 * Nothing
 */
static void led_timer_callback(TimerHandle_t timer)
{
    timer_on = !timer_on;
    //STM_EVAL_LEDToggle(LED_TIMER_LED);
    USART_SendByte(0x14);
//    if (timer_on)
//    {
//        STM_EVAL_LEDOn(LED_TIMER_LED);
//    }
//    else
//    {
//        STM_EVAL_LEDOff(LED_TIMER_LED);
//    }
//    GPIO_WriteBit(LED_TIMER_PORT, LED_TIMER_PIN, 
//                  timer_on ? Bit_SET : Bit_RESET);
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