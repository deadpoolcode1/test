/*
 * crs_locks.c
 *
 *  Created on: Oct 30, 2022
 *      Author: yardenr
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdlib.h>
#include <ti/sysbios/knl/Semaphore.h>

#include "application/crs/crs_locks.h"
#include "application/util_timer.h"
#include "mac/mac_util.h"
#include "application/crs/crs_agc_management.h"
#include "application/crs/crs_alarms.h"
#include "application/crs/crs_fpga.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define LOCKS_CHECKLOCK_EV 0x1
#define LOCKS_READ_NEXT_REG_EV 0x2
#define LOCKS_FINISH_READING_REG_EV 0x4
#define LOCKS_OPEN_FAIL_EV  0x8

#define HEXADECIMAL 16
#define ONE_SECOND  1000
#define LOCKS_LINE_SZ   30

#ifndef CLI_SENSOR
#define READ_TDD_REG    "rd 0x23\r"
#else
#define READ_ADF_REG    "rd 0x41\r"
#endif
#define READ_TI_REG     "rd 0x41\r"

#ifdef CLI_SENSOR
#define BIT_0   0x1
#endif

#define BIT_1   0x2



enum lockType
{
#ifndef CLI_SENSOR
    lockType_tddLock,
#else
    lockType_adfLock,
#endif
    lockType_tiLock,

    lockType_numOfLockTypes
};


enum lockStatus
{
    lockStatus_Unlocked = 0,
    lockStatus_Locked = 1
};

typedef uint8_t Locks_lockChecker_t [lockType_numOfLockTypes] ;

typedef CRS_retVal_t (*locks_WriteFpgafxn)(void);
typedef CRS_retVal_t (*locks_ReadValuefxn)(uint32_t);


/******************************************************************************
 Local variables
 *****************************************************************************/
static Semaphore_Handle collectorSem;
static Clock_Struct locksClkStruct;
static Clock_Handle locksClkHandle;
static uint16_t Locks_events = 0;

static Locks_lockChecker_t gLockChecker = {0};
static uint16_t gLocksIdx = 0;
static bool gGettersArray[lockType_numOfLockTypes] = {
     true,
     true
};

static Locks_checkLocksCB gCallBack = NULL;

#ifndef CLI_SENSOR
static CRS_retVal_t writeToTddLockReg(void);
#else
static CRS_retVal_t writeToAdfLockReg(void);
#endif
static CRS_retVal_t writeToTiLockReg(void);

#ifndef CLI_SENSOR
static CRS_retVal_t saveTddLockStatus(uint32_t val);
#else
static CRS_retVal_t saveAdfLockStatus(uint32_t val);
#endif
static CRS_retVal_t saveTiLockStatus(uint32_t val);


//static void checkCurrentLock();
static void MoveIdxToNextLockIdx(void);
static CRS_retVal_t setAlarms(void);
static void getLockValueCallback(const FPGA_cbArgs_t _cbArgs);
static void processLocksTimeoutCallback(UArg a0);
static void setLocksClock(uint32_t locksTime);
static CRS_retVal_t valsInit(void);
static void startCheckingLocks(void);


static locks_WriteFpgafxn gFpgaWriteFunctionsTable [lockType_numOfLockTypes] =
{
#ifndef CLI_SENSOR
 writeToTddLockReg,
#else
 writeToAdfLockReg,
#endif
 writeToTiLockReg
};


static locks_ReadValuefxn gSaveValueFunctionTable [lockType_numOfLockTypes] =
{
#ifndef CLI_SENSOR
 saveTddLockStatus,
#else
 saveAdfLockStatus,
#endif
 saveTiLockStatus
};

//        FPGA_setFpgaClock(20);

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t Locks_init(void *sem)
{
    collectorSem = sem;
    locksClkHandle = UtilTimer_construct(&locksClkStruct,
                                        processLocksTimeoutCallback,
                                        0,
                                        0,
                                        false,
                                        0);
    setLocksClock(5 * ONE_SECOND);

    return CRS_SUCCESS;

}
CRS_retVal_t Locks_process(void)
{
if (Locks_events & LOCKS_CHECKLOCK_EV)
{
//    CLI_cliPrintf("\r\nLOCKS_CHECKLOCK_EV");
    AGCM_runTask(startCheckingLocks);

    Util_clearEvent(&Locks_events, LOCKS_CHECKLOCK_EV);
}

if (Locks_events & LOCKS_READ_NEXT_REG_EV)
{
//    CLI_cliPrintf("\r\nLOCKS_READ_NEXT_REG_EV");

    MoveIdxToNextLockIdx();

    Util_clearEvent(&Locks_events, LOCKS_READ_NEXT_REG_EV);
}

if (Locks_events & LOCKS_FINISH_READING_REG_EV)
{
    setAlarms();
    // fix unlockness
    if (NULL != gCallBack)
    {
//        CLI_cliPrintf("\r\nfinished cli call");
        gCallBack();
        gCallBack = NULL;
    }
    else
    {
//        CLI_cliPrintf("\r\nfinished clock round");
        AGCM_finishedTask();
        setLocksClock(30 * ONE_SECOND);
    }

    Util_clearEvent(&Locks_events, LOCKS_FINISH_READING_REG_EV);
}


if(Locks_events & LOCKS_OPEN_FAIL_EV)
{
    //do something
    Util_clearEvent(&Locks_events, LOCKS_OPEN_FAIL_EV);
}


return CRS_SUCCESS;
}


void Locks_checkLocks(Locks_checkLocksCB cb)
{
    gCallBack = cb;
    startCheckingLocks();
}


static void startCheckingLocks(void)
{
    valsInit();
    gFpgaWriteFunctionsTable[gLocksIdx]();
}


#ifndef CLI_SENSOR
bool Locks_getTddLockVal(void)
{
    return gGettersArray[lockType_tddLock];
}
#else
bool Locks_getAdfLockVal(void)
{
    return gGettersArray[lockType_adfLock];
}
#endif
bool Locks_getTiLockVal(void)
{
    return gGettersArray[lockType_tiLock];
}


//static void checkCurrentLock()
//{
//    gFpgaWriteFunctionsTable[gLocksIdx]();
//}


static void getLockValueCallback(const FPGA_cbArgs_t _cbArgs)
{

    char *line = _cbArgs.arg3;
    char lineRspStr[LOCKS_LINE_SZ] = {0};

       int gTmpLine_idx = 0;
       int counter = 0;
       bool isNumber = false;
       bool isFirst = true;
       while (memcmp(&line[counter], "AP>", 3) != 0)
       {
           if (line[counter] == '0' && line[counter + 1] == 'x')
           {
               if (isFirst == true)
               {
                   counter++;
                   isFirst = false;
                   continue;

               }
               isNumber = true;
               lineRspStr[gTmpLine_idx] = line[counter];
               gTmpLine_idx++;
               counter++;
               continue;
           }

           if (line[counter] == '\r' || line[counter] == '\n')
           {
               isNumber = false;
           }

           if (isNumber == true)
           {
               lineRspStr[gTmpLine_idx] = line[counter];
               gTmpLine_idx++;
               counter++;
               continue;

           }
           counter++;
       }

       uint32_t val =  strtoul(&lineRspStr[6], NULL, HEXADECIMAL);
       gSaveValueFunctionTable[gLocksIdx](val);

       Util_setEvent(&Locks_events, LOCKS_READ_NEXT_REG_EV);

       Semaphore_post(collectorSem);

}

void MoveIdxToNextLockIdx(void)
{
    gLocksIdx++;
    if(gLocksIdx == lockType_numOfLockTypes)
    {
        Util_setEvent(&Locks_events, LOCKS_FINISH_READING_REG_EV);
    }

    else
    {
        gFpgaWriteFunctionsTable[gLocksIdx]();
    }
}

static void processLocksTimeoutCallback(UArg a0)
{

    Util_setEvent(&Locks_events, LOCKS_CHECKLOCK_EV);

    Semaphore_post(collectorSem);

}

static void setLocksClock(uint32_t locksTime)
{
    /* Stop the Tracking timer */
    if (UtilTimer_isActive(&locksClkStruct) == true)
    {
        UtilTimer_stop(&locksClkStruct);
    }

    if (locksTime)
    {
        /* Setup timer */
        UtilTimer_setTimeout(locksClkHandle, locksTime);
        UtilTimer_start(&locksClkStruct);
    }
}

static CRS_retVal_t setAlarms(void)
{
    Alarms_alarmType_t alarm_types[lockType_numOfLockTypes] =
    {
#ifndef CLI_SENSOR
     TDDLock,
     PLLLockPrimary
#else
     PLLLockPrimary,
     SyncPLLLock
#endif
    };


    uint16_t i = 0;
    for(i = 0; i < lockType_numOfLockTypes; i++)
    {
        if (gLockChecker[i] == lockStatus_Unlocked)
        {
            Alarms_setAlarm(alarm_types[i]);
        }
        else
        {
            Alarms_clearAlarm(alarm_types[i], ALARM_INACTIVE);
        }
    }

    return CRS_SUCCESS;

}

#ifndef CLI_SENSOR
static CRS_retVal_t writeToTddLockReg(void)
{
    Fpga_writeMultiLineNoPrint(READ_TDD_REG, getLockValueCallback);

    return CRS_SUCCESS;
}
#else
static CRS_retVal_t writeToAdfLockReg(void)
{
    Fpga_writeMultiLineNoPrint(READ_ADF_REG, getLockValueCallback);

    return CRS_SUCCESS;
}
#endif
static CRS_retVal_t writeToTiLockReg(void)
{
    Fpga_writeMultiLineNoPrint(READ_TI_REG, getLockValueCallback);

    return CRS_SUCCESS;
}


#ifndef CLI_SENSOR
static CRS_retVal_t saveTddLockStatus(uint32_t val)
{
    bool isLocked = val != lockStatus_Unlocked;

    if (isLocked)
    {
     gLockChecker[lockType_tddLock] = lockStatus_Locked;
    }
    else
    {
     gLockChecker[lockType_tddLock] = lockStatus_Unlocked;
    }


    gGettersArray[lockType_tddLock] = isLocked;
//    CLI_cliPrintf("\r\nis tdd locked %x", isLocked);
    return CRS_SUCCESS;
}

#else
static CRS_retVal_t saveAdfLockStatus(uint32_t val)
{
    bool isLocked = ((val & BIT_0) == lockStatus_Unlocked) ? false :true;
    if (isLocked)
    {
       gLockChecker[lockType_adfLock] = lockStatus_Locked;
    }
    else
    {
       gLockChecker[lockType_adfLock] = lockStatus_Unlocked;
    }

    gGettersArray[lockType_adfLock] = isLocked;

    return CRS_SUCCESS;
}

#endif

static CRS_retVal_t saveTiLockStatus(uint32_t val)
{

    bool isLocked = ((val & BIT_1) == lockStatus_Unlocked) ? false :true;
    if (isLocked)
    {
        gLockChecker[lockType_tiLock] = lockStatus_Locked;
    }
    else
    {
        gLockChecker[lockType_tiLock] = lockStatus_Unlocked;
    }

    gGettersArray[lockType_tiLock] = isLocked;

    return CRS_SUCCESS;
}

static CRS_retVal_t valsInit(void)
{
    gLocksIdx = 0;
    memset(gLockChecker,0,sizeof(gLockChecker));

    return CRS_SUCCESS;
}
