/*
 * macTask.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_MACTASK_H_
#define MAC_MACTASK_H_

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "crs_global_defines.h"
#include "application/crs/crs_cli.h"
#include "easylink/EasyLink.h"
//#include <ti/drivers/PIN.h>

#define MAC_TASK_CLI_UPDATE_EVT 0x0001
#define MAC_TASK_TX_DONE_EVT 0x0002
#define MAC_TASK_RX_DONE_EVT 0x0004
#define MAC_TASK_NODE_TIMEOUT_EVT 0x0008

#define CRS_PAN_ID 0x11

#define CRS_PAYLOAD_MAX_SIZE 1000
#define CRS_PKT_MAX_SIZE 150

#define CRS_MAX_PKT_RETRY 5

#define CRS_BEACON_INTERVAL 5

typedef enum
{
    MAC_COMMAND_DATA, MAC_COMMAND_ACK, MAC_COMMAND_BEACON, MAC_COMMAND_ASSOC_REQ, MAC_COMMAND_ASSOC_RSP, MAC_COMMAND_DISCOVERY, MAC_COMMAND_TEST
} MAC_commandId_t;

typedef struct Frame
{
    //payload len
    MAC_commandId_t commandId;
    uint16_t len;
    uint16_t seqSent;
    uint16_t seqRcv;
    uint8_t srcAddr[8];
    uint8_t dstAddr[8];
    uint16_t srcAddrShort;
    uint16_t dstAddrShort;
    uint8_t panId;
    uint8_t isNeedAck;
    uint8_t payload[CRS_PAYLOAD_MAX_SIZE];
} MAC_crsPacket_t;

typedef struct FrameBeacon
{

    MAC_commandId_t commandId;
    uint8_t srcAddr[8];
    uint16_t srcAddrShort;
    uint8_t panId;
    uint32_t discoveryTime;

} MAC_crsBeaconPacket_t;

typedef struct FrameAsocReq
{

    MAC_commandId_t commandId;
    uint8_t srcAddr[8];
    uint16_t srcAddrShort;
    uint8_t panId;

} MAC_crsAssocReqPacket_t;

typedef struct FrameAsocRsp
{

    MAC_commandId_t commandId;
    uint8_t srcAddr[8];
    uint16_t srcAddrShort;
    uint8_t panId;
    uint16_t dstShortAddr;
    bool isPremited;

} MAC_crsAssocRspPacket_t;

typedef struct collectorNode
{
    ApiMac_sAddrExt_t mac;
    uint8_t panId;
    uint16_t shortAddr;
} MAC_collectorInfo_t;

extern MAC_collectorInfo_t collectorPib;
extern uint16_t macEvents;

void Mac_init();

void Mac_cliUpdate();
void MAC_printSensorStateMachine();

void Mac_cliSendContent(uint8_t mac[8]);

bool MAC_createDataCnf(macMcpsDataCnf_t *rsp, uint8_t msduHandle,
                       ApiMac_status_t status);
bool MAC_sendCnfToApp(macMcpsDataCnf_t *dataCnf);
bool MAC_createDataInd(macMcpsDataInd_t *rsp, MAC_crsPacket_t *pkt,
                       ApiMac_status_t status);
bool MAC_sendDataIndToApp(macMcpsDataInd_t *dataCnf);

void Smri_recivedPcktCb(EasyLink_RxPacket *rxPacket,
                                  EasyLink_Status status);

void MAC_moveToRxIdleState();

bool MAC_createAssocInd(macMlmeAssociateInd_t *rsp, sAddrExt_t deviceAddress, uint16_t shortAddr,
                              ApiMac_status_t status);

bool MAC_sendAssocIndToApp(macMlmeAssociateInd_t *dataCnf);

bool MAC_createDisssocInd(macMlmeDisassociateInd_t *rsp, sAddrExt_t deviceAddress);

bool MAC_sendDisassocIndToApp(macMlmeDisassociateInd_t *dataCnf);

void MAC_moveToBeaconState();

bool MAC_sendDiscoveryIndToApp(macMlmeDiscoveryInd_t *dataCnf);

bool MAC_createDiscovryInd(macMlmeDiscoveryInd_t *rsp, int8_t rssi, int8_t rssiMaxRemote, int8_t rssiMinRemote,
                           int8_t rssiAvgRemote,  int8_t rssiLastRemote, sAddrExt_t deviceAddress);

bool MAC_getDeviceShortAddr(uint8_t * mac, uint16_t* shortAddr);

bool MAC_getDeviceLongAddr(uint16_t shortAddr, uint8_t* mac);



#endif /* MAC_MACTASK_H_ */
