/*
 * crs_tdd.c
 *
 *  Created on: 24 Feb 2022
 *      Author: epc_4
 */

#include "crs_tdd.h"
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/sysbios/knl/Semaphore.h>

#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include <ti/drivers/GPIO.h>

#include "ti_drivers_config.h"
#include "util_timer.h"
#include "mac_util.h"

#include "crs_cli.h"

#define CRC_TAB_SIZE 256
#define WIDTH (8 * sizeof(uint16_t))
#define LOCK_TIMEOUT 300000 // time to wait between each status request to TDD when timeout status is on
#define LOCK_WAIT 500000    // time to wait between each status request to TDD when timeout status is off
#define BV(n)               (1 << (n))

static uint16_t crcTable [CRC_TAB_SIZE] =
{
     0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
     0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
     0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
     0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
     0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
     0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
     0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
     0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
     0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
     0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
     0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
     0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
     0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
     0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
     0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
     0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
     0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
     0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
     0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
     0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
     0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
     0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
     0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
     0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
     0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
     0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
     0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
     0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
     0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
     0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
     0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
     0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};


#define TX_BUFF_SIZE 256

#define STATUS_RSP_NUM_BYTES 69
#define STATUS_REQ_NUM_BYTES 11

#define SET_RSP_NUM_BYTES 69
#define SET_REQ_NUM_BYTES 69

#define TDD_OPEN_EV 0x1
#define TDD_STATUS_RSP_EV 0x2
#define TDD_SET_RSP_EV 0x4
#define TDD_OPEN_FAIL_EV 0x8

static uint8_t gStatusCommand[STATUS_REQ_NUM_BYTES] = {0x16, 0x16, 0x16, 0x16, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x11, 0x0C};

typedef struct {
    uint8_t *ttg_vals[4];
    uint8_t * rtg_vals[4];
    uint16_t * dl1_us;
    uint16_t * pattern1_period;
    bool * pattern2;
    uint16_t * pattern2_period;
    uint16_t * dl2_us;
    uint8_t * scs;
    uint8_t * ss_pos;
    uint8_t * ho_min;
    uint8_t * ho_sec;
    bool * detect;

} Tdd_setRequest_t;

typedef struct {
    bool lock;
    bool timeout;
}Tdd_lockStatus_t;
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[TX_BUFF_SIZE] = {0};
static uint32_t gUartTxBufferIdx = 0;

static uint8_t gUartRxBuffer[1] = {0};

static TDD_cbFn_t gCbFn = NULL;
static TDD_cbFn_t gInnerCbFn = NULL;
static TDD_cbFn_t gFinalCbFn = NULL;

static Clock_Struct tddClkStruct;
static Clock_Handle tddClkHandle;

static uint16_t Tdd_events = 0;

static Semaphore_Handle collectorSem;

static uint32_t gWantedTxBytes = 0;
static bool gReadNextCommand = false;

static  bool gIsWantedTxBytesKnown = false;
static  bool gIsPrintStatusCommand = false;


static bool gIsOpen = false;
static uint8_t gIsLocked;

static uint8_t gFrame = 0;
static uint8_t gAlloc = 0;

static TDD_tdArgs_t tdArgs = { .period = 5000,
                               .dl1 = 1214};

uint16_t gPeriod [2] = {5000, 10000};
uint16_t gConfigs[2][15] ={ {1214, 1642, 1714, 1785, 1857, 2214, 2642, 2714, 2785, 2857, 3214, 3642, 3714, 3785, 3857},
                          {6214, 6642, 6714, 6785, 6857, 7214, 7642, 7714, 7785, 7857, 8214, 8642, 8714, 8785, 8857}};
//uint16_t configs[2][15] ={ {2214, 1642, 1714, 1785, 1857, 2214, 2642, 2714, 2785, 2857, 3214, 3642, 3714, 3785, 3857},
//                          {6214, 6642, 6714, 6785, 6857, 7214, 7642, 7714, 7785, 7857, 8214, 8642, 8714, 8785, 8857}};

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);
static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size);
static void processTddTimeoutCallback(UArg a0);
static void tddSetReqCallback(const TDD_cbArgs_t _cbArgs);
static void tddOpenCallback(const TDD_cbArgs_t _cbArgs);
static void printStatus(const TDD_cbArgs_t _cbArgs);
static void tddGetStatusCallback(const TDD_cbArgs_t _cbArgs);
static CRS_retVal_t sendMsgAndGetStatus(void * _buffer, size_t _sizeToSend,uint32_t bytesToRead ,TDD_cbFn_t _cbFn );
static CRS_retVal_t setFrameFormatAndAllocationMode(uint8_t frame, uint8_t alloc, TDD_cbFn_t _cbFn);
static void getInitStatus(const TDD_cbArgs_t _cbArgs);
static void Tdd_setTddClock(uint32_t tddTime);

static CRS_retVal_t Tdd_writeString(void * _buffer, size_t _size);
static CRS_retVal_t startRead(uint32_t bytesRead);

CRS_retVal_t Tdd_initSem(void *sem)
{
    collectorSem = sem;
    tddClkHandle = UtilTimer_construct(&tddClkStruct,
                                       processTddTimeoutCallback,
                                        10, 0,
                                        false,
                                        0);
}

CRS_retVal_t Tdd_init(TDD_cbFn_t _cbFn)
{

    if (gUartHandle != NULL)
    {
        return CRS_FAILURE;
    }

    {
        // General UART setup
        UART_init();
        UART_Params_init(&gUartParams);
        gUartParams.baudRate = 38400;
        //        gUartParams.writeMode = UART_MODE_CALLBACK;
        gUartParams.writeMode = UART_MODE_CALLBACK;
        gUartParams.writeCallback = UartWriteCallback;

        gUartParams.writeDataMode = UART_DATA_BINARY;
        //gUartParams.writeCallback = UartWriteCallback;
        gUartParams.readReturnMode = UART_RETURN_FULL;
        gUartParams.baudRate = 38400;
        gUartParams.readEcho = UART_ECHO_OFF;
        gUartParams.readMode = UART_MODE_CALLBACK;
        gUartParams.readDataMode = UART_DATA_BINARY;
        gUartParams.readCallback = UartReadCallback;

        gUartHandle = UART_open(CONFIG_UART_1, &gUartParams);
        if (NULL == gUartHandle)
        {
            return CRS_FAILURE;
        }


        memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
        memset(gUartRxBuffer, 0, 1);

        gUartTxBufferIdx = 0;
        gReadNextCommand = false;
        UART_read(gUartHandle, gUartRxBuffer, 1);

        gFinalCbFn = _cbFn;
        gInnerCbFn = getInitStatus;
        startRead(70);
        sendMsgAndGetStatus(gStatusCommand, 11, 69, tddOpenCallback);
        return CRS_SUCCESS;

    }

    return CRS_SUCCESS;
}


TDD_tdArgs_t Tdd_getTdArgs(){
    return tdArgs;
}

void Tdd_process(void)
{
    if (Tdd_events & TDD_OPEN_EV)
    {
        UtilTimer_stop(&tddClkStruct);

        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        gInnerCbFn(cbArgs);
//        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
        Util_clearEvent(&Tdd_events, TDD_OPEN_EV);
    }

    if (Tdd_events & TDD_STATUS_RSP_EV)
    {
        UtilTimer_stop(&tddClkStruct);

        TDD_cbArgs_t cbArgs;
               cbArgs.arg0 = gUartTxBufferIdx;
               cbArgs.arg3 = gUartTxBuffer;
               gInnerCbFn(cbArgs);
               gFinalCbFn(cbArgs);
        Util_clearEvent(&Tdd_events, TDD_STATUS_RSP_EV);
    }

    if (Tdd_events & TDD_SET_RSP_EV)
    {
        UtilTimer_stop(&tddClkStruct);

        TDD_cbArgs_t cbArgs;
               cbArgs.arg0 = gUartTxBufferIdx;
               cbArgs.arg3 = gUartTxBuffer;
               gInnerCbFn(cbArgs);
               gFinalCbFn(cbArgs);
        Util_clearEvent(&Tdd_events, TDD_SET_RSP_EV);
    }

    if (Tdd_events & TDD_OPEN_FAIL_EV)
    {
        UtilTimer_stop(&tddClkStruct);

        if (    gIsPrintStatusCommand == true)
        {
                    CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
                    Tdd_close();
                    TDD_cbArgs_t cbArgs;
                                   cbArgs.arg0 = gUartTxBufferIdx;
                                   cbArgs.arg3 = gUartTxBuffer;
                                   gFinalCbFn(cbArgs);
        }
        else
        {
            Tdd_printStatus(gFinalCbFn);
        }
        Util_clearEvent(&Tdd_events, TDD_OPEN_FAIL_EV);
    }



}

static uint16_t crcFast(uint8_t const message[], int nBytes)
{
    uint8_t data;
    uint16_t remainder = 0;


    /*
     * Divide the message by the polynomial, a byte at a time.
     */
    int byte;
    for (byte = 0; byte < nBytes; ++byte)
    {
        data = message[byte] ^ (remainder >> (WIDTH - 8));
        remainder = crcTable[data] ^ (remainder << 8);
    }

    /*
     * The final remainder is the CRC.
     */
    return (remainder);

}

static Tdd_setRequest_t createRequest()
{
    Tdd_setRequest_t request ={ .dl1_us = NULL,
                           .dl2_us = NULL,
                           .ho_min = NULL,
                           .ho_sec = NULL,
                           .pattern1_period = NULL,
                           .pattern2_period = NULL,
                           .pattern2 = NULL,
                           .rtg_vals = {NULL, NULL, NULL, NULL},
                           .ttg_vals = {NULL, NULL, NULL, NULL},
                           .scs = NULL,
                           .ss_pos = NULL,
                           .detect = NULL
    };
    return request;
}

static void  makeRequest(Tdd_setRequest_t request, uint8_t * request_array)
{
    /*
        This function builds and formats a "set request" to send to TDD.
        request: array of 45 where we put the request.
        ttg_vals, rtg_vals: TTG and RTG values, arrays of 4 pointers to bytes.
        pattern2: OFF by default
        period1/2, dl1_us/dl2_us: parameters in us.
        SCS - 1 : 15KHZ, 2 : 30KHz, 4 : 60KHz - 1 by default
        ho_min and ho_sec: values between 0 and 60.
        SS_Pos: 0 by default, 0, 2, 4, 8, 16, 20, 22, 30, 32,36, 44, 48 or 50
        special - dictionary of the special subframes as keys and DL1 uS as the values
    */
    uint8_t set_request [45] = {0x16, 0x16, 0x16, 0x16, 0xFF, 0xFF, 0x01, 0x00, 0x22, 0x00, 0x00}; // 11 bytes
    uint8_t flag3 = 0x0;

    if (request.pattern1_period){
            flag3 =  flag3 | 0x20;
            set_request[27] = *request.pattern1_period & 0xff;
            set_request[28] = *request.pattern1_period >> 8;
    }

    if (request.dl1_us){
        flag3 =  flag3 | 0x02;
        set_request[29] = *request.dl1_us & 0xff;
        set_request[30] = *request.dl1_us>>8;
    }

    if(request.pattern2){
        flag3 =  flag3 | 0x01;
        if(*request.pattern2)
            set_request[40] = 0x31;
        else
            set_request[40] = 0x30;
    }

    if (request.pattern2_period){
        flag3 =  flag3 | 0x04;
        set_request[31] = *request.pattern2_period & 0xff;
        set_request[32] = *request.pattern2_period >>8;
    }

    if (request.dl2_us){
        flag3 =  flag3 | 0x08;
        set_request[33] = *request.dl2_us & 0xff;
        set_request[34] = *request.dl2_us >> 8;
    }

    if (request.scs){
        flag3 =  flag3 | 0x10;
        set_request[39] = *request.scs;
    }

    if (request.ss_pos){
        flag3 =  flag3 | 0x40;
        set_request[41] = *request.ss_pos;
    }

    if (request.ho_min){
        flag3 =  flag3 | 0x80;
        set_request[14] = *request.ho_min;
    }

    // set flag4
    uint8_t flag4 = 0x0;
    if(request.ho_sec){
        flag4 = flag4 | 0x10;
        set_request[42] = *request.ho_sec;
    }

    if (request.detect){
        flag4 = flag4 | 0x80;
        if(*request.detect){
            set_request[15] = 0x31; // manual detect
        }
        else{
            set_request[15] = 0x30; // auto detect
        }
    }

    // set flag5
    uint8_t flag5 = 0x0;
    int i;
    for(i=0;i<4;i++){
        if(request.ttg_vals[i]){
            flag5 = flag5 | (0x1 << i);
            set_request[i+19] = *request.ttg_vals[i];
        }
    }

    for(i=0;i<4;i++){
        if(request.rtg_vals[i]){
            flag5 = flag5 | (0x1 << (i+4));
            set_request[i+23] = *request.rtg_vals[i];
        }
    }

    // build request //
    // bytes 0-10: header
    set_request[11] = flag3;
    set_request[12] = flag4;
    set_request[13] = flag5;
    // byte 14: ho_min
    // byte 15: detect mode
    set_request[16] = 0x00; // TDD SW on/off
    set_request[17] = 0x00; // TDD Default
    set_request[18] = 0x00; // TDD Polarity
    // 19-26: TTG and RTG values
    set_request[35] = 0x00; // reserved
    set_request[36] = 0x00; // reserved
    set_request[37] = 0x00; // TDD switch Mode
    set_request[38] = 0x00; // TDD Path Mode
    // bytes 39-42: scs index, pattern_2 on/off, ss_pos, hol_sec

    // calculate CRC
    uint8_t body [39];
    memcpy(body, &set_request[4], 39);
    uint16_t crc = crcFast(body, 39);
    set_request[43] = crc >> 8;
    set_request[44] = crc & 0xff;
    memcpy(request_array, set_request, 45);

}

static void getInitStatus(const TDD_cbArgs_t _cbArgs)
{

    uint8_t scs = gUartTxBuffer[29];  //(currently always 15hz which is 1)
    uint8_t pattern2 = gUartTxBuffer[30];
    uint8_t sspos = gUartTxBuffer[31];
    uint8_t detect = gUartTxBuffer[33];
    uint16_t period = gUartTxBuffer[45] + (gUartTxBuffer[46] << 8);

    if (scs != 1 || pattern2 != 0x30 || sspos != 0x0 || (period != 5000 && period != 10000))
    {
        setFrameFormatAndAllocationMode(0,0,gFinalCbFn);
                return;
    }
    TDD_cbArgs_t cbArgs;
                   cbArgs.arg0 = gUartTxBufferIdx;
                   cbArgs.arg3 = gUartTxBuffer;
    printStatus(cbArgs);
    gFinalCbFn(cbArgs);
}

CRS_retVal_t Tdd_close()
{
    gIsOpen = false;
    if (gUartHandle == NULL)
    {
        return CRS_SUCCESS;
    }

    UART_close(gUartHandle);
    gUartHandle = NULL;
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_isOpen()
{
    if (gIsOpen){
        return CRS_SUCCESS;
    }

    return CRS_TDD_NOT_OPEN;
}

CRS_retVal_t Tdd_isLocked(){
    GPIO_init();
    if(GPIO_read(CONFIG_GPIO_BTN1)){
        return CRS_TDD_NOT_LOCKED;
    }
    return CRS_SUCCESS;
//    if(gIsLocked){
//        return CRS_SUCCESS;
//    }
}

CRS_retVal_t Tdd_printStatus(TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(gStatusCommand, 11, 69,tddGetStatusCallback );
    gIsPrintStatusCommand = true;
}

static void printStatus(const TDD_cbArgs_t _cbArgs)
{
    CLI_cliPrintf("\r\nTDDStatus=OK");
    bool isGood = true;
    uint8_t scs = gUartTxBuffer[29];  //(currently always 15hz which is 1)
    if (scs == 1)
    {
        CLI_cliPrintf("\r\nSCS=15");
    }
    else if (scs == 2)
    {
        isGood = false;
        CLI_cliPrintf("\r\nSCS=30");
    }
    else if (scs == 3)
    {
        isGood = false;
        CLI_cliPrintf("\r\nSCS=60");
    }

    uint8_t pattern2 = gUartTxBuffer[30];
    if (pattern2 == 0x30)
    {
        CLI_cliPrintf("\r\nPattern2=Off");
    }
    else if (pattern2 == 0x31)
    {
        isGood = false;
        CLI_cliPrintf("\r\nPattern2=On");
    }

    uint8_t sspos = gUartTxBuffer[31];
    if (sspos == 0x0)
    {
        CLI_cliPrintf("\r\nSSPosition=0x%x", sspos);
    }
    else
    {
        isGood = false;
        CLI_cliPrintf("\r\nSSPosition=0x%x", sspos);
    }

    uint8_t detect = gUartTxBuffer[33];
    if (detect == 0x30)
    {
        isGood = false;

        CLI_cliPrintf("\r\nSyncMode=Auto");
    }
    else if (detect == 0x31)
    {
        CLI_cliPrintf("\r\nSyncMode=Manual");
    }
    int8_t ttg_vals [4] = {gUartTxBuffer[37], gUartTxBuffer[38], gUartTxBuffer[39], gUartTxBuffer[40]};
    CLI_cliPrintf("\r\nTTG=%i %i %i %i", ttg_vals[0], ttg_vals[1], ttg_vals[2], ttg_vals[3]);
    int8_t rtg_vals [4] = {gUartTxBuffer[41], gUartTxBuffer[42], gUartTxBuffer[43], gUartTxBuffer[44]};
    CLI_cliPrintf("\r\nRTG=%i %i %i %i", rtg_vals[0], rtg_vals[1], rtg_vals[2], rtg_vals[3]);

    uint16_t period = gUartTxBuffer[45] + (gUartTxBuffer[46] << 8);
    uint16_t dl = gUartTxBuffer[47] + (gUartTxBuffer[48] << 8);
    tdArgs.period = period;
    tdArgs.dl1 = dl;
    CLI_cliPrintf("\r\nPeriod1=0x%x", period);
    CLI_cliPrintf("\r\nDL1=0x%x", dl);

    if (isGood == false)
    {
        return;
    }


    if (period == 10000)
    {
        if (dl == 6214 || dl == 7214 || dl == 8214)
        {
            gFrame = 0;
            CLI_cliPrintf("\r\nFrameFormat=0");
        }
        if (dl == 6642 || dl == 7642 || dl == 8642)
        {
            gFrame = 1;

            CLI_cliPrintf("\r\nFrameFormat=1");

        }
        if (dl == 6714 || dl == 7714 || dl == 8714)
        {
            gFrame = 2;

            CLI_cliPrintf("\r\nFrameFormat=2");

        }
        if (dl == 6785 || dl == 7785 || dl == 8785)
        {
            gFrame = 3;

            CLI_cliPrintf("\r\nFrameFormat=3");

        }
        if (dl == 6857 || dl == 7857 || dl == 8857)
        {
            gFrame = 4;

            CLI_cliPrintf("\r\nFrameFormat=4");
        }

        if (dl == 6214 || dl == 6642 || dl == 6714 || dl == 6785 || dl == 6857)
        {
            gAlloc = 3;
            CLI_cliPrintf("\r\nAllocationMode=3");
        }
        if (dl == 7214 || dl == 7642 || dl == 7714 || dl == 7785 || dl == 7857)
        {
            gAlloc = 4;

            CLI_cliPrintf("\r\nAllocationMode=4");
        }
        if (dl == 8214 || dl == 8642 || dl == 8714 || dl == 8785 || dl == 8857)
        {
            gAlloc = 5;

            CLI_cliPrintf("\r\nAllocationMode=5");
        }
    }
    else if (period == 5000)
    {
        if (dl == 1214 || dl == 2214 || dl == 3214)
        {
            gFrame = 0;

            CLI_cliPrintf("\r\nFrameFormat=0");
        }
        if (dl == 1642 || dl == 2642 || dl == 3642)
        {
            gFrame = 1;

            CLI_cliPrintf("\r\nFrameFormat=1");
        }
        if (dl == 1714 || dl == 2714 || dl == 3714)
        {
            gFrame = 2;

            CLI_cliPrintf("\r\nFrameFormat=2");
        }
        if (dl == 1785 || dl == 2785 || dl == 3785)
        {
            gFrame = 3;

            CLI_cliPrintf("\r\nFrameFormat=3");
        }
        if (dl == 1857 || dl == 2857 || dl == 3857)
        {
            gFrame = 4;

            CLI_cliPrintf("\r\nFrameFormat=4");
        }
        if (dl == 1214 || dl == 1642 || dl == 1714 || dl == 1785 || dl == 1857)
        {
            gAlloc = 0;

            CLI_cliPrintf("\r\nAllocationMode=0");
        }
        if (dl == 2214 || dl == 2642 || dl == 2714 || dl == 2785 || dl == 2857)
        {
            gAlloc = 1;

            CLI_cliPrintf("\r\nAllocationMode=1");
        }
        if (dl == 3214 || dl == 3642 || dl == 3714 || dl == 3785 || dl == 3857)
        {
            gAlloc = 2;

            CLI_cliPrintf("\r\nAllocationMode=2");
        }

    }

    gIsLocked = (gUartTxBuffer[58] & 0x10)>>4;
    uint8_t isTimeout = gUartTxBuffer[58] & 0x4;
    CLI_cliPrintf("\r\nTDDLock=0x%x", gIsLocked);
    CLI_cliPrintf("\r\nSearcherTimeout=0x%x", isTimeout>>2);

    uint8_t hol_sec = gUartTxBuffer[57];
    uint8_t hol_min = gUartTxBuffer[18];
    uint16_t holdover_time = hol_sec + (hol_min * 60);
    CLI_cliPrintf("\r\nHoldoverTime=0x%x", holdover_time);

    return CRS_SUCCESS;


}


CRS_retVal_t Tdd_setSyncMode(bool mode, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    // set detect mode 0: auto 1: manual
    Tdd_setRequest_t set = createRequest();
    if (mode > 1)
    {
        mode = 1;
    }

    set.detect = &mode;
    uint8_t req[100] = {0};
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_setSCS(uint8_t scs, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    Tdd_setRequest_t set = createRequest();
    set.scs = &scs;

    uint8_t req[100] = {0};
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_setAllocationMode(uint8_t alloc, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    // make request to change Allocation Mode.
    uint8_t scs = 1;  // 15kHz
    bool pattern2 = false; // pattern2 off
    uint8_t ss_pos = 0; // ss_pos 0
    bool detect = true; // detect manual
    uint16_t dl1;
    uint16_t period;

    Tdd_setRequest_t set = createRequest();

    if(alloc>5){
        // prevent allocation mode from being greater than 5 for now.
        alloc=5;
    }

    set.scs = &scs;
    set.pattern2 = &pattern2;
    set.ss_pos = &ss_pos;
    set.detect = &detect;

    uint8_t frame = gFrame;

    if(frame == 0 || frame == 5){
        dl1 = 214;
    }
    else if(frame == 1 || frame == 6){
        dl1 = 642;
    }
    else if(frame == 2 || frame == 7){
        dl1 = 714;
    }
    else if(frame == 3 || frame == 8){
        dl1 = 785;
    }
    else if(frame == 4){
        dl1 = 857;
    }
    if(alloc<3){
        period = 5000;
        set.pattern1_period = &period;
        dl1 += 1000 + (alloc * 1000);
    }
    else{
        period = 10000;
        set.pattern1_period = &period;
        dl1 += 3000 + (alloc * 1000);
    }

    gAlloc = alloc;
    set.dl1_us = &dl1;

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_setFrameFormat(uint8_t frame, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    // make request to change Allocation Mode.
    uint8_t scs = 1;  // 15kHz
    bool pattern2 = false; // pattern2 off
    uint8_t ss_pos = 0; // ss_pos 0
    bool detect = true; // detect manual
    uint16_t dl1;
    uint16_t period;

    Tdd_setRequest_t set = createRequest();

    set.scs = &scs;
    set.pattern2 = &pattern2;
    set.ss_pos = &ss_pos;
    set.detect = &detect;

    uint8_t alloc = gAlloc;

    if (frame > 8){
        // prevent frame format greater than 8
        frame=8;
    }

    if(frame == 0 || frame == 5){
        dl1 = 214;
    }
    else if(frame == 1 || frame == 6){
        dl1 = 642;
    }
    else if(frame == 2 || frame == 7){
        dl1 = 714;
    }
    else if(frame == 3 || frame == 8){
        dl1 = 785;
    }
    else if(frame == 4){
        dl1 = 857;
    }
    if(alloc<3){
        period = 5000;
        set.pattern1_period = &period;
        dl1 += 1000 + (alloc * 1000);
    }
    else{
        period = 10000;
        set.pattern1_period = &period;
        dl1 += 3000 + (alloc * 1000);
    }

    gFrame = frame;

    set.dl1_us = &dl1;

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

static CRS_retVal_t setFrameFormatAndAllocationMode(uint8_t frame, uint8_t alloc, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }
    // make request to change Allocation Mode.
    uint8_t scs = 1;  // 15kHz
    bool pattern2 = false; // pattern2 off
    uint8_t ss_pos = 0; // ss_pos 0
    bool detect = true; // detect manual
    uint16_t dl1;
    uint16_t period;

    Tdd_setRequest_t set = createRequest();

    set.scs = &scs;
    set.pattern2 = &pattern2;
    set.ss_pos = &ss_pos;
    set.detect = &detect;



    if(frame == 0 || frame == 5){
        dl1 = 214;
    }
    else if(frame == 1 || frame == 6){
        dl1 = 642;
    }
    else if(frame == 2 || frame == 7){
        dl1 = 714;
    }
    else if(frame == 3 || frame == 8){
        dl1 = 785;
    }
    else if(frame == 4){
        dl1 = 857;
    }
    if(alloc<3){
        period = 5000;
        set.pattern1_period = &period;
        dl1 += 1000 + (alloc * 1000);
    }
    else{
        period = 10000;
        set.pattern1_period = &period;
        dl1 += 3000 + (alloc * 1000);
    }


    set.dl1_us = &dl1;

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_setHolTime(uint8_t min, uint8_t sec, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }

    uint8_t hol_sec = sec;
    uint8_t hol_min = min;

    Tdd_setRequest_t set = createRequest();
    set.ho_sec = &hol_sec;
    set.ho_min = &hol_min;

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;

}

CRS_retVal_t Tdd_setTtg(int8_t * ttg_vals, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }


    Tdd_setRequest_t set = createRequest();
    int i;
    for(i=0;i<4;i++){
        set.ttg_vals[i] = &ttg_vals[i];
    }

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

CRS_retVal_t Tdd_setRtg(int8_t * rtg_vals, TDD_cbFn_t _cbFn)
{
    if (Tdd_isOpen() == CRS_TDD_NOT_OPEN)
    {
        CLI_cliPrintf("\r\nTDDStatus=TDD_NOT_OPEN");
        TDD_cbArgs_t cbArgs;
        cbArgs.arg0 = gUartTxBufferIdx;
        cbArgs.arg3 = gUartTxBuffer;
        _cbFn(cbArgs);
        return CRS_FAILURE;
    }


    Tdd_setRequest_t set = createRequest();
    int i;
    for(i=0;i<4;i++){
        set.rtg_vals[i] = &rtg_vals[i];
    }

    uint8_t req[100] = { 0 };
    makeRequest(set, req);

    gFinalCbFn = _cbFn;
    gInnerCbFn = printStatus;
    sendMsgAndGetStatus(req, 45, 69, tddGetStatusCallback);
    return CRS_SUCCESS;
}

static CRS_retVal_t sendMsgAndGetStatus(void * _buffer, size_t _sizeToSend,uint32_t bytesToRead ,TDD_cbFn_t _cbFn )
{
    gCbFn = _cbFn;
    gIsPrintStatusCommand = false;
    startRead(bytesToRead);
    Tdd_setTddClock(70);
    Tdd_writeString(_buffer,_sizeToSend );
}

static CRS_retVal_t startRead(uint32_t bytesRead)
{
    gIsWantedTxBytesKnown = false;
    gWantedTxBytes = bytesRead;
    memset(gUartTxBuffer, 0, TX_BUFF_SIZE);
    gUartTxBufferIdx = 0;
    gReadNextCommand = true;
}

static CRS_retVal_t Tdd_writeString(void * _buffer, size_t _size)
{
    if((gUartHandle == NULL) )
    {
        return CRS_UART_FAILURE;
    }


    if (_buffer == NULL)
    {
        return CRS_SUCCESS;
    }

    UART_write(gUartHandle, _buffer, _size);
}

static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size)
{

}

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size)
{
    if (_size)
    {
        gIsOpen = true;
        if (gReadNextCommand == false)
        {
            UART_read(gUartHandle, gUartRxBuffer, 1);
            return;
        }


        gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*) _buf)[0];
        gUartTxBufferIdx++;
        memset(gUartRxBuffer, 0, _size);
        if (gIsWantedTxBytesKnown == true)
        {
            gWantedTxBytes--;
        }

        if (gUartTxBufferIdx == 9)
        {
            gIsWantedTxBytesKnown = true;
            gWantedTxBytes = gUartTxBuffer[gUartTxBufferIdx - 1];
            gWantedTxBytes += 2;
        }
        if (gWantedTxBytes == 0)
        {
            gIsWantedTxBytesKnown = false;
            gReadNextCommand = false;
            TDD_cbArgs_t cbArgs;
            cbArgs.arg0 = gUartTxBufferIdx;
//            cbArgs.arg1 = gCommandSize;
            cbArgs.arg3 = gUartTxBuffer;
            gCbFn(cbArgs);
        }
        UART_read(gUartHandle, gUartRxBuffer, 1);

    }
    else
    {
        // Handle error or call to UART_readCancel()
        UART_readCancel(gUartHandle);
    }
}

static void processTddTimeoutCallback(UArg a0)
{

    Util_setEvent(&Tdd_events, TDD_OPEN_FAIL_EV);

       Semaphore_post(collectorSem);

}

static void tddOpenCallback(const TDD_cbArgs_t _cbArgs)
{
    Util_setEvent(&Tdd_events, TDD_OPEN_EV);

    Semaphore_post(collectorSem);

}

static void tddSetReqCallback(const TDD_cbArgs_t _cbArgs)
{
    Util_setEvent(&Tdd_events, TDD_SET_RSP_EV);

    Semaphore_post(collectorSem);

}

static void tddGetStatusCallback(const TDD_cbArgs_t _cbArgs)
{
    Util_setEvent(&Tdd_events, TDD_STATUS_RSP_EV);

    Semaphore_post(collectorSem);

}

static void Tdd_setTddClock(uint32_t tddTime)
{
    /* Stop the Tracking timer */
       if(UtilTimer_isActive(&tddClkStruct) == true)
       {
           UtilTimer_stop(&tddClkStruct);
       }

       if(tddTime)
       {
           /* Setup timer */
           UtilTimer_setTimeout(tddClkHandle, tddTime);
           UtilTimer_start(&tddClkStruct);
       }
}

