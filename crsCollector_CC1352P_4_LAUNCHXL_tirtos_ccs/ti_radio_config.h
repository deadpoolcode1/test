#ifndef _SMARTRF_SETTINGS_H_
#define _SMARTRF_SETTINGS_H_

//*********************************************************************************
// Generated by SmartRF Studio version 2.24.0 (build#328)
// The applied template is compatible with cc13x2_26x2 SDK version 2.30.xx.xx or newer.
// Device: CC1352P Rev. E (2.1). 
//
//*********************************************************************************
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)
#include <ti/drivers/rf/RF.h>

#define LAUNCHXL_CC1352P_4

// TX Power Table Size Definition
#define TX_POWER_TABLE_SIZE 18

// TX Power Table Object
extern RF_TxPowerTable_Entry txPowerTable[];

// TI-RTOS RF Mode Object
extern RF_Mode RF_prop;

// RF Core API Commands
extern rfc_CMD_PROP_RADIO_DIV_SETUP_PA_t RF_cmdPropRadioDivSetup;
extern rfc_CMD_RADIO_SETUP_PA_t RF_cmdRadioSetup;
extern rfc_CMD_FS_t RF_cmdFs;
extern rfc_CMD_PROP_TX_t RF_cmdPropTx;
extern rfc_CMD_PROP_RX_t RF_cmdPropRx;
extern rfc_CMD_PROP_RX_ADV_t RF_cmdPropRxAdv;
extern rfc_CMD_PROP_RADIO_SETUP_PA_t RF_cmdPropRadioSetup;

// RF Core API Overrides
extern uint32_t pOverrides[];

#endif // _SMARTRF_SETTINGS_H_
