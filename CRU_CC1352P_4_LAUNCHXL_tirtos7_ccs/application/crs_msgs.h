/*
 * crs_msgs.h
 *
 *  Created on: 14 Sep 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_MSGS_H_
#define APPLICATION_CRS_CRS_MSGS_H_


/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "crs/crs.h"
#include "string.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Msgs_init(void *sem);
void Msgs_process(void);
CRS_retVal_t Msgs_addMsg(uint8_t * msg, uint32_t len);
CRS_retVal_t Msgs_sendMsgs(uint16_t shortAddr);
void Msgs_sendNextMsgCb();
void Msgs_sendingMsgFailedCb();



#endif /* APPLICATION_CRS_CRS_MSGS_H_ */
