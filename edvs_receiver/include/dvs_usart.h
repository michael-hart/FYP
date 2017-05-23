
#ifndef _DVS_USART_H
#define _DVS_USART_H

/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"
   
#include <stdint.h>

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Global Processor Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Enum and Type definitions
 ******************************************************************************/
/* Standard struct for DVS data to fill */   
typedef struct dvs_data_s {
    uint8_t x;
    uint8_t y;
    uint8_t polarity;
} dvs_data_t;

/*******************************************************************************
 * External Variable Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Public Function Declarations
 ******************************************************************************/

/**
 * DESCRIPTION
 * Initial setup for DVS USART comms
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
void dvs_config(void);

/**
 * DESCRIPTION
 * Request forwarding of DVS packets to PC
 * 
 * INPUTS
 * forward (uint8_t) : true or false of whether to forward. Resets any existing
 *                     state
 * timeout_ms (uint16_t) : timeout in ms of how long forward packets, or 0 to
 *                         set state permanently
 *
 * RETURNS
 * Nothing
 */
void dvs_forward_pc(uint8_t forward, uint16_t timeout_ms);

/**
 * DESCRIPTION
 * Place a simulated packet into the queue to be treated as a normal packet
 * 
 * INPUTS
 * data (dvs_data_t) : struct containing simulated data
 *
 * RETURNS
 * Nothing
 */
void dvs_put_sim(dvs_data_t data);


#endif /* _DVS_USART_H */

/*******************************************************************************
 * End of File
 ******************************************************************************/
