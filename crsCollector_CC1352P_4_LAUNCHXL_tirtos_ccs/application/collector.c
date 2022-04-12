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

#include "collector.h"
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
uint16_t Collector_events = 0;

/******************************************************************************
 Local variables
 *****************************************************************************/

static void *sem;


/*! Device's PAN ID */
static uint16_t devicePanId = 0xFFFF;

static uint16_t gDeviceShortAddr = 0xaabb;


/*! Device's Outgoing MSDU Handle values */
static uint8_t deviceTxMsduHandle = 0;

Cllc_associated_devices_t Cllc_associatedDevList[4];


/******************************************************************************
 Local function prototypes
 *****************************************************************************/
static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len, uint8_t *pData);
static uint8_t getMsduHandle(Smsgs_cmdIds_t msgType);

static void dataCnfCB(ApiMac_mcpsDataCnf_t *pDataCnf);
static void dataIndCB(ApiMac_mcpsDataInd_t *pDataInd);

/******************************************************************************
 Callback tables
 *****************************************************************************/
//TODO: add assoc ind cb.
/*! API MAC Callback table */
ApiMac_callbacks_t Collector_macCallbacks = {

                                              /*! Start Confirmation callback */
                                              NULL,

                                              /*! Data Confirmation callback */
                                              dataCnfCB,
                                              /*! Data Indication callback */
                                              dataIndCB

                                              };

void Collector_init()
{
    sem = ApiMac_init();
    /* initialize association table */
    memset(Cllc_associatedDevList, 0xFF,
           (sizeof(Cllc_associated_devices_t) * 4));
    /* Register the MAC Callbacks */
    ApiMac_registerCallbacks(&Collector_macCallbacks);

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

void Collector_process(void)
{
    Config_process();
      MultiFiles_process();
      RF_process();
      Snap_process();
      Fpga_process();
      DIG_process();
      Tdd_process();
}

bool Collector_isKnownDevice(ApiMac_sAddr_t *pDstAddr)
{

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
            if (SMSGS_CRS_MSG_LENGTH < strlen(line))
            {
                memcpy(buffer + 1, line, SMSGS_CRS_MSG_LENGTH - 2);
                if ((sendMsg(Smsgs_cmdIds_crsReq, pDstAddr->addr.shortAddr,

                             (SMSGS_CRS_MSG_LENGTH - 2), buffer)) == true)
                {
//                    if (memcmp(line, "rssi", 4) == 0)
//                    {
//                        gIsRssi = true;
//                    }
//                    status = Collector_status_success;
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
                memcpy(buffer + 1, line, strlen(line));
                if ((sendMsg(Smsgs_cmdIds_crsReq, pDstAddr->addr.shortAddr,
                             (strlen(buffer) + 20),
                             buffer)) == true)
                {
//                    if (memcmp(line, "rssi", 4) == 0)
//                    {
//                        gIsRssi = true;
//                    }
//                    status = Collector_status_success;
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


static bool sendMsg(Smsgs_cmdIds_t type, uint16_t dstShortAddr, uint16_t len, uint8_t *pData)
{
    static uint8_t map_index = 0;
    ApiMac_mcpsDataReq_t dataReq;

    /* Fill the data request field */
    memset(&dataReq, 0, sizeof(ApiMac_mcpsDataReq_t));

    dataReq.dstAddr.addrMode = ApiMac_addrType_short;
    dataReq.dstAddr.addr.shortAddr = dstShortAddr;
    dataReq.srcAddrMode = ApiMac_addrType_short;




    dataReq.msduHandle = getMsduHandle(type);
    if (type == Smsgs_cmdIds_crsReq )
    {
//        gCrsHandle = dataReq.msduHandle;
    }

//    CLI_cliPrintf("\r\nin send msg: msdu handle:0x%x, type:0x%x", dataReq.msduHandle, type);


    dataReq.msdu.len = len;
    dataReq.msdu.p = pData;



    ApiMac_status_t status = ApiMac_mcpsDataReq(&dataReq);
    if (status != ApiMac_status_success)
    {
        /*  Transaction overflow occurred */
        return (false);
    }
    else
    {
//        /* Structure is used to track data request handles to help remove unresponsive devices from macSecurity table in FH mode */
//        dataRequestMsduMappingTable[map_index % MAX_DATA_REQ_MSDU_MAP_TABLE_SIZE].dstAddr =
//                dataReq.dstAddr;
//        dataRequestMsduMappingTable[map_index % MAX_DATA_REQ_MSDU_MAP_TABLE_SIZE].msduHandle =
//                dataReq.msduHandle;
//        map_index++;
        return (true);
    }
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

void Cllc_getFfdShortAddr(uint16_t* shortAddr)
{
    *shortAddr = gDeviceShortAddr;
}




