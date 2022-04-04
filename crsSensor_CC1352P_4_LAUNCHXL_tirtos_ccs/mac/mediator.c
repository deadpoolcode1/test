/*
 * mediator.c
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "mediator.h"
#include <ti/sysbios/BIOS.h>
#include <xdc/runtime/Error.h>

/******************************************************************************
 Global variables
 *****************************************************************************/

/******************************************************************************
 Local variables
 *****************************************************************************/
//sent to mac
static Mailbox_Params mbxParamsToMac;
static Mailbox_Handle mbxHandleToMac;

//sent to app
static Mailbox_Params mbxParamsToApp;
static Mailbox_Handle mbxHandleToApp;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


/******************************************************************************
 Public Functions
 *****************************************************************************/

void Mediator_init()
{
    Mailbox_Params_init(&mbxParamsToMac);
    mbxHandleToMac = Mailbox_create(sizeof(Mediator_msgObj_t), 5, &mbxParamsToMac, Error_IGNORE);

    Mailbox_Params_init(&mbxParamsToApp);
    mbxHandleToApp = Mailbox_create(sizeof(Mediator_msgObj_t), 5, &mbxParamsToApp,
                                    Error_IGNORE);

}
void Mediator_sendMsgToApp(Mediator_msgObj_t* msg )
{
    Mailbox_post(mbxHandleToApp, msg, BIOS_NO_WAIT);
}
void Mediator_sendMsgToMac(Mediator_msgObj_t* msg )
{
    Mailbox_post(mbxHandleToMac, msg, BIOS_NO_WAIT);
}

//mac calls this func to get msgs from app
void Mediator_getNextAppMsg(Mediator_msgObj_t* msg )
{
    Mailbox_pend(mbxHandleToMac, msg, BIOS_NO_WAIT);

}

//app calls this func to get msgs from mac
void Mediator_getNextMacMsg(Mediator_msgObj_t* msg )
{
    Mailbox_pend(mbxHandleToApp, msg, BIOS_NO_WAIT);
}

