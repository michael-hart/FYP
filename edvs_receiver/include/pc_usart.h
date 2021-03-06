
#ifndef _PC_USART_H
#define _PC_USART_H

/*******************************************************************************
 * Global Includes
 ******************************************************************************/
#include "stm32f0xx.h"

/*******************************************************************************
 * Local Includes
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Global Processor Definitions
 ******************************************************************************/
#define PC_EOL "\r"

/*******************************************************************************
 * Enum and Type definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * External Variable Definitions
 ******************************************************************************/
/* None */

/*******************************************************************************
 * Public Function Declarations
 ******************************************************************************/

/**
 * DESCRIPTION
 * Initial setup for PC USART comms
 * 
 * INPUTS
 * None
 *
 * RETURNS
 * Nothing
 */
void pc_config(void);

/**
 * DESCRIPTION
 * Transmit byte to PC
 * 
 * INPUTS
 * data (uint8_t) : byte to transmit
 *
 * RETURNS
 * Nothing
 */
void pc_send_byte(uint8_t data);

/**
 * DESCRIPTION
 * Transmit null-terminated string to PC
 * 
 * INPUTS
 * str (char *) : Null-terminated string to transmit
 *
 * RETURNS
 * Nothing
 */
void pc_send_string(char * str);

#endif /* _PC_USART_H */

/*******************************************************************************
 * End of File
 ******************************************************************************/
