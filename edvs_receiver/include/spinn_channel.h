
#ifndef _SPINN_CHANNEL_H
#define _SPINN_CHANNEL_H

/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "FreeRTOS.h"
#include "semphr.h"

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
#include "dvs_usart.h"

/*******************************************************************************
 * Global Processor Definitions
 ******************************************************************************/
#define SPIN_NUM_MODES (4)

/*******************************************************************************
 * Enum and Type definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * External Variable Definitions
 ******************************************************************************/
extern xSemaphoreHandle xSpinnTxSemaphore;
extern xSemaphoreHandle xSpinnRxSemaphore;

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
 * p_data (dvs_data_t*) : Pointer to struct containing data to be sent
 *
 * RETURNS
 * Nothing
 */
void spinn_send_dvs(dvs_data_t* p_data);

/**
 * DESCRIPTION
 * Sets SpiNNaker forwarding mode resolution
 * 
 * INPUTS
 * res (dvs_res_t) : Enum value to convert
 *
 * RETURNS
 * Nothing
 */
void spinn_set_mode(dvs_res_t res);

/**
 * DESCRIPTION
 * Request forwarding of received data from PC
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
void spinn_forward_rx_pc(uint8_t forward, uint16_t timeout_ms);

/**
 * DESCRIPTION
 * Given a pointer to a buffer containing a SpiNNaker packet, either forwards
 * data over UART or sets motor PWM signal period
 * 
 * INPUTS
 * buf (uint8_t*) : Buffer containing SpiNNaker packet data
 *
 * RETURNS
 * Nothing
 */
void spinn_use_data(uint8_t *buf);


#endif /* _SPINN_CHANNEL_H */

/*******************************************************************************
 * End of File
 ******************************************************************************/
