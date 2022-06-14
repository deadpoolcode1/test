/*
 * sensor.c
 *
 *  Created on: 14 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mac/api_mac.h"
#include "sensor.h"
#include "smsgs.h"
#include "mac/api_mac.h"
#include "crs/crs_cli.h"
#include "crs/crs_fpga.h"
#include "crs/crs_nvs.h"
#include "application/crs/snapshots/crs_snapshot.h"
#include "application/crs/snapshots/config_parsing.h"
#include "application/crs/snapshots/crs_multi_snapshots.h"
#include "application/crs/snapshots/crs_snap_rf.h"
#include "application/crs/snapshots/crs_script_dig.h"
#include "crs/crs_tdd.h"
#include "agc/agc.h"
#include "crs/crs_thresholds.h"
#include "easylink/EasyLink.h"
#include "application/crs/crs_alarms.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* Default MSDU Handle rollover */
#define MSDU_HANDLE_MAX 0x1F

/* App marker in MSDU handle */
#define APP_MARKER_MSDU_HANDLE 0x80
#define RSSI_ARR_SIZE 10

typedef struct _Sensor_rssi_t
{
    int8_t rssiArr[RSSI_ARR_SIZE];
    uint32_t rssiArrIdx;
    int8_t rssiMax;
    int8_t rssiMin;
    int8_t rssiAvg;
    int8_t rssiLast;

} Sensor_rssi_t;

/******************************************************************************
 Global variables
 *****************************************************************************/
/* Task pending events */
uint16_t Sensor_events = 0;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *sem;

static Sensor_rssi_t gRssiStrct = { 0 };

static Llc_netInfo_t parentInfo = { 0 };

static bool isConnected = false;
/*! Device's PAN ID */
static uint16_t devicePanId = 0xFFFF;

static uint16_t gDeviceShortAddr = 0xaabb;

/*! Device's Outgoing MSDU Handle values */
static uint8_t deviceTxMsduHandle = 0;

static ApiMac_deviceDescriptor_t gDevInfo = { 0 };
static Llc_netInfo_t gParentInfo = { 0 };

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len,
                    uint8_t *pData);
static uint8_t getMsduHandle(Smsgs_cmdIds_t msgType);

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf);
static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd);
static void assocIndCB(ApiMac_mlmeAssociateInd_t *pAssocInd);
static void disassocIndCB(ApiMac_mlmeDisassociateInd_t *pDisassocInd);
static void discoveryIndCB(ApiMac_mlmeDiscoveryInd_t *pDiscoveryInd);

static void processCrsRequest(ApiMac_mcpsDataInd_t *pDataInd);
static bool Sensor_sendMsg(Smsgs_cmdIds_t type, ApiMac_sAddr_t *pDstAddr,
                           uint16_t len, uint8_t *pData);

static void updateRssiStrct(int8_t rssi);
static void fpgaCrsStartCallback(const FPGA_cbArgs_t _cbArgs);
static void fpgaCrsMiddleCallback(const FPGA_cbArgs_t _cbArgs);
static void fpgaCrsDoneCallback(const FPGA_cbArgs_t _cbArgs);
/******************************************************************************
 Callback tables
 *****************************************************************************/

//TODO: add assoc ind cb.
/*! API MAC Callback table */
static ApiMac_callbacks_t Sensor_macCallbacks = { assocIndCB, disassocIndCB,
                                                  discoveryIndCB,

                                                  /*! Start Confirmation callback */
                                                  NULL,

                                                  /*! Data Confirmation callback */
                                                  dataCnfCB,
                                                  /*! Data Indication callback */
                                                  dataIndCB

};

/******************************************************************************
 Public Functions
 *****************************************************************************/

void Sensor_init(PIN_Handle pinHandl)
{
    sem = ApiMac_init();

    EasyLink_getIeeeAddr(gDevInfo.extAddress);

    gRssiStrct.rssiArrIdx = 0;
    gRssiStrct.rssiMax = 0;
    gRssiStrct.rssiMin = 0;
    gRssiStrct.rssiLast = 0;

    gDevInfo.panID = CRS_GLOBAL_PAN_ID;
//    gDevInfo.extAddress

    /* Register the MAC Callbacks */
    ApiMac_registerCallbacks(&Sensor_macCallbacks);

    Nvs_init(sem);
    Thresh_init();
    SnapInit(sem);
    //    Fs_init(sem);
    MultiFiles_multiFilesInit(sem);
    RF_init(sem);
    Config_configInit(sem);
    Fpga_initSem(sem);
    //    AGCinit(sem);
    DigInit(sem);
    Tdd_initSem(sem);
    CRS_init(pinHandl);
    Agc_init();
    Ssf_crsInitScript();
}

void Sensor_process(void)
{

    if (Sensor_events & SENSOR_UI_INPUT_EVT)
    {
        CLI_processCliUpdate(NULL, NULL);

        /* Clear the event */
        Util_clearEvent(&Sensor_events, SENSOR_UI_INPUT_EVT);
    }

    if (Sensor_events & SENSOR_START_EVT)
    {
//        CLI_processCliSendMsgUpdate();

        /* Clear the event */
        Util_clearEvent(&Sensor_events, SENSOR_START_EVT);
    }

    Config_process();
    MultiFiles_process();
    RF_process();
    Snap_process();
    Fpga_process();
    DIG_process();
    Tdd_process();
    Alarms_process();
    if (Sensor_events == 0)
    {
        ApiMac_processIncoming();
    }
}

void Ssf_crsInitScript()
{
//    CRS_LOG(CRS_DEBUG, "Running script");

    CRS_retVal_t retStatus = Fpga_init(fpgaCrsStartCallback);
    if (retStatus != CRS_SUCCESS)
    {
        FPGA_cbArgs_t cbArgs;
        CLI_cliPrintf("\r\nUnable to run init script");

        fpgaCrsDoneCallback(cbArgs);
    }
}

static void fpgaCrsStartCallback(const FPGA_cbArgs_t _cbArgs)
{
//    CRS_retVal_t retStatus = DIG_uploadSnapFpga("TDDModeToTx", MODE_NATIVE, NULL, fpgaCrsDoneCallback);
    if (Fpga_isOpen() == CRS_SUCCESS)
    {
        CRS_retVal_t retStatus = Config_runConfigFile("flat",
                                                      fpgaCrsDoneCallback);

    }
    else
    {
        CLI_cliPrintf("\r\nUnable to run flat file");
        FPGA_cbArgs_t cbArgs;
        fpgaCrsDoneCallback(cbArgs);
    }

}

static void fpgaCrsMiddleCallback(const FPGA_cbArgs_t _cbArgs)
{
    CRS_retVal_t retStatus = DIG_uploadSnapFpga("TDDModeToTx", MODE_NATIVE,
    NULL,
                                                fpgaCrsDoneCallback);
//    CRS_retVal_t retStatus = Config_runConfigFile("flat", fpgaCrsDoneCallback);

    if (retStatus == CRS_FAILURE)
    {
        CLI_cliPrintf("\r\nUnable to run TDDModeToTx script");
        FPGA_cbArgs_t cbArgs;
        fpgaCrsDoneCallback(cbArgs);
    }

}

static void fpgaCrsDoneCallback(const FPGA_cbArgs_t _cbArgs)
{
    CLI_startREAD();
    Alarms_init(sem);

//    if (CONFIG_AUTO_START)
//    {
//        CLI_cliPrintf("\r\nCollector\r\nForming nwk...");
//
//        /* Start the device */
//        Util_setEvent(&Collector_events, COLLECTOR_START_EVT);
//        Semaphore_post(collectorSem);
//
//    }
}
void Ssf_processCliUpdate()
{
    Util_setEvent(&Sensor_events, SENSOR_UI_INPUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sem);
}

Sensor_status_t Sensor_sendCrsRsp(ApiMac_sAddr_t *pDstAddr, uint8_t *pMsg)
{
    Smsgs_statusValues_t stat = Smsgs_statusValues_invalid;
    uint8_t crsRsp[SMSGS_CRS_MSG_LENGTH] = { 0 };
    memcpy(crsRsp + 1, pMsg, SMSGS_CRS_MSG_LENGTH - 2);

//    if ((Jdllc_getProvState() == Jdllc_states_joined)
//            || (Jdllc_getProvState() == Jdllc_states_rejoined))
    {
        /* send the response message directly */
        crsRsp[0] = (uint8_t) Smsgs_cmdIds_crsRsp;

        Sensor_sendMsg(Smsgs_cmdIds_crsRsp, pDstAddr, strlen(crsRsp) + 2,
                       crsRsp);

    }

    stat = Smsgs_statusValues_success;

}

static bool Sensor_sendMsg(Smsgs_cmdIds_t type, ApiMac_sAddr_t *pDstAddr,
                           uint16_t len, uint8_t *pData)
{
    bool ret = false;
    /* information about the network */
    ApiMac_mcpsDataReq_t dataReq;

    /* Construct the data request field */
    memset(&dataReq, 0, sizeof(ApiMac_mcpsDataReq_t));
    memcpy(&dataReq.dstAddr, pDstAddr, sizeof(ApiMac_sAddr_t));

    /* set the correct address mode. */
    if (pDstAddr->addrMode == ApiMac_addrType_extended)
    {
        dataReq.srcAddrMode = ApiMac_addrType_extended;
    }
    else
    {
        dataReq.srcAddrMode = ApiMac_addrType_short;
    }

    dataReq.msduHandle = getMsduHandle(type);

    dataReq.msdu.len = len;
    dataReq.msdu.p = pData;

    /* Send the message */
    if (ApiMac_mcpsDataReq(&dataReq) == ApiMac_status_success)
    {
        ret = true;
    }

    return (ret);
}

/*!
 * @brief      Get the next MSDU Handle
 *             <BR>
 *             The MSDU handle has 3 parts:<BR>
 *             - The MSBit(7), when set means the the application sent the message
 *             - Bit 6, when set means that the app message is a config request
 *             - Bits 0-5, used as a message counter that rolls over.
 *
 * @param      msgType - message command id needed
 *
 * @return     msdu Handle
 */
static uint8_t getMsduHandle(Smsgs_cmdIds_t msgType)
{
    uint8_t msduHandle = deviceTxMsduHandle;

    /* Increment for the next msdu handle, or roll over */
    if (deviceTxMsduHandle >= MSDU_HANDLE_MAX)
    {
        deviceTxMsduHandle = 0;
    }
    else
    {
        deviceTxMsduHandle++;
    }

    /* Add the App specific bit */
    msduHandle |= APP_MARKER_MSDU_HANDLE;

    return (msduHandle);
}

extern bool Ssf_getNetworkInfo(ApiMac_deviceDescriptor_t *pDevInfo,
                               Llc_netInfo_t *pParentInfo)
{
    if (isConnected == false)
    {
        return false;
    }
    memcpy(pDevInfo, &gDevInfo, sizeof(ApiMac_deviceDescriptor_t));
    memcpy(pParentInfo, &gParentInfo, sizeof(Llc_netInfo_t));
    return true;

//    pDevInfo->extAddress
}

static void assocIndCB(ApiMac_mlmeAssociateInd_t *pAssocInd)
{
//        CLI_cliPrintf("\r\nassocIndCB");

    isConnected = true;
    memcpy(gParentInfo.devInfo.extAddress, pAssocInd->deviceAddress, 8);
    gParentInfo.devInfo.shortAddress = pAssocInd->shortAddr;
    gParentInfo.devInfo.panID = CRS_GLOBAL_PAN_ID;

}

static void disassocIndCB(ApiMac_mlmeDisassociateInd_t *pDisassocInd)
{
//        CLI_cliPrintf("\r\ndisassocIndCB");

    isConnected = false;

}

static void discoveryIndCB(ApiMac_mlmeDiscoveryInd_t *pDiscoveryInd)
{
        CLI_cliPrintf("\r\ndiscoveryIndCB: 0x%x", pDiscoveryInd->isLocked);

//    updateRssiStrct(pDiscoveryInd->rssi);
}

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf)
{

}

static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd)
{
    if ((pDataInd != NULL) && (pDataInd->msdu.p != NULL)
            && (pDataInd->msdu.len > 0))
    {
        Smsgs_cmdIds_t cmdId = (Smsgs_cmdIds_t) *(pDataInd->msdu.p);

        switch (cmdId)
        {
        case Smsgs_cmdIds_crsReq:
            //                CRS_LOG(CRS_DEBUG,"got Smsgs_cmdIds_crsReq");

            processCrsRequest(pDataInd);
            break;

        default:
            /* Should not receive other messages */
            break;
        }
    }
}

static void processCrsRequest(ApiMac_mcpsDataInd_t *pDataInd)
{
    CLI_processCliUpdate((pDataInd->msdu.p) + 1, &pDataInd->srcAddr);
}

static void updateRssiStrct(int8_t rssi)
{
//    CRS_LOG(CRS_DEBUG, "START");
    gRssiStrct.rssiArr[gRssiStrct.rssiArrIdx] = rssi;

    if (gRssiStrct.rssiArrIdx + 1 < RSSI_ARR_SIZE)
    {
        gRssiStrct.rssiArrIdx += 1;
    }
    else if (gRssiStrct.rssiArrIdx + 1 == RSSI_ARR_SIZE)
    {
        gRssiStrct.rssiArrIdx = 0;
    }

    int i = 0;
    int16_t sum = 0;
    uint8_t numZeros = 0;
    for (i = 0; i < RSSI_ARR_SIZE; i++)
    {
        sum += gRssiStrct.rssiArr[i];
        if (gRssiStrct.rssiArr[i] == 0)
        {
            numZeros++;
        }
    }

    gRssiStrct.rssiAvg = sum / (RSSI_ARR_SIZE - numZeros);

    if (gRssiStrct.rssiMax == 0)
    {
        gRssiStrct.rssiMax = rssi;
    }
    if (gRssiStrct.rssiMin == 0)
    {
        gRssiStrct.rssiMin = rssi;
    }
    gRssiStrct.rssiLast = rssi;

    int x = 0;

    for (x = 0; x < RSSI_ARR_SIZE; x++)
    {
        if (gRssiStrct.rssiArr[x] != 0
                && gRssiStrct.rssiMax > gRssiStrct.rssiArr[x])
        {
            gRssiStrct.rssiMax = gRssiStrct.rssiArr[x];
        }

        if (gRssiStrct.rssiArr[x] != 0
                && gRssiStrct.rssiMin < gRssiStrct.rssiArr[x])
        {
            gRssiStrct.rssiMin = gRssiStrct.rssiArr[x];
        }
    }

}

