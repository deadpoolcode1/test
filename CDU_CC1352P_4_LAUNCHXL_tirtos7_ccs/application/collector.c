/*
 * crs_collector.c
 *
 *  Created on: 30 Mar 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "mac/api_mac.h"
#include "collector.h"
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
#include "crs/crs_env.h"
#include "crs/crs_thresholds.h"
#include "application/crs/crs_alarms.h"
#include "crs/crs_thresholds.h"
#include "agc/agc.h"
#include "application/crs/crs_management.h"
#include "oad/crs_oad.h"
#include "easylink/EasyLink.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* Default MSDU Handle rollover */
#define MSDU_HANDLE_MAX 0x1F

/* App marker in MSDU handle */
#define APP_MARKER_MSDU_HANDLE 0x80
#define TEST_MARKER_MSDU_HANDLE 0x40

/******************************************************************************
 Global variables
 *****************************************************************************/
/* Task pending events */
uint16_t Collector_events = 0;
Cllc_associated_devices_t Cllc_associatedDevList[CRS_GLOBAL_MAX_SENSORS];

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *sem;

/*! Device's PAN ID */

static uint16_t gDeviceShortAddr = 0xaabb;

/*! Device's Outgoing MSDU Handle values */
static uint8_t deviceTxMsduHandle = 0;
static bool gIsTestPacket = false;

/******************************************************************************
 Local function prototypes
 *****************************************************************************/
static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len,
                    uint8_t *pData);
static uint8_t getMsduHandle(Smsgs_cmdIds_t msgType);
//static void processCliUpdateCb(Manage__cbArgs_t *_cbArgs);

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf);
static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd);
static void assocIndCB(ApiMac_mlmeAssociateInd_t *pAssocInd);
static void disassocIndCB(ApiMac_mlmeDisassociateInd_t *pDisassocInd);
static void discoveryIndCB(ApiMac_mlmeDiscoveryInd_t *pDiscoveryInd);
static void updateCduRssiStrct(int8_t rssi, int idx);
static void fpgaCrsStartCallback(const FPGA_cbArgs_t _cbArgs);
//static void fpgaCrsMiddleCallback(const FPGA_cbArgs_t _cbArgs);
static void fpgaCrsDoneCallback(const FPGA_cbArgs_t _cbArgs);

/******************************************************************************
 Callback tables
 *****************************************************************************/

//TODO: add assoc ind cb.
/*! API MAC Callback table */
ApiMac_callbacks_t Collector_macCallbacks = { assocIndCB, disassocIndCB,
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

void Collector_init()
{
    sem = ApiMac_init();
    /* initialize association table */
    memset(Cllc_associatedDevList, 0xFF,
           (sizeof(Cllc_associated_devices_t) * CRS_GLOBAL_MAX_SENSORS));
    /* Register the MAC Callbacks */
    ApiMac_registerCallbacks(&Collector_macCallbacks);
    Nvs_init(sem);
    Env_init();
    Thresh_init();
    SnapInit(sem);
    MultiFiles_multiFilesInit(sem);
    RF_init(sem);
    Config_configInit(sem);
    Fpga_initSem(sem);
    DigInit(sem);
    Tdd_initSem(sem);
    CRS_init();
    Manage_initSem(sem);
    Oad_init(sem);
    Csf_crsInitScript();
//       Agc_init(); ----------->agc init is after you run flat script

}

void Collector_process(void)
{
    if (Collector_events & COLLECTOR_UI_INPUT_EVT)
    {
        CLI_processCliUpdate(NULL, NULL);

        /* Clear the event */
        Util_clearEvent(&Collector_events, COLLECTOR_UI_INPUT_EVT);
    }

    if (Collector_events & COLLECTOR_SEND_MSG_EVT)
    {
        CLI_processCliSendMsgUpdate();

        /* Clear the event */
        Util_clearEvent(&Collector_events, COLLECTOR_SEND_MSG_EVT);
    }
    Config_process();
    MultiFiles_process();
    RF_process();
    Snap_process();
    Fpga_process();
    DIG_process();
    Tdd_process();
    Agc_process();

    Alarms_process();
    Oad_process();
    if (Collector_events == 0)
    {
        ApiMac_processIncoming();
    }
}

void Csf_crsInitScript()
{
//    CRS_LOG(CRS_DEBUG, "Running script");

    CRS_retVal_t retStatus = Fpga_init(fpgaCrsStartCallback);
    if (retStatus != CRS_SUCCESS)
    {
        FPGA_cbArgs_t cbArgs={0};
        CLI_cliPrintf("\r\nUnable to run init script");

        fpgaCrsDoneCallback(cbArgs);
    }
}

void Csf_processCliSendMsgUpdate()
{
    Util_setEvent(&Collector_events, COLLECTOR_SEND_MSG_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sem);
}

void Csf_processCliUpdate()
{
//    Manage_addTask(processCliUpdateCb, NULL);
    Util_setEvent(&Collector_events, COLLECTOR_UI_INPUT_EVT);

    /* Wake up the application thread when it waits for clock event */
    Semaphore_post(sem);
}

bool Collector_isKnownDevice(ApiMac_sAddr_t *pDstAddr)
{
    int x = 0;
    for (x = 0; (x < MAX_DEVICES_IN_NETWORK); x++)
    {
        if (pDstAddr->addr.shortAddr == Cllc_associatedDevList[x].shortAddr)
        {

            return true;

        }

    }
    return false;
}



Collector_status_t Collector_sendCrsMsg(ApiMac_sAddr_t *pDstAddr, uint8_t *line)
{
    Collector_status_t status = Collector_status_invalid_state;
    /* Are we in the right state? */

//        CLI_cliPrintf("\r\nin Collector_sendCrsMsg");
    /* Is the device a known device? */
    if (Collector_isKnownDevice(pDstAddr))
    {
//            CLI_cliPrintf("\r\nin Collector_sendCrsMsg222");

        uint8_t buffer[SMSGS_CRS_MSG_LENGTH + 20] = { 0 };
        buffer[0] = (uint8_t) Smsgs_cmdIds_crsReq;
        if (SMSGS_CRS_MSG_LENGTH < strlen((char *)line))
        {
            memcpy(buffer + 1, line, SMSGS_CRS_MSG_LENGTH - 2);
            if ((sendMsg(Smsgs_cmdIds_crsReq, pDstAddr->addr.shortAddr,

            (SMSGS_CRS_MSG_LENGTH - 2),
                         buffer)) == true)
            {
//                    if (memcmp(line, "rssi", 4) == 0)
//                    {
//                        gIsRssi = true;
//                    }
                status = Collector_status_success;
//                    memcpy(gLastCrsMsg, line, SMSGS_CRS_MSG_LENGTH - 2);
//                    memset(&gCrsLastpDstAddr, 0, sizeof(ApiMac_sAddr_t));
//                    gCrsLastpDstAddr.addrMode = pDstAddr->addrMode;
//                    memcpy(gCrsLastpDstAddr.addr.extAddr,
//                           (pDstAddr->addr).extAddr, 8);
//                    gCrsLastpDstAddr.addr.shortAddr =
//                            (pDstAddr->addr).shortAddr;

            }
            else
            {
                status = Collector_status_deviceNotFound;
            }

        }
        else
        {
            memcpy(buffer + 1, line, strlen((char *)line));
            if ((sendMsg(Smsgs_cmdIds_crsReq, pDstAddr->addr.shortAddr,
                         (strlen((char *)buffer) + 20), buffer)) == true)
            {
//                    if (memcmp(line, "rssi", 4) == 0)
//                    {
//                        gIsRssi = true;
//                    }
                status = Collector_status_success;
//                    memcpy(gLastCrsMsg, line, SMSGS_CRS_MSG_LENGTH);
//
//                    memset(&gCrsLastpDstAddr, 0, sizeof(ApiMac_sAddr_t));
//                    gCrsLastpDstAddr.addrMode = pDstAddr->addrMode;
//                    memcpy(gCrsLastpDstAddr.addr.extAddr,
//                           (pDstAddr->addr).extAddr, 8);
//                    gCrsLastpDstAddr.addr.shortAddr =
//                            (pDstAddr->addr).shortAddr;

            }
            else
            {
                status = Collector_status_deviceNotFound;
            }

        }

    }

//    CLI_cliPrintf("Status in Collector_sendCrsMsg: 0x%x", status);
    return (status);
}

Collector_status_t Collector_oadSendMsg(uint16_t *pDstAddr,
                                        uint8_t *pMsgPayload, uint32_t msgLen)
{
    Collector_status_t status = Collector_status_invalid_state;
    ApiMac_sAddr_t apimac = { 0 };
//    memcpy(apimac.addr.extAddr,oadApiMacExtAddr,sizeof(ApiMac_sAddrExt_t));
    apimac.addr.shortAddr = *pDstAddr;
    /* Are we in the right state? */

//        CLI_cliPrintf("\r\nin Collector_sendCrsMsg");
    /* Is the device a known device? */
    if (Collector_isKnownDevice(&apimac))
    {
//            CLI_cliPrintf("\r\nin Collector_sendCrsMsg222");

        uint8_t buffer[SMSGS_CRS_MSG_LENGTH + 20] = { 0 };
        buffer[0] = (uint8_t) Smsgs_cmdIds_OADsendImgIdentifyReq;
        memcpy(buffer + 1, pMsgPayload, msgLen);
        if ((sendMsg(Smsgs_cmdIds_OADsendImgIdentifyReq, apimac.addr.shortAddr,
                     msgLen + 5, buffer)) == true)
        {
            status = Collector_status_success;
        }
        else
        {
            status = Collector_status_deviceNotFound;
        }
    }
    else
    {
        status = Collector_status_deviceNotFound;
    }

//    CLI_cliPrintf("Status in Collector_sendCrsMsg: 0x%x", status);
    return (status);

}

bool Csf_getNetworkInformation(Llc_netInfo_t *nwkInfo)
{
    nwkInfo->devInfo.panID = CRS_GLOBAL_PAN_ID;
    nwkInfo->devInfo.shortAddress = CRS_GLOBAL_COLLECTOR_SHORT_ADDR;
    EasyLink_getIeeeAddr(nwkInfo->devInfo.extAddress);
    return true;
}

void Csf_sensorsDataPrint(uint16_t shortAddr)
{
    int x = 0;
    uint32_t numSpaces = 5;
    char spacesStr[100] = { 0 };

    memset(spacesStr, ' ', numSpaces);
    /* Clear any timed out transactions */
    for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
    {
        if ((Cllc_associatedDevList[x].shortAddr != CSF_INVALID_SHORT_ADDR)
                && (Cllc_associatedDevList[x].status == 0x2201))
        {
            if (shortAddr == Cllc_associatedDevList[x].shortAddr)
            {
                CLI_cliPrintf("\r\nShortAddr=0x%x Avg=%d Max=%d Min=%d Last=%d",
                              Cllc_associatedDevList[x].shortAddr,
                              Cllc_associatedDevList[x].rssiAvgCru,
                              Cllc_associatedDevList[x].rssiMaxCru,
                              Cllc_associatedDevList[x].rssiMinCru,
                              Cllc_associatedDevList[x].rssiLastCru);

                CLI_cliPrintf(
                        "\r\nShortAddr=0xaabb Avg=%d Max=%d Min=%d Last=%d",
                        Cllc_associatedDevList[x].rssiAvgCdu,
                        Cllc_associatedDevList[x].rssiMaxCdu,
                        Cllc_associatedDevList[x].rssiMinCdu,
                        Cllc_associatedDevList[x].rssiLastCdu);
            }

        }
    }
}

void Cllc_getFfdShortAddr(uint16_t *shortAddr)
{
    *shortAddr = gDeviceShortAddr;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/
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
        FPGA_cbArgs_t cbArgs={0};
        fpgaCrsDoneCallback(cbArgs);
    }

}

//static void fpgaCrsMiddleCallback(const FPGA_cbArgs_t _cbArgs)
//{
//    CRS_retVal_t retStatus = DIG_uploadSnapFpga("TDDModeToTx", MODE_NATIVE,
//    NULL,
//                                                fpgaCrsDoneCallback);
////    CRS_retVal_t retStatus = Config_runConfigFile("flat", fpgaCrsDoneCallback);
//
//    if (retStatus == CRS_FAILURE)
//    {
//        CLI_cliPrintf("\r\nUnable to run TDDModeToTx script");
//        FPGA_cbArgs_t cbArgs;
//        fpgaCrsDoneCallback(cbArgs);
//    }
//
//}

static void fpgaCrsDoneCallback(const FPGA_cbArgs_t _cbArgs)
{
    CLI_startREAD();
    Alarms_init(sem);
    Agc_init(sem);
    Agc_ledEnv();
    Locks_init(sem);
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

static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len,
                    uint8_t *pData)
{
    ApiMac_mcpsDataReq_t dataReq;

    /* Fill the data request field */
    memset(&dataReq, 0, sizeof(ApiMac_mcpsDataReq_t));

    dataReq.dstAddr.addrMode = ApiMac_addrType_short;
    dataReq.dstAddr.addr.shortAddr = dstShortAddr;
    dataReq.srcAddrMode = ApiMac_addrType_short;

    int x = 0;
    for (x = 0; (x < MAX_DEVICES_IN_NETWORK); x++)
    {
        if (dstShortAddr == Cllc_associatedDevList[x].shortAddr)
        {
            dataReq.dstAddr.addrMode = ApiMac_addrType_extended;
            memcpy(dataReq.dstAddr.addr.extAddr,
                   Cllc_associatedDevList[x].extAddr, 8);
            break;
        }

    }

    if (x == MAX_DEVICES_IN_NETWORK)
    {
        return false;
    }

    dataReq.msduHandle = getMsduHandle(type);
    if (type == Smsgs_cmdIds_crsReq)
    {
//        gCrsHandle = dataReq.msduHandle;
    }

//    CLI_cliPrintf("\r\nin send msg: msdu handle:0x%x, type:0x%x", dataReq.msduHandle, type);

    dataReq.msdu.len = len;

    dataReq.msdu.p = pData;
    gIsTestPacket = false;

    ApiMac_status_t status = ApiMac_mcpsDataReq(&dataReq);
    if (status != ApiMac_status_success)
    {
        /*  Transaction overflow occurred */
        return (false);
    }
    else
    {

        return (true);
    }
}

bool Collector_sendCrsMsgTest(uint32_t time)
{

    ApiMac_mcpsDataTest_t dataTest = {0};


    dataTest.msduHandle = getMsduHandle(Smsgs_cmdIds_crsReq);


    //    CLI_cliPrintf("\r\nin send msg: msdu handle:0x%x, type:0x%x", dataReq.msduHandle, type);

    dataTest.time = time;

    gIsTestPacket = true;

    ApiMac_status_t status = ApiMac_mcpsDataTest(&dataTest);
    if (status != ApiMac_status_success)
    {
        /*  Transaction overflow occurred */
        return (false);
    }
    else
    {

        return (true);
    }
}


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


static void assocIndCB(ApiMac_mlmeAssociateInd_t *pAssocInd)
{
//    CLI_cliPrintf("\r\nassocIndCB");
    int x = 0;

    for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
    {
        if (Cllc_associatedDevList[x].shortAddr == pAssocInd->shortAddr)
        {
            memset(&Cllc_associatedDevList[x], 0xff,
                   sizeof(Cllc_associated_devices_t));
            memcpy(Cllc_associatedDevList[x].extAddr, pAssocInd->deviceAddress,
                   8);
            Cllc_associatedDevList[x].shortAddr = pAssocInd->shortAddr;
            Cllc_associatedDevList[x].status = 0x2201;
            return;
        }

    }
    x = 0;
    for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
    {
        if (Cllc_associatedDevList[x].shortAddr == CSF_INVALID_SHORT_ADDR)
        {
            memcpy(Cllc_associatedDevList[x].extAddr, pAssocInd->deviceAddress,
                   8);
            Cllc_associatedDevList[x].shortAddr = pAssocInd->shortAddr;
            Cllc_associatedDevList[x].status = 0x2201;
            return;
        }

    }
}

static void disassocIndCB(ApiMac_mlmeDisassociateInd_t *pDisassocInd)
{
//    CLI_cliPrintf("\r\ndisassocIndCB");
if (oadInProgress) {
    Oad_Reinit();
}
    int x = 0;
    for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
    {
        if (memcmp(pDisassocInd->deviceAddress,
                   Cllc_associatedDevList[x].extAddr, 8) == 0)
        {
            memset(&(Cllc_associatedDevList[x]), 0xff,
                   sizeof(Cllc_associated_devices_t));
            return;
        }

    }
}

static void discoveryIndCB(ApiMac_mlmeDiscoveryInd_t *pDiscoveryInd)
{
//    CLI_cliPrintf("\r\ndiscoveryIndCB");

    int x = 0;
    for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
    {
        if (memcmp(pDiscoveryInd->deviceAddress,
                   Cllc_associatedDevList[x].extAddr, 8) == 0)
        {
            updateCduRssiStrct(pDiscoveryInd->rssi, x);

            Cllc_associatedDevList[x].rssiAvgCru =
                    pDiscoveryInd->rssiRemote.rssiAvg;
            Cllc_associatedDevList[x].rssiLastCru =
                    pDiscoveryInd->rssiRemote.rssiLast;
            Cllc_associatedDevList[x].rssiMaxCru =
                    pDiscoveryInd->rssiRemote.rssiMax;
            Cllc_associatedDevList[x].rssiMinCru =
                    pDiscoveryInd->rssiRemote.rssiMin;

            //TODO update rssi here.
            return;
        }

    }
}

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf)
{
//    CLI_cliPrintf("\r\ndataCnfCB");

    if (gIsTestPacket == true)
    {
        gIsTestPacket = false;
        if (pDataCnf->status != ApiMac_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x14");
            CLI_startREAD();
            return;

        }
        CLI_cliPrintf("\r\nStatus: 0x0");
        CLI_startREAD();
        return;

    }

    if (pDataCnf->status != ApiMac_status_success)
    {
        CLI_cliPrintf("\r\nStatus: 0x14");
        CLI_startREAD();

    }

}

static uint16_t gTotalSmacPackts = 0;
//sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len,
//                    uint8_t *pData)
static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd)
{
//    CLI_cliPrintf("\r\ndataIndCB 0x%x", gTotalSmacPackts);
    gTotalSmacPackts++;

    if ((pDataInd != NULL) && (pDataInd->msdu.p != NULL)
            && (pDataInd->msdu.len > 0))
    {
        Smsgs_cmdIds_t cmdId = (Smsgs_cmdIds_t) *(pDataInd->msdu.p);

        switch (cmdId)
        {
        {
        case Smsgs_cmdIds_crsRsp:
        {
            if (pDataInd->msdu.len > 1)
            {
                CLI_cliPrintf("%s", pDataInd->msdu.p + 1);
            }

            CLI_startREAD();

            break;
        }
        case Smsgs_cmdIds_OADsendImgIdentifyRsp:
        {
       //            memcpy(oadApiMacExtAddr,pDataInd->srcAddr.addr.extAddr,sizeof(ApiMac_sAddrExt_t));
                   Oad_parseOadPkt((pDataInd->msdu.p) + 3);
       //            OADProtocol_ParseIncoming(&pDataInd->srcAddr.addr.shortAddr, (pDataInd->msdu.p) + 3);
       //            CLI_startREAD();
                   break;
        }
        case Smsgs_cmdIds_crsRspInParts:
        {
                    if (pDataInd->msdu.len > 1)
                    {
                        CLI_cliPrintf("%s", pDataInd->msdu.p + 1);
                    }
                    uint8_t msg[10] = {0};
                    msg[0] = Smsgs_cmdIds_crsReqInParts;
                    sendMsg(Smsgs_cmdIds_crsReqInParts, pDataInd->srcShortAddr, 2, msg);

                    break;

        }
        default:
        {
            /* Should not receive other messages */
            break;
        }
        }
        }
    }
    else
    {
        CLI_cliPrintf("\r\ndataIndCB 0x%x", gTotalSmacPackts);
    }
}

static void updateCduRssiStrct(int8_t rssi, int idx)
{
    //    CRS_LOG(CRS_DEBUG, "START");
    if (Cllc_associatedDevList[idx].rssiArrIdx > RSSI_ARR_SIZE)
    {
        Cllc_associatedDevList[idx].rssiArrIdx = 0;
    }
    Cllc_associatedDevList[idx].rssiArr[Cllc_associatedDevList[idx].rssiArrIdx] =
            rssi;

    if (Cllc_associatedDevList[idx].rssiArrIdx + 1 < RSSI_ARR_SIZE)
    {
        Cllc_associatedDevList[idx].rssiArrIdx += 1;
    }
    else if (Cllc_associatedDevList[idx].rssiArrIdx + 1 == RSSI_ARR_SIZE)
    {
        Cllc_associatedDevList[idx].rssiArrIdx = 0;
    }

    int i = 0;
    int16_t sum = 0;
    uint8_t numZeros = 0;
    for (i = 0; i < RSSI_ARR_SIZE; i++)
    {
        if (Cllc_associatedDevList[idx].rssiArr[i] == -1)
        {
            numZeros++;
        }
        else
        {
            sum += Cllc_associatedDevList[idx].rssiArr[i];
        }
    }

    Cllc_associatedDevList[idx].rssiAvgCdu = sum / (RSSI_ARR_SIZE - numZeros);

    if (Cllc_associatedDevList[idx].rssiMaxCdu == -1)
    {
        Cllc_associatedDevList[idx].rssiMaxCdu = rssi;
    }
    if (Cllc_associatedDevList[idx].rssiMinCdu == -1)
    {
        Cllc_associatedDevList[idx].rssiMinCdu = rssi;
    }
    Cllc_associatedDevList[idx].rssiLastCdu = rssi;

    int x = 0;

    for (x = 0; x < RSSI_ARR_SIZE; x++)
    {
        if (Cllc_associatedDevList[idx].rssiArr[x] != -1
                && Cllc_associatedDevList[idx].rssiMaxCdu
                        < Cllc_associatedDevList[idx].rssiArr[x])
        {
            Cllc_associatedDevList[idx].rssiMaxCdu =
                    Cllc_associatedDevList[idx].rssiArr[x];
        }

        if (Cllc_associatedDevList[idx].rssiArr[x] != -1
                && Cllc_associatedDevList[idx].rssiMinCdu
                        > Cllc_associatedDevList[idx].rssiArr[x])
        {
            Cllc_associatedDevList[idx].rssiMinCdu =
                    Cllc_associatedDevList[idx].rssiArr[x];
        }
    }
}
