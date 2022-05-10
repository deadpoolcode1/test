/*
 * smsgs.h
 *
 *  Created on: 11 Apr 2022
 *      Author: avi
 */

#ifndef APPLICATION_SMSGS_H_
#define APPLICATION_SMSGS_H_

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define SMSGS_CRS_MSG_LENGTH 800
#define SMSGS_CRS_DISC_MSG_LENGTH 100

#define SMSGS_CRS_ALARMS_MSG_LENGTH 100

/*!
 Message IDs for Sensor data messages.  When sent over-the-air in a message,
 this field is one byte.
 */
typedef enum
{
    /*! Configuration message, sent from the collector to the sensor */
    Smsgs_cmdIds_configReq = 1,
    /*! Configuration Response message, sent from the sensor to the collector */
    Smsgs_cmdIds_configRsp = 2,
    /*! Tracking request message, sent from the the collector to the sensor */
    Smsgs_cmdIds_trackingReq = 3,
    /*! Tracking response message, sent from the sensor to the collector */
    Smsgs_cmdIds_trackingRsp = 4,
    /*! Sensor data message, sent from the sensor to the collector */
    Smsgs_cmdIds_sensorData = 5,
    /* Toggle LED message, sent from the collector to the sensor */
    Smsgs_cmdIds_toggleLedReq = 6,
    /* Toggle LED response msg, sent from the sensor to the collector */
    Smsgs_cmdIds_toggleLedRsp = 7,
    /* new data type for ramp data */
    Smsgs_cmdIds_rampdata = 8,
    /*! OAD mesages, sent/received from both collector and sensor */
    Smsgs_cmdIds_oad = 9,
    /* Broadcast control msg, sent from the collector to the sensor */
    Smgs_cmdIds_broadcastCtrlMsg = 10,
    /* KEY Exchange msg, between collector and the sensor */
    Smgs_cmdIds_KeyExchangeMsg = 11,
    /* Identify LED request msg */
    Smsgs_cmdIds_IdentifyLedReq = 12,
    /* Identify LED response msg */
    Smsgs_cmdIds_IdentifyLedRsp = 13,
    /*! SM Commissioning start command sent from collector to the sensor */
    Smgs_cmdIds_CommissionStart = 14,
    /*! SM Commissioning message sent bi-directionally between the collector and sensor */
    Smgs_cmdIds_CommissionMsg = 15,
    /* Device type request msg */
    Smsgs_cmdIds_DeviceTypeReq = 16,
    /* Device type response msg */
    Smsgs_cmdIds_DeviceTypeRsp = 17,
    /* Send ping req message, sent from the collector to the sensor */
    Smsgs_cmdIds_pingReq = 18,
    /* Send Ping res response msg, sent from the sensor to the collector */
    Smsgs_cmdIds_pingRsp = 19,
    /* Send ping req message, sent from the collector to the sensor */
    Smsgs_cmdIds_xrReq = 20,
    /* Send Ping res response msg, sent from the sensor to the collector */
    Smsgs_cmdIds_xrRsp = 21,
    Smsgs_cmdIds_crsReq = 22,
    Smsgs_cmdIds_crsRsp = 23,
    Smsgs_cmdIds_crsDiscReq = 24,
    Smsgs_cmdIds_crsDiscRsp = 25,

    Smsgs_cmdIds_crsAlarmsListReq = 26,
    Smsgs_cmdIds_crsAlarmsListRsp = 27,
    Smsgs_cmdIds_crsAlarmsSetReq = 28,
    Smsgs_cmdIds_crsAlarmsSetRsp = 29,

} Smsgs_cmdIds_t;

/*!
 Status values for the over-the-air messages
 */
typedef enum
{
    /*! Success */
    Smsgs_statusValues_success = 0,
    /*! Message was invalid and ignored */
    Smsgs_statusValues_invalid = 1,
    /*!
     Config message was received but only some frame control fields
     can be sent or the reportingInterval or pollingInterval fail
     range checks.
     */
    Smsgs_statusValues_partialSuccess = 2,
} Smsgs_statusValues_t;

#endif /* APPLICATION_SMSGS_H_ */
