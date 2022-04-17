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
#include <ti/sysbios/knl/Semaphore.h>

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

static void *appSem;
static void *macSem;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/


/******************************************************************************
 Public Functions
 *****************************************************************************/

void Mediator_init()
{
    Mailbox_Params_init(&mbxParamsToMac);
    mbxHandleToMac = Mailbox_create(sizeof(Mediator_msgObjSentToMac_t), 5, &mbxParamsToMac, Error_IGNORE);

    Mailbox_Params_init(&mbxParamsToApp);
    mbxHandleToApp = Mailbox_create(sizeof(Mediator_msgObjSentToApp_t), 5, &mbxParamsToApp,
                                    Error_IGNORE);

}

void Mediator_setAppSem(void* sem)
{
    appSem = sem;
}
void Mediator_setMacSem(void* sem)
{
    macSem = sem;
}

void Mediator_sendMsgToApp(Mediator_msgObjSentToApp_t* msg )
{
    Mailbox_post(mbxHandleToApp, msg, BIOS_NO_WAIT);
    Semaphore_post(appSem);

}
void Mediator_sendMsgToMac(Mediator_msgObjSentToMac_t* msg )
{
    Mailbox_post(mbxHandleToMac, msg, BIOS_NO_WAIT);
    Semaphore_post(macSem);

}

//mac calls this func to get msgs from app
bool Mediator_getNextAppMsg(Mediator_msgObjSentToMac_t* msg )
{
    bool rsp = Mailbox_pend(mbxHandleToMac, msg, BIOS_NO_WAIT);
    return rsp;

}

//app calls this func to get msgs from mac
bool Mediator_getNextMacMsg(Mediator_msgObjSentToApp_t* msg )
{
    return Mailbox_pend(mbxHandleToApp, msg, BIOS_NO_WAIT);
}

