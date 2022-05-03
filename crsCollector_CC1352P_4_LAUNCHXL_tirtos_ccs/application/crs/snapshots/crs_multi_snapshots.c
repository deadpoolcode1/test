/*
 * crs_multi_snapshots.c
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */
#include "crs_multi_snapshots.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "ti_drivers_config.h"
#include "application/crs/crs.h"
#include "application/crs/crs_nvs.h"
#include "config_parsing.h"
#include "mac/mac_util.h"
#include "crs_snap_rf.h"
#include "crs_script_dig.h"


#define RUN_NEXT_FILE_EV 0x1
#define RUN_NEXT_LINE_EV 0x2

static crs_package_t gPackage = {0};
static uint16_t gMultiFilesEvents = 0;

static uint32_t gLutLineIdx = 0;
static uint32_t gFileIdx = 0;

static FPGA_cbFn_t gCbFn = NULL;

static Semaphore_Handle collectorSem;
static CRS_chipType_t gChipType;
static CRS_chipMode_t gChipMode;

static CRS_retVal_t getNextLutLine(uint32_t* lineNum);

static void uploadSnapRfCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs);

CRS_retVal_t MultiFiles_multiFilesInit(void *sem)
{
    collectorSem = sem;
}

void MultiFiles_process(void)
{

    CRS_retVal_t rspStatus;

    if (gMultiFilesEvents & RUN_NEXT_FILE_EV)
    {
        CRS_LOG(CRS_DEBUG,"in MultiFiles_process RUN_NEXT_FILE_EV");

        if (gFileIdx >= gPackage.numFileInfos)
        {
            const FPGA_cbArgs_t cbArgs;
            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
            gCbFn(cbArgs);
            return;
        }
        gLutLineIdx = 0;
        CRS_retVal_t rspStatus;
        if (gChipType == RF)
        {
            Util_setEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
            Semaphore_post(collectorSem);
            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
            return;
        }
        else if (gChipType == DIG)
        {
            rspStatus = DIG_uploadSnapDig(gPackage.fileInfos[gFileIdx].name, gChipMode, 0xff, gPackage.fileInfos[gFileIdx].nameValues, uploadSnapDigCb);
        }
        else
        {
            rspStatus = DIG_uploadSnapFpga(gPackage.fileInfos[gFileIdx].name, gChipMode,gPackage.fileInfos[gFileIdx].nameValues, uploadSnapDigCb);
        }

        gFileIdx++;

        if (rspStatus != CRS_SUCCESS)
        {
            const FPGA_cbArgs_t cbArgs;
            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
            gCbFn(cbArgs);
            return;
        }
        Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);

    }

    if (gMultiFilesEvents & RUN_NEXT_LINE_EV)
    {
        CRS_LOG(CRS_DEBUG,"in MultiFiles_process RUN_NEXT_LINE_EV");

        //if ends lutlines then: gFileIdx++
        while (gLutLineIdx < LUT_SZ && (gPackage.fileInfos[gFileIdx].LUTline[gLutLineIdx] == 0))
        {
            gLutLineIdx++;
        }

        if (gLutLineIdx == LUT_SZ)
        {
            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
            Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
            gFileIdx++;
            Semaphore_post(collectorSem);
            return;

        }

        CRS_retVal_t rspStatus = RF_uploadSnapRf(gPackage.fileInfos[gFileIdx].name, 0xff, gLutLineIdx, gChipMode, gPackage.fileInfos[gFileIdx].nameValues,uploadSnapRfCb);
        if (rspStatus != CRS_SUCCESS)
        {
            const FPGA_cbArgs_t cbArgs;
            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
            gCbFn(cbArgs);
            return;
        }
        gLutLineIdx++;
        Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
    }


}




CRS_retVal_t MultiFiles_runMultiFiles(crs_package_t *package, CRS_chipType_t chipType, CRS_chipMode_t chipMode,FPGA_cbFn_t cbFunc)
{
    gLutLineIdx = 0;
    gFileIdx = 0;
    gChipType = chipType;
    gChipMode = chipMode;

    memset(&gPackage, 0, sizeof(crs_package_t));
    memcpy(&gPackage, package, sizeof(crs_package_t));
    CRS_LOG(CRS_DEBUG,"in MultiFiles_runMultiFiles");
    gCbFn = cbFunc;
    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);

    Semaphore_post(collectorSem);
    return CRS_SUCCESS;
}

static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
    Semaphore_post(collectorSem);
}

static void uploadSnapRfCb(const FPGA_cbArgs_t _cbArgs)
{
    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}


