/*
 * crs_alarms.c
 *
 *  Created on: 3 ???? 2022
 *      Author: nizan
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
#include "crs_global_defines.h"
#ifndef CLI_SENSOR
#include "application/collector.h"
#endif
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ALARMS_SET_TEMP_HIGH_ALARM_EVT 0x0001
#define ALARMS_SET_TEMP_LOW_ALARM_EVT 0x0002
#define ALARMS_SET_TDDLOCK_ALARM_EVT 0x0004
#define ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT 0x0008
#define ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT 0x00010
#define ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT 0x0020
#define ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT 0x0040

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define TEMP_SZ 200
#define EXPECTEDVAL_SZ 20

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
static bool gIsPllPrimaryDiscoverd = false;
static bool gIsPllsecondaryDiscoverd = false;
static char gDiscRdRespLine[TEMP_SZ] = { 0 };
static char gDiscExpectVal[EXPECTEDVAL_SZ] = { 0 };

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void Alarms_PLL_Check(void *arg);
static void Alarms_PLLPrimaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void Alarms_PLLSecondaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void Alarms_PLLPrimaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void Alarms_PLLSecondaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t Alarms_parseRsp(const FPGA_cbArgs_t _cbArgs,
                                    uint32_t *rspUint32);
static CRS_retVal_t Alarms_cmpDiscRsp(char *rsp, char *expVal);
/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t Alarms_printAlarms()
{
    CLI_cliPrintf("\r\n1 DLMaxInputPower 0x%x", gAlarmArr[DLMaxInputPower]);
    CLI_cliPrintf("\r\n2 ULMaxOutputPower 0x%x", gAlarmArr[ULMaxOutputPower]);
    CLI_cliPrintf("\r\n3 MaxCableLoss 0x%x", gAlarmArr[MaxCableLoss]);
    CLI_cliPrintf("\r\n4 SystemTemperatureHigh 0x%x",
                  gAlarmArr[SystemTemperatureHigh]);
    CLI_cliPrintf("\r\n5 SystemTemperatureLow 0x%x",
                  gAlarmArr[SystemTemperatureLow]);
    CLI_cliPrintf("\r\n6 ULMaxInputPower 0x%x", gAlarmArr[ULMaxInputPower]);
    CLI_cliPrintf("\r\n7 DLMaxOutputPower 0x%x", gAlarmArr[DLMaxOutputPower]);
    CLI_cliPrintf("\r\n8 TDDLock 0x%x", gAlarmArr[TDDLock]);
    CLI_cliPrintf("\r\n9 PLLLockPrimary 0x%x", gAlarmArr[PLLLockPrimary]);
    CLI_cliPrintf("\r\n10 PLLLockSecondary 0x%x", gAlarmArr[PLLLockSecondary]);
    CLI_cliPrintf("\r\n11 SyncPLLLock 0x%x", gAlarmArr[SyncPLLLock]);

}
/**
 * turns on the alarm_active bit or the failed to discover bit
 */
CRS_retVal_t Alarms_setAlarm(Alarms_alarmType_t alarmType)
{
    if (alarmType >= ALARMS_NUM)
    {
        return CRS_FAILURE;
    }
    if (alarmType == PLLLockPrimary && (gIsPllPrimaryDiscoverd == false))
    {
        gAlarmArr[alarmType] |= 1UL << ALARM_DISCOVERY_BIT_LOCATION; //turn on the failed discovery bit
        return CRS_SUCCESS;
    }
    if (alarmType == PLLLockSecondary && (gIsPllsecondaryDiscoverd == false))
    {
        gAlarmArr[alarmType] |= 1UL << ALARM_DISCOVERY_BIT_LOCATION; //turn on the failed discovery bit
        return CRS_SUCCESS;
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
        if (alarmType == SystemTemperatureHigh)
        {
            char envFile[4096] = { 0 };
            Thresh_read("UpperTempThr", envFile);
            int16_t highTempThrsh = strtol(envFile + strlen("UpperTempThr="),
            NULL,
                                           10);
            memset(envFile, 0, 4096);
            Thresh_read("TempOffset", envFile);
            int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
            NULL,
                                        10);
            CRS_retVal_t status = Alarms_setTemperatureHigh(
                    highTempThrsh + tempOffset);

        }
        else if (alarmType == SystemTemperatureLow)
        {
            char envFile[4096] = { 0 };
            Thresh_read("LowerTempThr", envFile);
            int16_t lowTempThrsh = strtol(envFile + strlen("LowerTempThr="),
            NULL,
                                           10);
            memset(envFile, 0, 4096);
            Thresh_read("TempOffset", envFile);
            int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
            NULL,
                                        10);
            CRS_retVal_t status = Alarms_setTemperatureLow(
                    lowTempThrsh + tempOffset);
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
    if (Alarms_events & ALARMS_SET_TEMP_HIGH_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperatureHigh);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_HIGH_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_TEMP_LOW_ALARM_EVT)
    {
        Alarms_setAlarm(SystemTemperatureLow);
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TEMP_LOW_ALARM_EVT);
    }
    if (Alarms_events & ALARMS_SET_TDDLOCK_ALARM_EVT)
    {
//        CLI_cliPrintf("\r\nTDD interrupt!");
        //if rising edge-->tdd is not locked!
        if (GPIO_read(CONFIG_GPIO_BTN1))
        {
//                PIN_setOutputValue(Crs_pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

            Alarms_setAlarm(TDDLock);
        }
        else
        {
//                PIN_setOutputValue(Crs_pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

            Alarms_clearAlarm(TDDLock, ALARM_INACTIVE);
        }
        /* Clear the event */
        Util_clearEvent(&Alarms_events, ALARMS_SET_TDDLOCK_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT)
    {
        if (Alarms_cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) == CRS_SUCCESS)
        {
            gIsPllPrimaryDiscoverd = true;
            Util_setEvent(&Alarms_events,
            ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT);
        }
        else
        {
            Alarms_setAlarm(PLLLockPrimary);
        }
        memset(gDiscRdRespLine, 0, TEMP_SZ);
        char discoveryPllSecondary[70] =
                "wr 0xff 0x8001\nwr 0x50 0x071234\nwr 0x51 0x070000\nrd 0x51";
        Fpga_writeMultiLineNoPrint(discoveryPllSecondary,
                                   Alarms_PLLSecondaryDiscoveryFpgaRsp);
        /* Clear the event */
        Util_clearEvent(&Alarms_events,
        ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT)
    {
        if (Alarms_cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) == CRS_SUCCESS)
        {
            gIsPllsecondaryDiscoverd = true;
        }
        else
        {
            Alarms_setAlarm(PLLLockSecondary);
        }
        /* Clear the event */
        Util_clearEvent(&Alarms_events,
        ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT)
    {
        if (gIsPllPrimaryDiscoverd)
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
        }
        else
        {
            //set event
            Util_setEvent(&Alarms_events,
            ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT);

            /* Wake up the application thread when it waits for clock event */
            Semaphore_post(collectorSem);

        }
        /* Clear the event */
        Util_clearEvent(&Alarms_events,
        ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT);
    }

    if (Alarms_events & ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT)
    {
        if (gIsPllsecondaryDiscoverd)
        {
            //            CLI_cliPrintf("\r\npll checking!");
            char checkTddLockSecondChip[70] =
                    "wr 0xff 0x8001\nwr 0x51 0x510000\nrd 0x51";
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
        }
        /* Clear the event */
        Util_clearEvent(&Alarms_events,
        ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT);
    }

}

CRS_retVal_t Alarms_getTemperature(int16_t *currentTemperature)
{
    *currentTemperature = Temperature_getTemperature();
}

CRS_retVal_t Alarms_setTemperatureHigh(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyHigh(
            &gNotifyObject, temperature, Alarms_tempThresholdHighNotifyFxn,
            NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

CRS_retVal_t Alarms_setTemperatureLow(int16_t temperature)
{
    int_fast16_t status = Temperature_registerNotifyLow(
            &gNotifyObject, temperature, Alarms_tempThresholdLowNotifyFxn,
            NULL);
    if (status != Temperature_STATUS_SUCCESS)
    {
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;
}

/*!
 *  @brief This function attaches the collector semaphore .
 *
 */
CRS_retVal_t Alarms_init(void *sem)
{
    collectorSem = sem;
    Alarms_temp_Init();
#ifndef CLI_SENSOR
    Alarms_TDDLock_Init();
#endif
    Alarms_PLL_Check_Clock_Init((Clock_FuncPtr) Alarms_PLL_Check);
}

/*!
 *  @brief This function initializes the Temperature driver, and sets an threshold for it .
 *
 */
CRS_retVal_t Alarms_temp_Init()
{
    Temperature_init();
    char envFile[4096] = { 0 };
    //System Temperature : ID=4, thrshenv= tmp
    Thresh_read("UpperTempThr", envFile);
    int16_t highTempThrsh = strtol(envFile + strlen("UpperTempThr="),
    NULL, 10);
    memset(envFile, 0, 4096);
    Thresh_read("TempOffset", envFile);
    int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
    NULL, 10);
    CRS_retVal_t status = Alarms_setTemperatureHigh(highTempThrsh + tempOffset);
    if (status == CRS_FAILURE)
    {
        return status;
    }
    memset(envFile, 0, 4096);
    Thresh_read("LowerTempThr", envFile);
    int16_t lowTempThrsh = strtol(envFile + strlen("LowerTempThr="),
    NULL, 10);
    status = Alarms_setTemperatureLow(lowTempThrsh + tempOffset);

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
    memcpy(gDiscExpectVal, "0x1234", sizeof("0x1234"));
    char discoveryPllPrimary[70] =
            "wr 0xff 0x8000\nwr 0x50 0x071234\nwr 0x51 0x070000\nrd 0x51";
    Fpga_writeMultiLine(discoveryPllPrimary, Alarms_PLLPrimaryDiscoveryFpgaRsp);
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

CRS_retVal_t Alarms_checkRssi(int8_t rssiAvg)
{

    char envFile[4096] = { 0 };
    //Max Cable Loss: ID=3, thrshenv= MaxCableLoss
//    memcpy(envFile, "MaxCableLoss", strlen("MaxCableLoss"));
    Thresh_read("MaxCableLossThr", envFile);
    uint32_t MaxCableLossThr = strtol(envFile + strlen("MaxCableLossThr="),
    NULL,
                                      10);
    memset(envFile, 0, 4096);
    Thresh_read("ModemTxPwr", envFile);
    uint32_t ModemTxPwr = strtol(envFile + strlen("ModemTxPwr="), NULL, 10);
    memset(envFile, 0, 4096);
    Thresh_read("CblCompFctr", envFile);
    uint32_t CblCompFctr = strtol(envFile + strlen("CblCompFctr="), NULL, 10);
    if ((ModemTxPwr - (rssiAvg) - CblCompFctr) > MaxCableLossThr)
    {
        Alarms_setAlarm(MaxCableLoss);
    }
    else
    {
        Alarms_clearAlarm(MaxCableLoss, ALARM_INACTIVE);
    }

}

void Alarms_tempThresholdHighNotifyFxn(int16_t currentTemperature,
                                       int16_t thresholdTemperature,
                                       uintptr_t clientArg,
                                       Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_HIGH_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

void Alarms_tempThresholdLowNotifyFxn(int16_t currentTemperature,
                                      int16_t thresholdTemperature,
                                      uintptr_t clientArg,
                                      Temperature_NotifyObj *notifyObject)
{
    Util_setEvent(&Alarms_events, ALARMS_SET_TEMP_LOW_ALARM_EVT);

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

CRS_retVal_t Alarms_discoveryPllPrimary()
{

}

CRS_retVal_t Alarms_discoveryPllSecondary()
{
}

/******************************************************************************
 Local Functions
 *****************************************************************************/

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

static void Alarms_PLLPrimaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    memcpy(gDiscRdRespLine, _cbArgs.arg3, strlen(_cbArgs.arg3));
//    CLI_cliPrintf("\r\ndiscovery pll primary resp: %s",_cbArgs.arg3);
    Util_setEvent(&Alarms_events, ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

static void Alarms_PLLSecondaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    memcpy(gDiscRdRespLine, _cbArgs.arg3, strlen(_cbArgs.arg3));
//    CLI_cliPrintf("\r\ndiscovery pll primary resp: %s",_cbArgs.arg3);
    Util_setEvent(&Alarms_events, ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
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

static void Alarms_PLL_Check(void *arg)
{
    //set event
    Util_setEvent(&Alarms_events, ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);

}

static CRS_retVal_t Alarms_cmpDiscRsp(char *rsp, char *expVal)
{
    uint32_t expValUint = strtoul(gDiscExpectVal, NULL, 16);
    uint32_t rdValUint = strtoul(&gDiscRdRespLine[16], NULL, 16);
    if (expValUint == rdValUint)
    {
        return CRS_SUCCESS;
    }

    return CRS_FAILURE;

}

