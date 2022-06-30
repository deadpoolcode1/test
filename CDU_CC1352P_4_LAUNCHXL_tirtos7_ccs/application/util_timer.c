/*
 * util_timer.c
 *
 *  Created on: 11 april 2022
 *      Author: nizan
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "util_timer.h"
/******************************************************************************
 Local variables
 *****************************************************************************/

static Clock_Params gclkTimerParams;
/******************************************************************************
 Public Functions
 *****************************************************************************/

Clock_Handle UtilTimer_construct(Clock_Struct *pClock,
Clock_FuncPtr clockCB,
                                 uint32_t clockDuration, uint32_t clockPeriod,
                                 uint8_t startFlag, UArg arg)
{
    Clock_Params_init(&gclkTimerParams);
    gclkTimerParams.period = clockPeriod;
    gclkTimerParams.startFlag = startFlag;
    Clock_construct(pClock, clockCB, (clockDuration * 1000) / Clock_tickPeriod,
                    &gclkTimerParams);
    return (Clock_handle(pClock));

}

void UtilTimer_start(Clock_Struct *pClock)
{

    Clock_Handle timerHandle = Clock_handle(pClock);
    Clock_start(timerHandle);
}

bool UtilTimer_isActive(Clock_Struct *pClock)
{
    Clock_Handle timerHandle = Clock_handle(pClock);
    return Clock_isActive(timerHandle);

}

void UtilTimer_stop(Clock_Struct *pClock)
{

    Clock_Handle timerHandle = Clock_handle(pClock);
    Clock_stop(timerHandle);

}

void UtilTimer_setTimeout(Clock_Handle handle, uint32_t timeout)
{
    uint32_t timeoutUs = timeout * 1000;
    Clock_setTimeout(handle, timeoutUs / Clock_tickPeriod);

}

uint32_t UtilTimer_getTimeout(Clock_Handle handle)
{

    return Clock_getTimeout(handle);

}

void UtilTimer_setFunc(Clock_Handle handle, Clock_FuncPtr fxn, UArg arg)
{

    Clock_setFunc(handle, fxn, arg);

}
