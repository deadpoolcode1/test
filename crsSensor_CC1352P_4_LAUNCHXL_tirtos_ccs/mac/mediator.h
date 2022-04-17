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
#include "api_mac.h"
/* ------------------------------------------------------------------------------------------------
 *                                           Function Prototypes
 * ------------------------------------------------------------------------------------------------
 */

//typedef enum
//{
//    MEDIATOR_DATA_IND,
//    MEDIATOR_ASSOC_IND,
//    MEDIATOR_DATA_CNF
//}Mediator_msgType_t;

typedef struct
{

    ApiMac_mcpsDataReq_t* msg;
} Mediator_msgObjSentToMac_t;

typedef struct
{

    macCbackEvent_t* msg;
} Mediator_msgObjSentToApp_t;

void Mediator_init();

void Mediator_setAppSem(void* sem);
void Mediator_setMacSem(void* sem);


void Mediator_sendMsgToApp(Mediator_msgObjSentToApp_t* msg );
void Mediator_sendMsgToMac(Mediator_msgObjSentToMac_t* msg );
//mac calls this func to get msgs from app
bool Mediator_getNextAppMsg(Mediator_msgObjSentToMac_t* msg );
//app calls this func to get msgs from mac
bool Mediator_getNextMacMsg(Mediator_msgObjSentToApp_t* msg );

#endif /* MAC_MEDIATOR_H_ */
