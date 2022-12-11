/*
 * crs_tmp.c
 *
 *  Created on: 7 Aug 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/
/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/SPI.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ti/sysbios/knl/Task.h>

/* Driver configuration */
#include "ti_drivers_config.h"
#include "crs_cli.h"
#include "crs_tmp.h"
#include <inttypes.h>
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define LINE_SIZE 30

/******************************************************************************
 Local variables
 *****************************************************************************/

static uint8_t gMasterRxBuffer[SPI_MSG_LENGTH] = {0};
static uint8_t gMasterTxBuffer[SPI_MSG_LENGTH] = {0};

static SPI_Handle  gMasterSpiHandle;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static uint8_t setBit(uint8_t n, uint8_t k);
static uint8_t clearBit(uint8_t n, uint8_t k);
static uint8_t toggleBit(uint8_t n, uint8_t k);
static CRS_retVal_t openSpi();
static CRS_retVal_t closeSpi();
static CRS_retVal_t convertLineAsciToSpiFormat(uint8_t *line , uint8_t* rspBuf);
static CRS_retVal_t setWrInBuf(uint8_t *buf);
static CRS_retVal_t setRdInBuf(uint8_t *buf);
static CRS_retVal_t addAddrToBuf(uint8_t *buf, uint8_t addr);
static CRS_retVal_t addValToBuf(uint8_t *buf, uint32_t val);
static CRS_retVal_t getValFromBuf(uint8_t *buf, uint32_t* val);
static CRS_retVal_t sendSpiBuf();

/******************************************************************************
 Public Functions
 *****************************************************************************/
//TODO: check return val from wr command to fpga.
//TODO: check valid input.
//TODO: print status.
//TODO: change the val of discovery back to orig.
//SPI format: first bit: read/write, 8 bit addr, 7 bit dont care, 32 bit data.
CRS_retVal_t Fpga_tmpInit()
{
    GPIO_init();
    SPI_init();

    GPIO_setConfig(CONFIG_FPGA_CS, GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH);
    GPIO_write(CONFIG_FPGA_CS, 1);

//    openSpi();
//    memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//    convertLineAsciToSpiFormat("wr 0x40 0x12345678", gMasterTxBuffer);
//
//    if (sendSpiBuf() != CRS_SUCCESS)
//    {
////        closeSpi();
//        return CRS_FAILURE;
//    }
//
//    memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//    convertLineAsciToSpiFormat("rd 0x40", gMasterTxBuffer);
//
//    if (sendSpiBuf() != CRS_SUCCESS)
//    {
////        closeSpi();
//        return CRS_FAILURE;
//    }
//
//    uint32_t val;
//    FPGA_getValFromBuf(gMasterRxBuffer, &val);
//
//    if (val != 0x12345678)
//    {
//        //return CRS_FAILURE;
//    }
//
//    int i = 0;
//
////    for (i = 0; i < 100; i++)
////    {
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("wr 0xb 0x1", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        char tmp[30] = {0};
////        memcpy(tmp, "wr 0xff 0x", strlen("wr 0xff 0x"));
////        //tmp[strlen(tmp)] = (i % 8) + '0';
////
////        if ( i % 2  == 0)
////        {
////            tmp[strlen(tmp)] = '6';
////        }
////        else
////        {
////            tmp[strlen(tmp)] = '7';
////        }
////        convertLineAsciToSpiFormat(tmp, gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("wr 0x50 0x071234", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("wr 0x51 0x70000", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("rd 0x51", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////
////    }
//    uint32_t counter = 0;
//    char tmp[30] = {0};
////"wr 0xb 0x1\n"
//    for (i = 0; i < 10; i+=1)
//    {
//        CLI_cliPrintf("\r\n----------------START----------------");
//        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//           convertLineAsciToSpiFormat("wr 0xb 0x1", gMasterTxBuffer);
//
//           if (sendSpiBuf() != CRS_SUCCESS)
//           {
//               //        closeSpi();
//               return CRS_FAILURE;
//           }
//        memset(tmp, 0, sizeof(tmp));
//       // if (i % 3 == 0)
//        {
//            memcpy(tmp, "wr 0xff 0x", strlen("wr 0xff 0x"));
//            sprintf(&(tmp[strlen(tmp)]), "%x", i);
//
//        }
//       // else if( i % 3 == 1)
//        {
//
//        }
//    //    else
//        {
//            //memcpy(tmp, "wr 0x42 0x", strlen("wr 0x42 0x"));
//
//        }
////        tmp[strlen(tmp)] = i;
//        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//        convertLineAsciToSpiFormat(tmp, gMasterTxBuffer);
//
//        if (sendSpiBuf() != CRS_SUCCESS)
//        {
//            //        closeSpi();
//            return CRS_FAILURE;
//        }
//        memcpy(tmp, "wr 0x50 0x71234", strlen("wr 0x50 0x71234"));
//        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//               convertLineAsciToSpiFormat(tmp, gMasterTxBuffer);
//
//               if (sendSpiBuf() != CRS_SUCCESS)
//               {
//                   //        closeSpi();
//                   return CRS_FAILURE;
//               }
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("wr 0x40 0x3", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
////
////        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
////        convertLineAsciToSpiFormat("wr 0x40 0x7", gMasterTxBuffer);
////
////        if (sendSpiBuf() != CRS_SUCCESS)
////        {
////            //        closeSpi();
////            return CRS_FAILURE;
////        }
//        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//
////        if (i % 3 == 0)
////        {
////            convertLineAsciToSpiFormat("rd 0x0", gMasterTxBuffer);
////        }
////        else if (i % 3 == 1)
////        {
////            convertLineAsciToSpiFormat("rd 0x41", gMasterTxBuffer);
////
////        }
////        else
////        {
////            convertLineAsciToSpiFormat("rd 0x42", gMasterTxBuffer);
////
////        }
//        convertLineAsciToSpiFormat("wr 0x51 0x70000", gMasterTxBuffer);
//        if (sendSpiBuf() != CRS_SUCCESS)
//                {
//                    //        closeSpi();
//                    return CRS_FAILURE;
//                }
//        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
//
//                    convertLineAsciToSpiFormat("rd 0x51", gMasterTxBuffer);
//
//        if (sendSpiBuf() != CRS_SUCCESS)
//        {
//            //        closeSpi();
//            return CRS_FAILURE;
//        }
//        val = 0;
//        FPGA_getValFromBuf(gMasterRxBuffer, &val);
//
//        if (val != i)
//        {
//            counter++;
//            CLI_cliPrintf("\r\n!!ERROR!!: Sent 0x%x. Rec: 0x%x", i, val);
//            //i--;
//        }
//        CLI_cliPrintf("\r\n----------------END----------------");
//
//    }
//    CLI_cliPrintf("\r\nNum mistake: 0x%x", counter);
//

    return CRS_SUCCESS;

}

CRS_retVal_t Fpga_tmpWriteMultiLine(char *line, uint32_t *rsp)
{
#ifndef CRS_TMP_SPI
    return CRS_FAILURE;

#endif
//    CLI_cliPrintf("\r\n%s", line);
    if (line == NULL   )
    {
        CRS_LOG(CRS_ERR, "\r\nLine is NULL");
        return CRS_FAILURE;
    }




    //maybe read reg a and see if there is a return.
    if (line[0] == '\r' && line[1] == 0)
    {
        CRS_LOG(CRS_INFO, "\r\nLine is empty");

        return CRS_SUCCESS;
    }

    const char d[2] = "\n";

    char *token2 = strtok((line), d);

//    memcpy(gFileToUpload[0], token2, strlen(token2));

//    token2 = strtok(NULL, d);
//    gNumLines = 1;
    while ((token2 != NULL) && (strlen(token2) != 0))
    {

        memset(gMasterTxBuffer, 0, sizeof(gMasterTxBuffer));
        if (token2[strlen(token2)-1] == '\r')
        {
            token2[strlen(token2)-1] = 0;
        }
        CRS_retVal_t retVal = convertLineAsciToSpiFormat((uint8_t*)token2, gMasterTxBuffer);
        if (retVal != CRS_SUCCESS)
        {
//            return CRS_FAILURE; //TODO fix 'ver' command and split it into rd 0x0 and rd 0x date register
            token2 = strtok(NULL, d);
            continue;
        }
//        CLI_cliPrintf("\r\n%s", token2);

//        CRS_LOG(CRS_INFO, "\r\nBefore sendSpiBuf()");
//        Task_sleep(5000);
        if (sendSpiBuf() != CRS_SUCCESS)
        {

            return CRS_FAILURE;
        }
        token2 = strtok(NULL, d);
    }

    uint32_t val;

    val = 0;
    // Add special case for 0x50/51
    FPGA_getValFromBuf(gMasterRxBuffer, &val);

    *rsp = val;
    return CRS_SUCCESS;

}



CRS_retVal_t FPGA_getValFromBuf(uint8_t *buf, uint32_t* val)
{
    *val = 0;
    *val =  buf[2];
    *val = (*val) << 8;
    *val = (*val) | buf[3];
    *val = (*val) << 8;
    *val = (*val) | buf[4];
    *val = (*val) << 8;
    *val = (*val) | buf[5];
    return CRS_SUCCESS;
}

/******************************************************************************
 Local Functions
 *****************************************************************************/

static CRS_retVal_t openSpi()
{
    SPI_Params      spiParams;

    /* Open SPI as master (default) */
    SPI_Params_init(&spiParams);
    spiParams.frameFormat = SPI_POL0_PHA0;
    spiParams.bitRate = 4000000;
    spiParams.dataSize = 8;
    gMasterSpiHandle = SPI_open(Board_SPI_0, &spiParams);
    if (gMasterSpiHandle == NULL)
    {
       return CRS_FAILURE;
    }
    return CRS_SUCCESS;

}

static CRS_retVal_t closeSpi()
{
    SPI_close(gMasterSpiHandle);
    return CRS_SUCCESS;
}

//SPI format: first bit: read/write, 8 bit addr, 7 bit dont care, 32 bit data.
//not using strtok
//TODO: check strstr returns
//TODO fix 'ver' command and split it into rd 0x0 and rd 0x date register
static CRS_retVal_t convertLineAsciToSpiFormat(uint8_t *line , uint8_t* rspBuf)
{
//    CLI_cliPrintf("\r\n%s", line);

    if (line[0] == 'w' && line[1] == 'r' && line[2] == ' ')
    {
        char tmpLine[15] = {0};
        //the addr end
        char *ptr = strstr((char*)(&line[3]), " ");
        if (ptr  == NULL || ptr - ((char *)line) > 10)
        {
            return CRS_FAILURE;
        }
        memcpy(tmpLine, (char*)(&line[3]), ptr - ((char*)(&line[3])));
        uint32_t addr = strtoul(tmpLine, NULL, 0);
        if (addr > 256)
        {
            return CRS_FAILURE;
        }

        uint32_t val = strtoul(ptr, NULL, 0);


        setWrInBuf(rspBuf);
        addAddrToBuf(rspBuf, (uint8_t)addr);
        addValToBuf(rspBuf, val);

    }
    else if (line[0] == 'r' && line[1] == 'd' && line[2] == ' ')
    {

        uint32_t addr = strtoul((char*) (&line[3]), NULL, 0);
        if (addr > 256)
        {
            return CRS_FAILURE;
        }

        setRdInBuf(rspBuf);
        addAddrToBuf(rspBuf, (uint8_t)addr);

    }
    else
    {
        CRS_LOG(CRS_INFO,"\r\nbad buffer input to spi %s",line);
        return CRS_FAILURE;
    }

    return CRS_SUCCESS;

}


static CRS_retVal_t setWrInBuf(uint8_t *buf)
{
    buf[0] = clearBit(buf[0], 7+1);
    return CRS_SUCCESS;
}

static CRS_retVal_t setRdInBuf(uint8_t *buf)
{
    buf[0] = setBit(buf[0], 7+1);
    return CRS_SUCCESS;
}

static CRS_retVal_t addAddrToBuf(uint8_t *buf, uint8_t addr)
{
    if (addr & 0x01)
    {
        buf[1] = setBit(buf[1], 7+1);
    }
    if (addr & 0x02)
    {
        buf[0] = setBit(buf[0], 0 + 1);
    }
    if (addr & 0x04)
    {
        buf[0] = setBit(buf[0], 1 + 1);
    }
    if (addr & 0x08)
    {
        buf[0] = setBit(buf[0], 2 + 1);
    }
    if (addr & 0x10)
    {
        buf[0] = setBit(buf[0], 3 + 1);
    }
    if (addr & 0x20)
    {
        buf[0] = setBit(buf[0], 4 + 1);
    }
    if (addr & 0x40)
    {
        buf[0] = setBit(buf[0], 5 + 1);
    }
    if (addr & 0x80)
    {
        buf[0] = setBit(buf[0], 6 + 1);
    }
    return CRS_SUCCESS;

}

static CRS_retVal_t addValToBuf(uint8_t *buf, uint32_t val)
{
    buf[2] = (uint8_t)((val & 0xff000000) >> 24);
    buf[3] = (uint8_t)((val & 0x00ff0000) >> 16);
    buf[4] = (uint8_t)((val & 0x0000ff00) >> 8);
    buf[5] = (uint8_t)((val & 0x000000ff));
    return CRS_SUCCESS;

}





static CRS_retVal_t sendSpiBuf()
{

    if (openSpi() != CRS_SUCCESS)
    {
        closeSpi();
        return CRS_FAILURE;

    }

    memset(gMasterRxBuffer, 0, sizeof(gMasterRxBuffer));

    SPI_Transaction transaction;
    bool            transferOK;

    transaction.count = 6;
    transaction.txBuf = (void*) gMasterTxBuffer;
    transaction.rxBuf = (void*) gMasterRxBuffer;
    int i = 0;

//    CLI_cliPrintf("\r\nsending 0x");

//    for (i =0; i < 6; i++)
//    {
////        uint8_t val2 = gMasterTxBuffer[i];
////        CLI_cliPrintf("%x" , (uint32_t) gMasterTxBuffer[i]);
//
//    }


    Task_sleep(100);

    GPIO_write(CONFIG_FPGA_CS, 0);
//    Task_sleep(10000);

    transferOK = SPI_transfer(gMasterSpiHandle, &transaction);
//    Task_sleep(100);

    GPIO_write(CONFIG_FPGA_CS, 1);
    Task_sleep(100);

    if (!transferOK)
    {
        closeSpi();
        return CRS_FAILURE;
    }
    //if this is a read
    if (gMasterTxBuffer[0] & 0x80)
    {
        uint32_t val = 0;
        FPGA_getValFromBuf(gMasterRxBuffer, &val);
//        CLI_cliPrintf("\r\nread 0x%x", val);
    }
    else
    {
        uint32_t val = 0;
        FPGA_getValFromBuf(gMasterRxBuffer, &val);
//        CLI_cliPrintf("\r\nwrite. and returned from fpga: 0x%x", val<<1);
    }
    closeSpi();
    return CRS_SUCCESS;


}

// Function to set the kth bit of n
static uint8_t setBit(uint8_t n, uint8_t k)
{
    return (n | (1 << (k - 1)));
}

// Function to clear the kth bit of n
static uint8_t clearBit(uint8_t n, uint8_t k)
{
    return (n & (~(1 << (k - 1))));
}

// Function to toggle the kth bit of n
static uint8_t toggleBit(uint8_t n, uint8_t k)
{
    return (n ^ (1 << (k - 1)));
}

