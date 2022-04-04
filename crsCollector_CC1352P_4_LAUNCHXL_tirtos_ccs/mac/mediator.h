/*
 * mediator.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_MEDIATOR_H_
#define MAC_MEDIATOR_H_


/* ------------------------------------------------------------------------------------------------
 *                                          Includes
 * ------------------------------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <ti/sysbios/knl/Mailbox.h>
/* ------------------------------------------------------------------------------------------------
 *                                           Function Prototypes
 * ------------------------------------------------------------------------------------------------
 */

typedef struct MsgObj
{
    uint8_t* msg;
} Mediator_msgObj_t;

void Mediator_init();
void Mediator_sendMsgToApp(Mediator_msgObj_t* msg );
void Mediator_sendMsgToMac(Mediator_msgObj_t* msg );

void Mediator_getNextAppMsg(Mediator_msgObj_t* msg );
void Mediator_getNextMacMsg(Mediator_msgObj_t* msg );

#endif /* MAC_MEDIATOR_H_ */
