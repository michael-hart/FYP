
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

/* Polarity enum for storing events from DVS */
typedef enum dvs_polarity_e {
    POL_NONE,
    POL_POS,
    POL_NEG,
} dvs_polarity_t;

typedef enum dvs_res_e {
    DVS_RES_128 = 0,
    DVS_RES_64 = 1,
    DVS_RES_32 = 2,
    DVS_RES_16 = 3,
} dvs_res_t;

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

/**
 * DESCRIPTION
 * Sets DVS resolution for downscaling
 * 
 * INPUTS
 * res (dvs_res_t) : Enum value to convert
 *
 * RETURNS
 * Nothing
 */
void dvs_set_mode(dvs_res_t res);


#endif /* _DVS_USART_H */

/*******************************************************************************
 * End of File
 ******************************************************************************/
