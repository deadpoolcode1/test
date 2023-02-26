/*
 * crs_discovery.c
 *
 *  Created on: 4 Jan 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "ti_drivers_config.h"
#include "crs_discovery.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define ROUTING_STRINGS_SIZE 15
#define CHECK_NEXT_CHIP_EV 0x1
#define FOUND_MODE_EV 0x2
#define RUN_NEXT_LINE_EV 0x4

#define RCM11_STATUS_BIT 0x1
#define RCM12_STATUS_BIT 0x2
#define RCM21_STATUS_BIT 0x4
#define RCM22_STATUS_BIT 0x8

#define RCM11_ROUTING_PLACE

/******************************************************************************
 Local variables
 *****************************************************************************/
//static char *gFpgaRoutingStrings[ROUTING_STRINGS_SIZE] = { "wr 0xff 0x" };
//static CRS_chipMode_t gMode = MODE_NATIVE;
//static Semaphore_Handle collectorSem;
//static uint32_t gChipsStatus = 0;
//static uint32_t gChipsStatusIdx = 0;
//static FPGA_cbFn_t gCbFn = NULL;
//static uint16_t Discovery_events = 0;

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t Discovery_init(void *sem)
{
//    collectorSem = sem;
    return CRS_SUCCESS;
}

void Discovery_process(void);

CRS_retVal_t Discovery_discoverAll(FPGA_cbFn_t _cbFn);

CRS_retVal_t Discovery_discoverChip(CRS_chipType_t chipType, FPGA_cbFn_t _cbFn);

CRS_retVal_t Discovery_discoverMode(FPGA_cbFn_t _cbFn);

