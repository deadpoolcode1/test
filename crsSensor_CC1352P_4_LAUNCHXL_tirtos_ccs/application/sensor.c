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

#include "api_mac.h"
#include "sensor.h"
#include "smsgs.h"
#include "mac/api_mac.h"
#include "crs/crs_cli.h"
#include "crs/crs_fpga.h"
#include "crs/crs_nvs.h"
#include "crs_snapshot.h"
#include "config_parsing.h"
#include "crs_multi_snapshots.h"
#include "crs_snap_rf.h"
#include "crs_script_dig.h"
#include "crs/crs_tdd.h"
//#include "agc/agc.h"
#include "crs/crs_thresholds.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/* Default MSDU Handle rollover */
#define MSDU_HANDLE_MAX 0x1F

/* App marker in MSDU handle */
#define APP_MARKER_MSDU_HANDLE 0x80

/******************************************************************************
 Global variables
 *****************************************************************************/
/* Task pending events */
uint16_t Sensor_events = 0;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *sem;

static Llc_netInfo_t parentInfo = { 0 };

/*! Device's PAN ID */
static uint16_t devicePanId = 0xFFFF;

static uint16_t gDeviceShortAddr = 0xaabb;

/*! Device's Outgoing MSDU Handle values */
static uint8_t deviceTxMsduHandle = 0;

static ApiMac_deviceDescriptor_t gDevInfo = {0};
static Llc_netInfo_t gParentInfo = {0};

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len,
                    uint8_t *pData);
static uint8_t getMsduHandle(Smsgs_cmdIds_t msgType);

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf);
static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd);
static void processCrsRequest(ApiMac_mcpsDataInd_t *pDataInd);
static bool Sensor_sendMsg(Smsgs_cmdIds_t type, ApiMac_sAddr_t *pDstAddr, uint16_t len,
                    uint8_t *pData);
/******************************************************************************
 Callback tables
 *****************************************************************************/

//TODO: add assoc ind cb.
/*! API MAC Callback table */
static ApiMac_callbacks_t Sensor_macCallbacks = {

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

void Sensor_init()
{
    sem = ApiMac_init();

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
    CRS_init();
//       Agc_init();

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
    if (Sensor_events == 0)
    {
        ApiMac_processIncoming();
    }
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

static bool Sensor_sendMsg(Smsgs_cmdIds_t type, ApiMac_sAddr_t *pDstAddr, uint16_t len,
                    uint8_t *pData)
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
//    pDevInfo->extAddress
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

