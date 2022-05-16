/*
 * crs_alarms.c
 *
 *  Created on: 3 ???? 2022
 *      Author: cellium
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_alarms.h"
#include "crs_nvs.h"
#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include "crs_fpga.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>
#include "mac/mac_util.h"
#include <ti/drivers/GPIO.h>
#include <ti/sysbios/knl/Clock.h>

/******************************************************************************
 Local variables
 *****************************************************************************/
static uint8_t gAlarmArr[ALARMS_NUM];
static Temperature_NotifyObj gNotifyObject;
static Semaphore_Handle collectorSem;
static uint16_t Alarms_events = 0;
static Clock_Params gClkParams;
static Clock_Struct gClkStruct;
static Clock_Handle gClkHandle;

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ALARMS_SET_TEMP_ALARM_EVT 0x0001
#define ALARMS_SET_TDDLOCK_ALARM_EVT 0x0002
#define ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT 0x0004
#define ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT 0x0008
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void Alarms_PLL_Check(void *arg);
static void Alarms_PLLPrimaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void Alarms_PLLSecondaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t Alarms_parseRsp(const FPGA_cbArgs_t _cbArgs,
                                    uint32_t *rspUint32);

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Alarms_printAlarms()
{
    CLI_cliPrintf("\r\n1 DLMaxInputPower 0x%x", gAlarmArr[DLMaxInputPower]);
    CLI_cliPrintf("\r\n2 ULMaxOutputPower 0x%x", gAlarmArr[ULMaxOutputPower]);
    CLI_cliPrintf("\r\n3 MaxCableLoss 0x%x", gAlarmArr[MaxCableLoss]);
    CLI_cliPrintf("\r\n4 SystemTemperature 0x%x", gAlarmArr[SystemTemperature]);
    CLI_cliPrintf("\r\n5 ULMaxInputPower 0x%x", gAlarmArr[ULMaxInputPower]);
    CLI_cliPrintf("\r\n6 DLMaxOutputPower 0x%x", gAlarmArr[DLMaxOutputPower]);
    CLI_cliPrintf("\r\n7 TDDLock 0x%x", gAlarmArr[TDDLock]);
    CLI_cliPrintf("\r\n8 PLLLockPrimary 0x%x", gAlarmArr[PLLLockPrimary]);
    CLI_cliPrintf("\r\n9 PLLLockSecondary 0x%x", gAlarmArr[PLLLockSecondary]);
    CLI_cliPrintf("\r\n10 SyncPLLLock 0x%x", gAlarmArr[SyncPLLLock]);

}
/**
 * turns on the alarm_active bit
 */
CRS_retVal_t Alarms_setAlarm(Alarms_alarmType_t alarmType)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }
    gAlarmArr[alarmType] |= 1UL << ALARM_ACTIVE_BIT_LOCATION; //turn on the alarm active bit
    gAlarmArr[alarmType] |= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
}

/**
 * @brief clears an alarm or clears a sticky mode.
 * alarmMode : ALARM_INACTIVE | ALARM_STICKY
 */
CRS_retVal_t Alarms_clearAlarm(Alarms_alarmType_t alarmType,
                               Alarms_alarmMode_t alarmMode)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }

    if (alarmMode == ALARM_INACTIVE)
    {
        //move to sticky state if before was in active
        if ((gAlarmArr[alarmType] & 1) == ALARM_ACTIVE)
        {
            gAlarmArr[alarmType] |= 1UL << ALARM_STICKY_BIT_LOCATION; //turn on the alarm sticky bit
        }
        gAlarmArr[alarmType] &= ~(1UL << ALARM_ACTIVE_BIT_LOCATION); //turn off the alarm active bit
        if (alarmType == SystemTemperature)
        {
            char envFile[1024] = { 0 };
            //System Temperature : ID=4, thrshenv= tmp
            Thresh_readVarsFile("TmpThr", envFile, 1);
            int16_t highTempThrsh = strtol(envFile + strlen("TmpThr="),
            NULL,
                                           10);
            CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh);

        }
        return CRS_SUCCESS;
    }
    //clear sticky alarm
    else if (alarmMode == ALARM_STICKY)
    {
        gAlarmArr[alarmType] &= ~(1UL << ALARM_STICKY_BIT_LOCATION); //turn off the alarm sticky bit
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

CRS_retVal_t Alarms_process(void)
{
    if (Alarms_events & ALARMS_SET_TEMP_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperature);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_ALARM_EVT);
    }
    if (Alarms_events & ALARMS_SET_TDDLOCK_ALARM_EVT)
    {
//        CLI_cliPrintf("\r\nTDD interrupt!");
        //if rising edge-->tdd is not locked!
        if (GPIO_read(CONFIG_GPIO_BTN1))
        {
            Alarms_setAlarm(TDDLock);
        }
        else
        {
            Alarms_clearAlarm(TDDLock, ALARM_INACTIVE);
        }
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TDDLOCK_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT)
    {

//            CLI_cliPrintf("\r\npll checking!");
        char checkTddLockFirstChip[70] =
                "wr 0xff 0x8000\nwr 0x51 0x510000\nrd 0x51";
        //check fpga open
        if (Fpga_isOpen() == CRS_SUCCESS)
        {
            if (Fpga_writeMultiLineNoPrint(checkTddLockFirstChip,
                                           Alarms_PLLPrimaryFpgaRsp)
                    == CRS_SUCCESS)
            {

            }
            else
            {

            }

        }
        else
        {

        }
        //check if fpga is busy

        //if open&&not busy- write to fpga 'wr 0x51 0x510000'\n'rd 0x51' and parse the reso with a callback

        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT);
    }
    if (Alarms_events & ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT)
    {

        //            CLI_cliPrintf("\r\npll checking!");
        char checkTddLockSecondChip[70] =
                "wr 0xff 0x8001\nwr 0x51 0x510000\nrd 0x51"; //need to complete
        //check fpga open
        if (Fpga_isOpen() == CRS_SUCCESS)
        {
            if (Fpga_writeMultiLineNoPrint(checkTddLockSecondChip,
                                           Alarms_PLLSecondaryFpgaRsp)
                    == CRS_SUCCESS)
            {

            }
            else
            {

            }

        }
        else
        {

        }
        //check if fpga is busy

        //if open&&not busy- write to fpga 'wr 0x51 0x510000'\n'rd 0x51' and parse the reso with a callback

        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT);
    }

}

static void Alarms_PLLPrimaryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    uint32_t resp = 0;
    Alarms_parseRsp((_cbArgs), &resp);
    if (CHECK_BIT(resp, 9))
    {
        Alarms_clearAlarm(PLLLockPrimary, ALARM_INACTIVE);
    }
    else
    {
        Alarms_setAlarm(PLLLockPrimary);
    }

    //set event
    Util_setEvent(&Alarms_events, ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);

}

static void Alarms_PLLSecondaryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    uint32_t resp = 0;
    Alarms_parseRsp((_cbArgs), &resp);
    if (CHECK_BIT(resp, 9))
    {
        Alarms_clearAlarm(PLLLockSecondary, ALARM_INACTIVE);
    }
    else
    {
        Alarms_setAlarm(PLLLockSecondary);
    }
}

static CRS_retVal_t Alarms_parseRsp(const FPGA_cbArgs_t _cbArgs,
                                    uint32_t *rspUint32)
{
    char *line = _cbArgs.arg3;
    char rdRespLine[200] = { 0 };
    uint32_t size = _cbArgs.arg0;

    memset(rdRespLine, 0, 200);

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
            rdRespLine[gTmpLine_idx] = line[counter];
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
            rdRespLine[gTmpLine_idx] = line[counter];
            gTmpLine_idx++;
            counter++;
            continue;

        }
        counter++;
    }

    *rspUint32 = strtoul(rdRespLine + 6, NULL, 16);

}

CRS_retVal_t Alarms_getTemperature(int16_t *currentTemperature)
{
    *currentTemperature = Temperature_getTemperature();
}

CRS_retVal_t Alarms_setTemperatureHigh(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyHigh(
            &gNotifyObject, temperature, Alarms_tempThresholdNotifyFxn, NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_setTemperatureLow(int16_t temperature)
{
    temperature = Temperature_getTemperature();
}

/*!
 *  @brief This function attaches the collector semaphore .
 *
 */
CRS_retVal_t Alarms_init(void *sem)
{
    collectorSem = sem;
    Alarms_temp_Init();
    Alarms_TDDLock_Init();
    Alarms_PLL_Check_Clock_Init((Clock_FuncPtr) Alarms_PLL_Check);
}

/*!
 *  @brief This function initializes the Temperature driver, and sets an threshold for it .
 *
 */
CRS_retVal_t Alarms_temp_Init()
{
    Temperature_init();
    char envFile[1024] = { 0 };
    //System Temperature : ID=4, thrshenv= tmp
    Thresh_readVarsFile("TmpThr", envFile, 1);
    int16_t highTempThrsh = strtol(envFile + strlen("TmpThr="),
    NULL,
                                   10);
    CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh);
    return status;

}

/*!
 *  @brief set interrupt on both edges whenever the tdd is not locked
 *
 */
CRS_retVal_t Alarms_TDDLock_Init()
{
    GPIO_init();
    //set on both edges
//    GPIO_setConfig(CONFIG_GPIO_BTN1, GPIO_CFG_IN_INT_BOTH_EDGES);
    GPIO_setCallback(CONFIG_GPIO_BTN1, Alarms_TDDLockNotifyFxn);
    GPIO_enableInt(CONFIG_GPIO_BTN1);
    //initial check
    if (GPIO_read(CONFIG_GPIO_BTN1))
    {
        Alarms_setAlarm(TDDLock);
    }
}

CRS_retVal_t Alarms_PLL_Check_Clock_Init(Clock_FuncPtr clockFxn)
{
    memset(&gClkStruct, 0, sizeof(gClkStruct));
    memset(&gClkHandle, 0, sizeof(gClkHandle));
    Clock_Params_init(&gClkParams);
    gClkParams.period = 1000000 / Clock_tickPeriod; //params.period specifies the periodic rate. (BTW, setting it = 0 gives us a 1-shot timer.)
    gClkParams.startFlag = FALSE; //params.startFlag tells the user-clock instance to run right-away (after BIOS_start()).
    Clock_construct(&gClkStruct, clockFxn, 100000 / Clock_tickPeriod,
                    &gClkParams);
//    Clock_setFunc(gClkHandle,clockFxn,NULL);
    gClkHandle = Clock_handle(&gClkStruct);
    Clock_start(gClkHandle);
    //for cnc to work, by defult would stop pooling until a user writes in the cli 'alarms start'
    Alarms_stopPooling();
}

static void Alarms_PLL_Check(void *arg)
{
    //set event
    Util_setEvent(&Alarms_events, ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);

}

CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg)
{

    char envFile[1024] = { 0 };
    //MaxCableLoss : ID=3, thrshenv= MaxCableLossThr
    Thresh_readVarsFile("MaxCableLossThr", envFile, 1);
    int8_t highRssiThrsh = strtol(envFile + strlen("MaxCableLossThr="),
    NULL,
                                  10);
    if (rssiAvg > highRssiThrsh)
    {
        Alarms_setAlarm(MaxCableLoss);
    }

}

void Alarms_tempThresholdNotifyFxn(int16_t currentTemperature,
                                   int16_t thresholdTemperature,
                                   uintptr_t clientArg,
                                   Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

void Alarms_TDDLockNotifyFxn(uint_least8_t index)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TDDLOCK_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

CRS_retVal_t Alarms_stopPooling()
{
    Clock_stop(gClkHandle);
}


CRS_retVal_t Alarms_startPooling()
{
    Clock_start(gClkHandle);
}


