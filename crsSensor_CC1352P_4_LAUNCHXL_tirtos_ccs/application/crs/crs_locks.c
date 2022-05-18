/*
 * crs_locks.c
 *
 *  Created on: 17 May 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_alarms.h"
#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include "crs_fpga.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/GPIO.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ALARMS_SET_TDDLOCK_ALARM_EVT 0x0002
#define ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT 0x0004
#define ALARMS_SET_CHECKPLLSECONDARY_ALARM_EVT 0x0008
#define ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT 0x0010
#define ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT 0x0020


#define EXPECTEDVAL_SZ 20
#define TEMP_SZ 200

/******************************************************************************
 Local variables
 *****************************************************************************/

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
static CRS_retVal_t tddLockInit();
static CRS_retVal_t pllCheckClockInit(Clock_FuncPtr clockFxn);
static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal);
static void pllCheck(void *arg);
static CRS_retVal_t parseRsp(const FPGA_cbArgs_t _cbArgs,
                                    uint32_t *rspUint32);
static void pllSecondaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static CRS_retVal_t parseRsp(const FPGA_cbArgs_t _cbArgs,
                                    uint32_t *rspUint32);
static void pllPrimaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void pllSecondaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
static void pllPrimaryFpgaRsp(const FPGA_cbArgs_t _cbArgs);
/******************************************************************************
 Public Functions
 *****************************************************************************/


CRS_retVal_t Locks_init(void *sem)
{

    collectorSem = sem;
    tddLockInit();
    pllCheckClockInit((Clock_FuncPtr) pllCheck);

}

static CRS_retVal_t tddLockInit()
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
    else
    {
        PIN_setOutputValue(Crs_pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

    }
}

static CRS_retVal_t pllCheckClockInit(Clock_FuncPtr clockFxn)
{
    memcpy(gDiscExpectVal, "0x1234", sizeof("0x1234"));
    char discoveryPllPrimary[70] =
            "wr 0xff 0x8000\nwr 0x50 0x071234\nwr 0x51 0x070000\nrd 0x51";
    CRS_retVal_t rsp = Fpga_writeMultiLine(discoveryPllPrimary, pllPrimaryFpgaRsp);
    if (rsp != CRS_SUCCESS)
    {

    }
    memset(&gClkStruct, 0, sizeof(gClkStruct));
    memset(&gClkHandle, 0, sizeof(gClkHandle));
    Clock_Params_init(&gClkParams);
    gClkParams.period = 1000000 / Clock_tickPeriod; //params.period specifies the periodic rate. (BTW, setting it = 0 gives us a 1-shot timer.)
    gClkParams.startFlag = FALSE; //params.startFlag tells the user-clock instance to run right-away (after BIOS_start()).
    Clock_construct(&gClkStruct, clockFxn, 100000 / Clock_tickPeriod,
                    &gClkParams);
//    Clock_setFunc(gClkHandle,clockFxn,NULL);
    gClkHandle = Clock_handle(&gClkStruct);
//    Clock_start(gClkHandle);
    //for cnc to work, by defult would stop pooling until a user writes in the cli 'alarms start'

}


CRS_retVal_t Locks_process(void)
{
    if (Alarms_events & ALARMS_SET_TDDLOCK_ALARM_EVT)
      {
  //        CLI_cliPrintf("\r\nTDD interrupt!");
          //if rising edge-->tdd is not locked!
          if (GPIO_read(CONFIG_GPIO_BTN1))
          {
                  PIN_setOutputValue(Crs_pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

              Alarms_setAlarm(TDDLock);
          }
          else
          {
                  PIN_setOutputValue(Crs_pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));

              Alarms_clearAlarm(TDDLock, ALARM_INACTIVE);
          }
          /* Clear the event */
          Util_clearEvent(&Alarms_events, ALARMS_SET_TDDLOCK_ALARM_EVT);
      }

      if (Alarms_events & ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT)
      {
          if (cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) == CRS_SUCCESS)
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
                                     pllSecondaryDiscoveryFpgaRsp);
          /* Clear the event */
          Util_clearEvent(&Alarms_events,
          ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT);
      }
//TODO: NOW WHAT??
      if (Alarms_events & ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT)
      {
          if (cmpDiscRsp(gDiscRdRespLine, gDiscExpectVal) == CRS_SUCCESS)
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
                                                 pllPrimaryFpgaRsp)
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
                                                 pllSecondaryFpgaRsp)
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


static void pllPrimaryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    uint32_t resp = 0;
    parseRsp((_cbArgs), &resp);
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

static void pllSecondaryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    uint32_t resp = 0;
    parseRsp((_cbArgs), &resp);
    if (CHECK_BIT(resp, 9))
    {
        Alarms_clearAlarm(PLLLockSecondary, ALARM_INACTIVE);
    }
    else
    {
        Alarms_setAlarm(PLLLockSecondary);
    }
}

static void pllPrimaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    memcpy(gDiscRdRespLine, _cbArgs.arg3, strlen(_cbArgs.arg3));
//    CLI_cliPrintf("\r\ndiscovery pll primary resp: %s",_cbArgs.arg3);
    Util_setEvent(&Alarms_events, ALARMS_SET_DISCOVERYPLLPRIMARY_ALARM_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

static void pllSecondaryDiscoveryFpgaRsp(const FPGA_cbArgs_t _cbArgs)
{
    memcpy(gDiscRdRespLine, _cbArgs.arg3, strlen(_cbArgs.arg3));
//    CLI_cliPrintf("\r\ndiscovery pll primary resp: %s",_cbArgs.arg3);
    Util_setEvent(&Alarms_events, ALARMS_SET_DISCOVERYPLLSECONDARY_ALARM_EVT);
    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);
}

static CRS_retVal_t parseRsp(const FPGA_cbArgs_t _cbArgs,
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


static void pllCheck(void *arg)
{
    //set event
    Util_setEvent(&Alarms_events, ALARMS_SET_CHECKPLLPRIMARY_ALARM_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(collectorSem);

}

static CRS_retVal_t cmpDiscRsp(char *rsp, char *expVal)
{
    if (memcmp(&gDiscExpectVal[2], &gDiscRdRespLine[6],
               strlen(&gDiscExpectVal[2])) != 0)
    {
        if (((!strstr(expVal, "16b'")) && (!strstr(expVal, "32b'"))))
        {
            return CRS_FAILURE;
        }

    }

    if (((!strstr(expVal, "16b'")) && (!strstr(expVal, "32b'"))))
    {
        return CRS_SUCCESS;
    }
    int i = 0;
    char tokenValue[100] = { 0 };
    memcpy(tokenValue, &expVal[4], 50);

    if (memcmp(expVal, "16b", 3) == 0)
    {

        for (i = 0; i < 16; i++)
        {
            uint16_t val = CLI_convertStrUint(&rsp[6]);
            uint16_t valPrev = val;
            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val &= ~(1 << (15 - i));
                if (val != (~(0)))
                {
                    return CRS_FAILURE;
                }
            }
            else if (tokenValue[i] == '0')
            {
                val &= (1 << (15 - i));
                if (val != 0)
                {
                    return CRS_FAILURE;
                }
            }

        }

    }
    else if (memcmp(expVal, "32b", 3) == 0)
    {

        for (i = 0; i < 32; i++)
        {
            uint32_t val = CLI_convertStrUint(&rsp[6]);
            uint32_t valPrev = val;

            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val |= (1 << (31 - i));
            }
            else if (tokenValue[i] == '0')
            {
                val &= ~(1 << (31 - i));
            }

            if (val != valPrev)
            {
                return CRS_FAILURE;
            }
        }

//        int2hex(val, valStr);

    }

    return CRS_SUCCESS;

}


