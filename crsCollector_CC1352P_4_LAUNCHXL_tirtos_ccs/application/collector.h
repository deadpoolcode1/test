/*
 * collector.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_COLLECTOR_H_
#define APPLICATION_COLLECTOR_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include "mac/api_mac.h"

/*! Event ID - Start the device in the network */
#define COLLECTOR_START_EVT               0x0001
#define COLLECTOR_UI_INPUT_EVT               0x0002
#define COLLECTOR_SEND_MSG_EVT 0x0004
//COLLECTOR_UI_INPUT_EVT

#define RSSI_ARR_SIZE 10
/*! Building block for association table */
typedef struct
{
    /*! Short address of associated device */
    uint16_t shortAddr;
    ApiMac_sAddrExt_t extAddr;

    /*! RSSI */
    int8_t rssi;
    /*! Device alive status */
    uint16_t status;

    int8_t rssiArr[RSSI_ARR_SIZE];

    uint32_t rssiArrIdx;
    /*! RSSI */
    int8_t rssiAvgCdu;
    /*! RSSI */
    int8_t rssiMinCdu;
    /*! RSSI */
    int8_t rssiMaxCdu;

    int8_t rssiLastCdu;

    /*! RSSI */
    int8_t rssiAvgCru;
    /*! RSSI */
    int8_t rssiMinCru;
    /*! RSSI */
    int8_t rssiMaxCru;
    /*! RSSI */
    int8_t rssiLastCru;

} Cllc_associated_devices_t;

/*! Network Information */
typedef struct
{
    /* Address information */
    ApiMac_deviceDescriptor_t devInfo;

} Llc_netInfo_t;

/*! Collector Status Values */
typedef enum
{
    /*! Success */
    Collector_status_success = 0,
    /*! Device Not Found */
    Collector_status_deviceNotFound = 1,
    /*! Collector isn't in the correct state to send a message */
    Collector_status_invalid_state = 2
} Collector_status_t;

/*! Collector Statistics */
typedef struct
{
    /*!
     Total number of tracking request messages attempted
     */
    uint32_t trackingRequestAttempts;
    /*!
     Total number of tracking request messages sent
     */
    uint32_t trackingReqRequestSent;
    /*!
     Total number of tracking response messages received
     */
    uint32_t trackingResponseReceived;
    /*!
     Total number of config request messages attempted
     */
    uint32_t configRequestAttempts;
    /*!
     Total number of config request messages sent
     */
    uint32_t configReqRequestSent;
    /*!
     Total number of config response messages received
     */
    uint32_t configResponseReceived;
    /*!
     Total number of sensor messages received
     */
    uint32_t sensorMessagesReceived;
    /*!
     Total number of failed messages because of channel access failure
     */
    uint32_t channelAccessFailures;
    /*!
     Total number of failed messages because of ACKs not received
     */
    uint32_t ackFailures;
    /*!
     Total number of failed transmit messages that are not channel access
     failure and not ACK failures
     */
    uint32_t otherTxFailures;
    /*! Total number of RX Decrypt failures. */
    uint32_t rxDecryptFailures;
    /*! Total number of TX Encrypt failures. */
    uint32_t txEncryptFailures;
    /* Total Transaction Expired Count */
    uint32_t txTransactionExpired;
    /* Total transaction Overflow error */
    uint32_t txTransactionOverflow;
    /* Total broadcast messages sent */
    uint16_t broadcastMsgSentCnt;
} Collector_statistics_t;

/*! Collector events flags */
extern uint16_t Collector_events;

/*! Collector statistics */
extern Collector_statistics_t Collector_statistics;

extern ApiMac_callbacks_t Collector_macCallbacks;

extern Cllc_associated_devices_t Cllc_associatedDevList[4];

extern void Collector_init();

extern void Collector_process(void);

void Csf_processCliUpdate();
void Csf_processCliSendMsgUpdate();


extern Collector_status_t Collector_sendCrsMsg(ApiMac_sAddr_t *pDstAddr, uint8_t* line);

bool Collector_isKnownDevice(ApiMac_sAddr_t *pDstAddr);

void Cllc_getFfdShortAddr(uint16_t* shortAddr);

#endif /* APPLICATION_COLLECTOR_H_ */
