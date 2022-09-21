/*
 * Copyright (c) 2017-2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== rfEasyLinkTx.c ========
 */
 /* Standard C Libraries */
#include <stdlib.h>
/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/PIN.h>

/* Board Header files */
#include "ti_drivers_config.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"

/* Application header files */
#include <ti_radio_config.h>

/* CRS protocol */
#include "mac/macTask.h"
#include "application/collector.h"
#include "cp_cli.h"
#include "application/crs/crs_cli.h"
#include "mac/mediator.h"
#include "oad/crs_oad.h"





#define RFEASYLINKTX_ASYNC

#define RFEASYLINKTX_TASK_STACK_SIZE    5
#define RFEASYLINKTX_TASK_PRIORITY      2

#define RFEASYLINKTX_BURST_SIZE         10
#define RFEASYLINKTXPAYLOAD_LENGTH      30

#define APP_TASK_STACK_SIZE 18000

#define APP_TASK_PRIORITY   1

Task_Struct appTask;        /* not static so you can see in ROV */
static uint8_t appTaskStack[APP_TASK_STACK_SIZE];


Task_Struct txTask;    /* not static so you can see in ROV */
//static Task_Params txTaskParams;
//static uint8_t txTaskStack[RFEASYLINKTX_TASK_STACK_SIZE];


void appTaskFxn(UArg a0, UArg a1)
{
    Collector_init();

    /* Kick off application - Forever loop */
       while(1)
       {
           Collector_process();
       }
}

void appTask_init()
{
//    pinHandle = inPinHandle;
    Task_Params taskParams;

    /* Configure task. */
       Task_Params_init(&taskParams);
       taskParams.stack = appTaskStack;
       taskParams.stackSize = APP_TASK_STACK_SIZE;
       taskParams.priority = APP_TASK_PRIORITY;

       Task_construct(&appTask, appTaskFxn, &taskParams, NULL);


}



/*
 *  ======== main ========
 */
int main(void)
{
    /* Call driver init functions. */
    Board_initGeneral();

    CLI_init(true);

//    CP_CLI_init();

    Mediator_init();
    appTask_init();
//    txTask_init(pinHandle);
    Mac_init();
    /* Start BIOS */
    BIOS_start();

    return (0);
}
