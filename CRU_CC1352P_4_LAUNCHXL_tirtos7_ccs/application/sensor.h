/*
 * sensor.h
 *
 *  Created on: 14 Apr 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_SENSOR_H_
#define APPLICATION_SENSOR_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/PIN.h>
#include "ti_drivers_config.h"
#include "crs_global_defines.h"
#include "mac/api_mac.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/

/*! Event ID - Start the device in the network */
#define SENSOR_START_EVT               0x0001
#define SENSOR_UI_INPUT_EVT               0x0002

/******************************************************************************
 Structures
 *****************************************************************************/

/*! Network Information */
typedef struct
{
    /* Address information */
    ApiMac_deviceDescriptor_t devInfo;

} Llc_netInfo_t;

/*! Sensor Status Values */
typedef enum
{
    /*! Success */
    Sensor_status_success = 0,
    /*! Sensor isn't in the correct state to send a message */
    Sensor_status_invalid_state = 1
} Sensor_status_t;

/******************************************************************************
 Global Variables
 *****************************************************************************/

/*! Collector events flags */
extern uint16_t Sensor_events;

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

extern void Sensor_init( );

extern void Sensor_process(void);

void Ssf_processCliUpdate();

Sensor_status_t Sensor_sendCrsRsp(uint16_t shortAddr, uint8_t *pMsg);
Sensor_status_t Sensor_sendOadRsp(uint16_t *pDstAddr,
                          uint8_t *pMsgPayload,
                          uint32_t msgLen);
void Ssf_crsInitScript();

extern bool Ssf_getNetworkInfo(ApiMac_deviceDescriptor_t *pDevInfo,
                              Llc_netInfo_t  *pParentInfo);


#endif /* APPLICATION_SENSOR_H_ */
