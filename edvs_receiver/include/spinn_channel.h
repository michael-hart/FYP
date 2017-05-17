
#ifndef _SPINN_CHANNEL_H
#define _SPINN_CHANNEL_H

/*******************************************************************************
 * Global Includes
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "dvs_usart.h"

/*******************************************************************************
 * Global Processor Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Enum and Type definitions
 ******************************************************************************/
typedef enum spin_mode_e {
    SPIN_MODE_128 = 0,
    SPIN_MODE_64 = 1,
    SPIN_MODE_32 = 2,
    SPIN_MODE_16 = 3,
} spin_mode_t;

/*******************************************************************************
 * External Variable Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Public Function Declarations
 ******************************************************************************/

/**
 * DESCRIPTION
 * Configure hardware and setup tasks for SpiNNaker link
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
void spinn_config(void);

/**
 * DESCRIPTION
 * Request forwarding of SpiNNaker packets to PC; replaces SpiNNaker GPIO link
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
void spinn_forward_pc(uint8_t forward, uint16_t timeout_ms);

/**
 * DESCRIPTION
 * Queues DVS packet to be sent to the SpiNNaker
 * 
 * INPUTS
 * data (dvs_data_t) : Struct containing data to be sent
 *
 * RETURNS
 * Nothing
 */
void spinn_send_dvs(dvs_data_t data);

/* TODO: spinn_encode, spinn_decode functions */

#endif /* _SPINN_CHANNEL_H */

/*******************************************************************************
 * End of File
 ******************************************************************************/