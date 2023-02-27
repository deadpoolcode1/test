/*
 * crs_multi_snapshots.c
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_multi_snapshots_spi.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ti/sysbios/knl/Semaphore.h>
#include "ti_drivers_config.h"
#include "application/crs/crs.h"
#include "application/crs/crs_nvs.h"
//#include "config_parsing.h"
#include "mac/mac_util.h"
//#include "crs_snap_rf.h"
//#include "crs_script_dig.h"
#include "crs_script_dig_spi.h"
#include "application/crs/snapshots/crs_snap_rf_spi.h"
#include "crs_script_rf.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define RUN_NEXT_FILE_EV 0x1
#define RUN_NEXT_LINE_EV 0x2

typedef struct multiPackageTraverser
{
    uint32_t lutLineIdx;
    uint32_t fileIdx;
    CRS_chipType_t chipType;
    CRS_chipMode_t chipMode;
    SPI_crs_package_t package;
    uint32_t chipAddr;
} multiPackageTraverser_t;
/******************************************************************************
 Local variables
 *****************************************************************************/
static SPI_crs_package_t gPackage = { 0 };
static uint16_t gMultiFilesEvents = 0;
static uint32_t gLutLineIdx = 0;
static uint32_t gFileIdx = 0;
//static FPGA_cbFn_t gCbFn = NULL;
//static FPGA_tmpCbFn_t gCbFn = NULL;
static Semaphore_Handle collectorSem;
static CRS_chipType_t gChipType;
static CRS_chipMode_t gChipMode;
/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
//static void uploadSnapRfCb(const FPGA_cbArgs_t _cbArgs);
//static void uploadSnapDigCb(const FPGA_cbArgs_t _cbArgs);
static void uploadSnapRfCb(void);
static void uploadSnapDigCb(void);

static CRS_retVal_t runNextFile(multiPackageTraverser_t *packageTraverser);
static CRS_retVal_t runNextLine(multiPackageTraverser_t *packageTraverser);

/******************************************************************************
 Public Functions
 *****************************************************************************/

CRS_retVal_t MultiFilesSPI_runMultiFiles(SPI_crs_package_t *package,
                                      uint32_t chipAddr,
                                      CRS_chipType_t chipType,
                                      CRS_chipMode_t chipMode)
{
//    gLutLineIdx = 0;
//    gFileIdx = 0;
//    gChipType = chipType;
//    gChipMode = chipMode;
    multiPackageTraverser_t packageTraverser = {0};
    packageTraverser.chipMode = chipMode;
    packageTraverser.chipType = chipType;
    packageTraverser.chipAddr = chipAddr;

    memset(&packageTraverser.package, 0, sizeof(SPI_crs_package_t));
    memcpy(&packageTraverser.package, package, sizeof(SPI_crs_package_t));
    CRS_LOG(CRS_DEBUG, "in MultiFiles_runMultiFiles");
//    gCbFn = cbFunc;
//    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
    CRS_retVal_t rspStatus = runNextFile(&packageTraverser);

    return rspStatus;
}

//void MultiFilesSPI_process(void)
//{
//    if (gMultiFilesEvents & RUN_NEXT_FILE_EV)
//    {
//        CRS_LOG(CRS_DEBUG, "in MultiFiles_process RUN_NEXT_FILE_EV");
//
//        if (gFileIdx >= gPackage.numFileInfos)
//        {
////            const FPGA_cbArgs_t cbArgs={0};
////            gCbFn(cbArgs);
//            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
////            gCbFn();
//            return;
//        }
//        gLutLineIdx = 0;
//        CRS_retVal_t rspStatus;
//        if (gChipType == RF)
//        {
//            Util_setEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
//            Semaphore_post(collectorSem);
//            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
//            return;
//        }
//        else if (gChipType == DIG)
//        {
//            rspStatus = DigSPI_uploadSnapDig(
//                    gPackage.fileInfos[gFileIdx].name, gChipMode, 0xff,
//                    gPackage.fileInfos[gFileIdx].nameValues);
//            uploadSnapDigCb();
//        }
//        else
//        {
//           if (gPackage.fileInfos[gFileIdx].type == SPI_SC)
//           {
//               rspStatus = scriptRf_runFile((uint8_t*)gPackage.fileInfos[gFileIdx].name,
//                                                gPackage.fileInfos[gFileIdx].nameValues,
//                                                0xff, gLutLineIdx);
//           }
//           else
//           {
//            rspStatus = DigSPI_uploadSnapFpga(
//                    gPackage.fileInfos[gFileIdx].name, gChipMode,
//                    gPackage.fileInfos[gFileIdx].nameValues);
//           }
//
//            uploadSnapDigCb();
//        }
//
//        gFileIdx++;
//
//        if (rspStatus != CRS_SUCCESS)
//        {
////            const FPGA_cbArgs_t cbArgs={0};
//            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
////            gCbFn();
//
////            gCbFn(cbArgs);
//            return;
//        }
////        Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
//
//    }
//
//    if (gMultiFilesEvents & RUN_NEXT_LINE_EV)
//    {
//        CRS_LOG(CRS_DEBUG, "in MultiFiles_process RUN_NEXT_LINE_EV");
//
//        //if ends lutlines then: gFileIdx++
//        while (gLutLineIdx < LUT_SZ
//                && (gPackage.fileInfos[gFileIdx].LUTline[gLutLineIdx] == 0))
//        {
//            gLutLineIdx++;
//        }
//
//        if (gLutLineIdx == LUT_SZ)
//        {
//            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
//            Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
//            gFileIdx++;
//            Semaphore_post(collectorSem);
//            return;
//
//        }
//        CRS_retVal_t rspStatus = CRS_SUCCESS;
//
//        if (gPackage.fileInfos[gFileIdx].type == SPI_SC)
//        {
//            rspStatus = scriptRf_runFile((uint8_t*)gPackage.fileInfos[gFileIdx].name,
//                                             gPackage.fileInfos[gFileIdx].nameValues,
//                                             0xff, gLutLineIdx);
//        }
//        else
//        {
//            rspStatus = SPI_RF_uploadSnapRf(gPackage.fileInfos[gFileIdx].name, 0xff, gLutLineIdx, gChipMode,
//                    gPackage.fileInfos[gFileIdx].nameValues);
//        }
//        Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
//
////        CRS_retVal_t rspStatus = scriptRf_runFile((uint8_t*)gPackage.fileInfos[gFileIdx].name,
////                                                  gPackage.fileInfos[gFileIdx].nameValues,
////                                                  0xff, gLutLineIdx);
//        if (rspStatus != CRS_SUCCESS)
//        {
////            const FPGA_cbArgs_t cbArgs={0};
//            Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
////            gCbFn(cbArgs);
////            gCbFn();
//
//            return;
//        }
//        uploadSnapRfCb();
//        gLutLineIdx++;
////        Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
//    }
//
//}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static void uploadSnapDigCb(void)
{
    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_FILE_EV);
    Semaphore_post(collectorSem);
}

static void uploadSnapRfCb(void)
{
    Util_setEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
    Semaphore_post(collectorSem);
}



static CRS_retVal_t runNextFile(multiPackageTraverser_t *packageTraverser)
{
//    CRS_LOG(CRS_DEBUG, "in MultiFiles_process RUN_NEXT_FILE_EV");

    while(packageTraverser->fileIdx < packageTraverser->package.numFileInfos)
    {
         packageTraverser->lutLineIdx = 0;
         CRS_retVal_t rspStatus = CRS_SUCCESS;
         if (packageTraverser->chipType == RF)
         {
             rspStatus = runNextLine(packageTraverser);
         }
         else if (packageTraverser->chipType == DIG)
         {
             rspStatus = DigSPI_uploadSnapDig(
                     packageTraverser->package.fileInfos[packageTraverser->fileIdx].name,
                     packageTraverser->chipMode, packageTraverser->chipAddr,
                     packageTraverser->package.fileInfos[packageTraverser->fileIdx].nameValues);
         }
         else
         {
            if (packageTraverser->package.fileInfos[packageTraverser->fileIdx].type == SPI_SC)
            {
               rspStatus = scriptRf_runFile((uint8_t*)packageTraverser->package.fileInfos[packageTraverser->fileIdx].name,
                                                packageTraverser->package.fileInfos[packageTraverser->fileIdx].nameValues,
                                                packageTraverser->chipAddr, packageTraverser->lutLineIdx, true);
            }
            else
            {
            rspStatus = DigSPI_uploadSnapFpga(
                    packageTraverser->package.fileInfos[packageTraverser->fileIdx].name,
                    packageTraverser->chipMode,
                    packageTraverser->package.fileInfos[packageTraverser->fileIdx].nameValues);
            }

         }


         if (rspStatus != CRS_SUCCESS)
         {
             return rspStatus;
         }

         packageTraverser->fileIdx++;
    }

    return CRS_SUCCESS;
}

static CRS_retVal_t runNextLine(multiPackageTraverser_t *packageTraverser)
{

    while(true)
    {
         //if ends lutlines then: gFileIdx++
         while (packageTraverser->lutLineIdx < LUT_SZ
                 && (packageTraverser->package.fileInfos[packageTraverser->fileIdx].LUTline[packageTraverser->lutLineIdx] == 0))
         {
             packageTraverser->lutLineIdx++;
         }

         if (packageTraverser->lutLineIdx == LUT_SZ)
         {
             return CRS_SUCCESS;
         }

         CRS_retVal_t rspStatus = CRS_SUCCESS;
         if (packageTraverser->package.fileInfos[packageTraverser->fileIdx].type == SPI_SC)
         {
             rspStatus = scriptRf_runFile((uint8_t*)packageTraverser->package.fileInfos[packageTraverser->fileIdx].name,
                                              packageTraverser->package.fileInfos[packageTraverser->fileIdx].nameValues,
                                              packageTraverser->chipAddr, packageTraverser->lutLineIdx, true);
         }
         else
         {
             rspStatus = SPI_RF_uploadSnapRf(
                     packageTraverser->package.fileInfos[packageTraverser->fileIdx].name, 0xff,
                     packageTraverser->lutLineIdx, packageTraverser->chipMode,
                     packageTraverser->package.fileInfos[packageTraverser->fileIdx].nameValues, true);
         }
//         CRS_retVal_t rspStatus = scriptRf_runFile((uint8_t*)gPackage.fileInfos[gFileIdx].name,
//                                                   gPackage.fileInfos[gFileIdx].nameValues,
//                                                   0xff, gLutLineIdx);
         if (rspStatus != CRS_SUCCESS)
         {
             return rspStatus;
         }
//         uploadSnapRfCb();
         packageTraverser->lutLineIdx++;
//         Util_clearEvent(&gMultiFilesEvents, RUN_NEXT_LINE_EV);
    }
}
