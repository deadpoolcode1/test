/*
 * crs_cli.c
 *
 *  Created on: 20 Nov 2021
 *      Author: avi
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/dpl/HwiP.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include <ti/drivers/dpl/SystemP.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include <ti/drivers/utils/Random.h>
#include <ti/drivers/apps/LED.h>
#include DeviceFamily_constructPath(driverlib/cpu.h)
#include "ti_drivers_config.h"
#include "crs_global_defines.h"
#include "application/util_timer.h"
#include "mac/api_mac.h"
#include "application/crs/crs_alarms.h"
#include "application/crs/crs_locks.h"
#include "crs.h"
#ifndef CLI_SENSOR
#include "application/collector.h"
#include "oad/crs_oad.h"
#else
#include "application/sensor.h"
#include "oad/native_oad/crs_oad_client.h"
#endif
#include "crs_cli.h"
#include "crs_nvs.h"
#include "application/crs/crs_fpga_uart.h"
#include "application/crs/snapshots/crs_flat_parser_spi.h"
#include "application/crs/snapshots/crs_script_dig_spi.h"
#include "application/crs/crs_agc_management.h"
#include "crs_cb_init_gain_states.h"
#include "easylink/EasyLink.h"
#include "application/crs/crs_fpga_spi.h"
#include "crs_tdd.h"
#include "crs_thresholds.h"
#include "crs_env.h"
#include "application/agc/agc.h"
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#ifdef CLI_SENSOR
#include "application/crs_msgs.h"
#endif

#ifdef CRS_TMP_SPI
#include "application/crs/snapshots/crs_snap_rf_spi.h"
#endif

#include "application/crs/snapshots/crs_script_returnvalues.h"
#include "logger/crs_logger.h"

#include "mac/mediator.h"
#ifdef CLI_CEU_CL
#include "application/crs_msgs.h"
#endif
//#ifdef defined(CLI_CEU_CL) || defined(CLI_CEU_BP)
//#endif
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define CLI_ESC_UP              "\033[A"
#define CLI_ESC_DOWN            "\033[B"
#define CLI_ESC_RIGHT           "\033[C"
#define CLI_ESC_LEFT            "\033[D"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#define CUI_NUM_UART_CHARS 1024
#define RSP_BUFFER_SIZE 1024

#define LAST_COMM_ARRAY_SIZE 10
#define LAST_COMM_COMM_SIZE 100
#define DATE_STR_LEN 10

#define CLI_MODE_SLAVE "slave"
#define CLI_MODE_NATIVE "native"

#define CLI_SNAP "snap"
#define CLI_SCRIPT "script"


#define CLI_PROMPT "\r\nCM> "
#define FPGA_PROMPT "AP>"
#define CONTROL_CH_STOP_TRANS '%'

#define CLI_CRS_HELP "help"
#define CLI_CRS_HELP_2  "help 2"

#ifndef CLI_SENSOR

#define CLI_FORM_NWK "form nwk"
#define CLI_OPEN_NWK "open nwk"
#define CLI_CLOSE_NWK "close nwk"

//#define CLI_DISCOVERY "discovery"

#define CLI_SEND_PING "send ping"
#define CLI_LIST_SENSORS "list units"
#define CLI_NWK_STATUS "nwk status"
#define CLI_LED_TOGGLE "led toggle"

#else
#define CLI_ASSOC "assoc"
#define CLI_DISASSOC "disassoc"

#endif

#define CLI_DEVICE "unit"
#define CLI_LIST_ALARMS_LIST "alarms list"
#define CLI_LIST_ALARMS_SET "alarms set"
#define CLI_LIST_ALARMS_STOP "alarms stop"
#define CLI_LIST_ALARMS_START "alarms start"

#define CLI_CRS_FPGA_OPEN "fpga open"
#define CLI_CRS_FPGA_CLOSE "fpga close"
#define CLI_CRS_FPGA_WRITE "fpga write"

#define CLI_CRS_FPGA_WRITELINES "fpga writelines"
#define CLI_CRS_FPGA_READLINES "fpga readlines"

#define CLI_CRS_FPGA_TRANSPARENT_START "uart bridge on"
#define CLI_CRS_FPGA_TRANSPARENT_END "uart bridge off"

#define CLI_CRS_TDD_OPEN "tdd open"
#define CLI_CRS_TDD_CLOSE "tdd close"
#define CLI_CRS_TDD_STATUS "tdd status"
#define CLI_CRS_TDD_SYNC_MODE "tdd set sync mode"
#define CLI_CRS_TDD_SCS "tdd set scs"
#define CLI_CRS_TDD_SS_POS "tdd set ss pos"
#define CLI_CRS_TDD_HOL "tdd set hol"
#define CLI_CRS_TDD_ALLOC "tdd set alloc"
#define CLI_CRS_TDD_FRAME "tdd set frame"
#define CLI_CRS_TDD_PERIOD1 "tdd set period1"
#define CLI_CRS_TDD_DL1 "tdd set dl1"
#define CLI_CRS_TDD_PATTERN2 "tdd set pattern2"
#define CLI_CRS_TDD_PERIOD2 "tdd set period2"
#define CLI_CRS_TDD_DL2 "tdd set dl2"

#define CLI_CRS_LOCKS "locks"
#define CLI_CRS_SET_DIO "set dio" //ment to set a GPIO/DIO value

#define CLI_CRS_RF_RUN  "rf run"
#define CLI_CRS_READ_RF_LINE    "read rf line"


#define CLI_CRS_TDD_TTG "tdd set ttg"
#define CLI_CRS_TDD_RTG "tdd set rtg"
#define CLI_CRS_TDD_CMD "tdd restart"
#define CLI_CRS_TDD_GET_LOCK "tdd get lock"

#define CLI_CRS_FS_INSERT "fs insert"
#define CLI_CRS_FS_LS "fs ls"
#define CLI_CRS_FS_READFILE  "fs cat"
#define CLI_CRS_FS_DELETE "fs rm"
#define CLI_CRS_FS_FORMAT "fs format"

#define CLI_CRS_FS_READLINE  "fs readline"
#define CLI_CRS_FS_NATIVE "fs native"

#define CLI_CRS_FS_UPLOAD_RF "fs upload rf"
#define CLI_CRS_FS_UPLOAD_DIG "fs upload dig"
#define CLI_CRS_FS_UPLOAD_FPGA "fs upload fpga"

#define CLI_CSV_SENSOR  "csv sensor"
#define CLI_AGC "sensor"
#define CLI_AGC_DEBUG "debug sensor"
#define CLI_AGC_MODE "set sensor"
#define CLI_AGC_GET_MODE "get sensor"
#define CLI_AGC_CHANNEL "set channel"

#define CLI_AGC_CHANNEL "set channel"

#define CLI_CRS_CONFIG_DIRECT "config direct"
//#define CLI_CRS_CONFIG_LINE "config line"
#define CLI_CRS_CONFIG_FILE "config file"
#define CLI_AGC_SET_GAP "agc set gap"
#define CLI_AGC_GET_GAP "agc get gap"
#define CLI_AGC_SET_TIME_MINMAX "agc set time"
#define CLI_AGC_GET_TIME_MINMAX "agc get time"

#define CLI_SCRIPT_RETVAL   "script retval"

#define CLI_CRS_ENV_LS "env ls"
#define CLI_CRS_ENV_UPDATE "env update"
#define CLI_CRS_ENV_RM "env rm"
#define CLI_CRS_ENV_RESTORE "env restore"
#define CLI_CRS_ENV_FORMAT "env format"

#define CLI_CRS_TRSH_LS "thrsh ls"
#define CLI_CRS_TRSH_UPDATE "thrsh update"
#define CLI_CRS_TRSH_RM "thrsh rm"
#define CLI_CRS_TRSH_RESTORE "thrsh restore"
#define CLI_CRS_TRSH_FORMAT "thrsh format"

#define CLI_CRS_INIT_GAIN_LS "init gain ls"
#define CLI_CRS_INIT_GAIN_UPDATE "init gain update"
#define CLI_CRS_INIT_GAIN_RM "init gain rm"
#define CLI_CRS_INIT_GAIN_RESTORE "init gain restore"
#define CLI_CRS_INIT_GAIN_FORMAT "init gain format"


#define CLI_CRS_GET_TIME "time"
#define CLI_CRS_SET_TIME "set time"

#define CLI_CRS_GET_GAIN "get gain"

#define CLI_CRS_WATCHDOG_DISABLE "watchdog disable"

#define CLI_CRS_MODEM_TEST "modem test"

//#define CLI_CRS_DEBUG "fs debug"
#define CLI_CRS_SPI_DEBUG   "fs debug spi" // todo

#define CLI_CRS_TMP "tmp"
#define CLI_CRS_DEBUG_RSSI "debug rssi"

#define CLI_CRS_RSSI "rssi"
#define CLI_CRS_RSSI_CHECK "rssi check"

#define CLI_DISCOVER_MODULES "discover modules"


#ifndef CLI_SENSOR
#define CLI_CRS_OAD_RCV_IMG "oad rcv img" //sending via UART a sensor img to extFlash
#define CLI_CRS_OAD_SEND_IMG "oad send img" //sending the oad img residing in the extFlash OverTheAir to the sensor extFlash
#define CLI_CRS_UPDATE_IMG "update img" //sending via UART the collector img to extFlash and upgrading it
#define CLI_CRS_OAD_GET_IMG_VER "oad get img" //getting the img version residing in extFlash
#define CLI_CRS_OAD_RCV_FACTORY_IMG "oad rcv factory img" //sending via UART a img into the extFlash factory slot
#endif
#define CLI_CRS_OAD_FACTORY_IMG "oad factory img" //backing up the current running img into extFlash factory slot
#define CLI_CRS_OAD_FACTORY_CHECK "oad factory check" //checking if factory img exists or not on extFlash

#define CLI_CRS_OAD_INVALID "oad invalid img" //making the current running img be invalid for the next boot
#define CLI_CRS_OAD_FORMAT "oad format"


#define CLI_CRS_LED_MODE "ledMode"
#define CLI_CRS_LED_ON "led on"
#define CLI_CRS_LED_OFF "led off"

#define CLI_CRS_RESET "reset"
#define CLI_LOGGER_GET  "logger get"
#define CLI_LOGGER_SET_LEVEL "logger set level"
#define CLI_LOGGER_START "logger start"
#define CLI_LOGGER_STOP "logger stop"
#define CLI_LOGGER_FLUSH    "logger flush"
#define CLI_LOGGER_SET_BUFFER_TYPE "logger set buffer type"
#define CLI_LOGGER_GET_BUFFER_TYPE "logger get buffer type"
#define CLI_LOGGER_GET_TIME "logger get time"
#define CLI_LOGGER_GET_LEVEL "logger get level"

#ifdef CLI_CEU_BP
#define CLI_CRS_UART_BP_COMM "uart bp"
#endif
/******************************************************************************
 Local variables
 *****************************************************************************/

//static uint8_t *gTmp = CLI_ESC_UP;
#ifndef CLI_SENSOR
//static Cllc_associated_devices_t gCllc_associatedDevListLocal[4];
#endif
int8_t gRssiAvg=0;
#ifndef CLI_SENSOR

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static CRS_retVal_t CLI_formNwkParsing(char *line);
static CRS_retVal_t CLI_openNwkParsing(char *line);
static CRS_retVal_t CLI_closeNwkParsing(char *line);

static CRS_retVal_t CLI_listSensorsParsing(char *line);



#else
static CRS_retVal_t CLI_associate(char *line);
static CRS_retVal_t CLI_disassociate(char *line);

#endif
static CRS_retVal_t CLI_unit(char *line);


static CRS_retVal_t CLI_AlarmsListParsing(char *line);
static CRS_retVal_t CLI_AlarmsSetParsing(char *line);


static CRS_retVal_t CLI_fpgaOpenParsing(char *line);
static CRS_retVal_t CLI_fpgaCloseParsing(char *line);
static CRS_retVal_t CLI_fpgaWriteLinesParsing(char *line);
static CRS_retVal_t CLI_fpgaReadLinesParsing(char *line);

static CRS_retVal_t CLI_fpgaTransStartParsing(char *line);
static CRS_retVal_t CLI_fpgaTransStopParsing(char *line);

static CRS_retVal_t CLI_tddCloseParsing(char *line);
static CRS_retVal_t CLI_tddOpenParsing(char *line);
static CRS_retVal_t CLI_tddStatusParsing(char *line);
static CRS_retVal_t CLI_tddSetSyncModeParsing(char *line);
static CRS_retVal_t CLI_tddSetScsParsing(char *line);
static CRS_retVal_t CLI_tddSetSsPosParsing(char *line);
static CRS_retVal_t CLI_tddSetHolParsing(char *line);
static CRS_retVal_t CLI_tddSetFrameParsing(char *line);
static CRS_retVal_t CLI_tddSetAllocParsing(char *line);
static CRS_retVal_t CLI_tddSetDl1Parsing(char *line);
static CRS_retVal_t CLI_tddSetPeriod1Parsing(char *line);
static CRS_retVal_t CLI_tddSetPattern2Parsing(char *line);
static CRS_retVal_t CLI_tddSetPeriod2Parsing(char *line);
static CRS_retVal_t CLI_tddSetDl2Parsing(char *line);


static CRS_retVal_t CLI_RfRunParsing(char *line);
static CRS_retVal_t CLI_readRfLineParsing(char *line);

static CRS_retVal_t CLI_tddSetTtgParsing(char *line);
static CRS_retVal_t CLI_tddSetRtgParsing(char *line);
static CRS_retVal_t CLI_tddCommandParsing(char *line);
static CRS_retVal_t CLI_tddGetLockParsing(char *line);

static CRS_retVal_t CLI_scriptRetValParsing(char *line);

static CRS_retVal_t CLI_fsInsertParsing(char *line);
static CRS_retVal_t CLI_fsLsParsing(char *line);
static CRS_retVal_t CLI_fsReadLineParsing(char *line);
static CRS_retVal_t CLI_fsDeleteParsing(char *line);
static CRS_retVal_t CLI_fsUploadRfParsing(char *line);
static CRS_retVal_t CLI_fsUploadDigParsing(char *line);
static CRS_retVal_t CLI_fsUploadFpgaParsing(char *line);
static CRS_retVal_t CLI_fsReadFileParsing(char *line);
static CRS_retVal_t CLI_fsReadFileNative(char *line);
static CRS_retVal_t CLI_fsFormat(char *line);
static CRS_retVal_t CLI_sensorsParsing(char *line);
static CRS_retVal_t CLI_sensorsDebugParsing(char *line);
static CRS_retVal_t CLI_csvSensorParsing(char *line);
static CRS_retVal_t CLI_sensorModeParsing(char *line);
static CRS_retVal_t CLI_sensorGetModeParsing(char *line);
static CRS_retVal_t CLI_sensorChannelParsing(char *line);
static CRS_retVal_t CLI_agcSetGapParsing(char *line);
static CRS_retVal_t CLI_agcGetGapParsing(char *line);
static CRS_retVal_t CLI_agcSetTimeMinMaxParsing(char *line);
static CRS_retVal_t CLI_agcGetTimeMinMaxParsing(char *line);
static CRS_retVal_t CLI_setDioParsing(char *line);



static CRS_retVal_t CLI_locksParsing(char *line);
static void printLocksStatus(void);

static CRS_retVal_t CLI_envUpdate(char *line);
static CRS_retVal_t CLI_envRm(char *line);
static CRS_retVal_t CLI_envLs(char *line);
static CRS_retVal_t CLI_envFormat(char *line);
static CRS_retVal_t CLI_envRestore(char *line);

static CRS_retVal_t CLI_trshUpdate(char *line);
static CRS_retVal_t CLI_trshRm(char *line);
static CRS_retVal_t CLI_trshLs(char *line);
static CRS_retVal_t CLI_trshFormat(char *line);
static CRS_retVal_t CLI_trshRestore(char *line);


static CRS_retVal_t CLI_cigsUpdate(char *line);
static CRS_retVal_t CLI_cigsRm(char *line);
static CRS_retVal_t CLI_cigsLs(char *line);
static CRS_retVal_t CLI_cigsFormat(char *line);
static CRS_retVal_t CLI_cigsRestore(char *line);


static CRS_retVal_t CLI_getTimeParsing(char *line);
static CRS_retVal_t CLI_setTimeParsing(char *line);


static CRS_retVal_t CLI_getGainParsing(char *line);

static CRS_retVal_t CLI_config_file(char *line);
//static CRS_retVal_t CLI_config_direct(char *line);
//static CRS_retVal_t CLI_config_line(char *line);

//static CRS_retVal_t CLI_config_direct_spi(char *line);
//static CRS_retVal_t CLI_config_file_spi(char *line);
//static CRS_retVal_t CLI_config_line_spi(char *line);
//





static CRS_retVal_t CLI_modemTest(char *line);
static CRS_retVal_t CLI_discoverModules(char *line);




static CRS_retVal_t CLI_discoverModules_spi(char *line);



#ifndef CLI_SENSOR
static CRS_retVal_t CLI_OadSendImgParsing(char *line);
static CRS_retVal_t CLI_OadGetImgParsing(char *line);
#endif
static CRS_retVal_t CLI_OadResetParsing(char *line);
static CRS_retVal_t CLI_loggerGetParsing(char *line);
static CRS_retVal_t CLI_loggerSetLevelParsing(char *line);
static CRS_retVal_t CLI_loggerStartParsing(char *line);
static CRS_retVal_t CLI_loggerStopParsing(char *line);
static CRS_retVal_t CLI_loggerFlushParsing(char *line);
static CRS_retVal_t CLI_loggerSetBufferTypeParsing(char *line);
static CRS_retVal_t CLI_loggerGetBufferTypeParsing(char *line);
static CRS_retVal_t CLI_loggerGetTimeParsing(char *line);
static CRS_retVal_t CLI_loggerGetLevelParsing(char *line);

static CRS_retVal_t CLI_watchdogDisableParsing(char *line);

static CRS_retVal_t CLI_tmpParsing(char *line);

static CRS_retVal_t CLI_rssiParsing(char *line);

static CRS_retVal_t CLI_ledModeParsing(char *line);
static CRS_retVal_t CLI_ledOnParsing(char *line);
static CRS_retVal_t CLI_ledOffParsing(char *line);
static CRS_retVal_t CLI_evtCntrParsing(char *line);

static void tddCallback(const TDD_cbArgs_t _cbArgs);
static void tddOpenCallback(const TDD_cbArgs_t _cbArgs);

static CRS_retVal_t CLI_helpParsing(char *line);
static CRS_retVal_t CLI_help2Parsing(char *line);
static uint32_t MakeULFromHex(char *hex_str);

static CRS_retVal_t CLI_printCommInfo(char *command, uint32_t commSize, char* description);
static CRS_retVal_t CLI_convertExtAddrTo2Uint32(ApiMac_sAddrExt_t  *extAddr, uint32_t* left, uint32_t* right);
static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size);
static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);
static CRS_retVal_t CLI_writeString(void *_buffer, size_t _size);
static int CLI_hex2int(char ch);
static void fpgaMultiLineCallback(const FPGA_cbArgs_t _cbArgs);

static void convertUintToDate(uint32_t date, char dateStr[DATE_STR_LEN]);

static CRS_retVal_t defaultTestLog( const log_level level, const char* file, const int line, const char* format, ... );
CLI_log_handler_func_type*  glogHandler = &defaultTestLog;
/*******************************************************************************
 * GLOBAL VARIABLES
 */
/*
 * [General Global Variables]
 */
static bool gModuleInitialized = false;

/*
 * [UART Specific Global Variables]
 */
static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[CUI_NUM_UART_CHARS] = { 0 };
static uint32_t gUartTxBufferIdx = 0;

static uint8_t gUartRxBuffer[2] = { 0 };

//gUartRxBuffer
//static SemaphoreP_Handle gUartSem;
//static SemaphoreP_Struct gUartSemStruct;

#define UART_WRITE_BUFF_SIZE 2500

static volatile uint8_t gWriteNowBuff[UART_WRITE_BUFF_SIZE];
static volatile uint8_t gWriteWaitingBuff[UART_WRITE_BUFF_SIZE];

static volatile uint32_t gWriteNowBuffIdx = 0;
static volatile uint32_t gWriteNowBuffSize = 0;
static volatile uint32_t gWriteWaitingBuffIdx = 0;

//static uint32_t gWriteNowBuffTotalSize = 0;

static volatile bool gIsDoneWriting = true;
static volatile bool gIsDoneFilling = false;
static volatile bool gIsNoPlaceForPrompt = false;



//for transparent bridge
static volatile bool gIsTransparentBridge = false;
static volatile bool gIsTransparentBridgeSuspended = false;
#ifndef CLI_SENSOR
static uint32_t gTransparentShortAddr = 0;
static uint32_t gRspBuffIdx = 0;
#endif
static volatile bool gIsAsyncCommand = false;



#ifdef CLI_SENSOR
static uint8_t gRspBuff[RSP_BUFFER_SIZE] = { 0 };
static uint16_t gDstAddr;
#endif

static volatile bool gIsRemoteCommand = false;
volatile bool gIsUartCommCommand = false;
volatile bool gIsUartCommCommandRemoteCL = false;

static volatile bool gIsRemoteTransparentBridge = false;

static volatile bool gIsTranRemoteCommandSent = false;

volatile bool gIsUartCommInParts=false;
static bool gReadNextCommand = false;

//static int8_t gRssi = 0;


//static uint32_t gUartLastCommBuffSize = 0;
//static bool gIsNewCommand = true;

static uint8_t gTmpUartTxBuffer[LAST_COMM_COMM_SIZE] = { 0 };
#ifndef CLI_SENSOR
static uint8_t gCopyUartTxBuffer[LAST_COMM_COMM_SIZE] = { 0 };
#endif

/******************************************************************************
 * Public CLI APIs
 *****************************************************************************/

CRS_retVal_t CLI_close(){
    if (NULL == gUartHandle)
        {
            return CRS_FAILURE;
        }
        else
        {

           UART_close(gUartHandle);
           gModuleInitialized=false;
    gUartHandle=NULL;
        }
    return CRS_SUCCESS;
}


void errorHandle(){
    Oad_invalidateImg();
    SysCtrlSystemReset();
}


/*********************************************************************
 * @fn          CLI_init
 *
 * @brief       Initialize the CLI module. This function must be called
 *                  before any other CUI functions.
 *
 *
 *
 * @return      CRS_retVal_t representing success or failure.
 */
CRS_retVal_t CLI_init(bool restartMsg)
{
//    int i=0;
//    char bugArr[2];
//    while(1){
//        bugArr[i]=100;
//        i++;
//        if (i==1000) {
//            break;
//        }
//    }

    /*
     *  Do nothing if the module has already been initialized or if
     *  CLI_init has been called without trying to manage any of the three
     *  resources (btns, leds, uart)
     */
    if (!gModuleInitialized)
    {
        // Semaphore Setup
        //  SemaphoreP_Params_init(&gSemParams);
        //set all sems in this module to be binary sems
        //gSemParams.mode = SemaphoreP_Mode_BINARY;

        {
            // UART semaphore setup
            // gUartSem = SemaphoreP_construct(&gUartSemStruct, 1, &gSemParams);

            {
                // General UART setup
                UART_init();
                UART_Params_init(&gUartParams);
                gUartParams.baudRate = 115200;
                gUartParams.writeMode = UART_MODE_CALLBACK;
                gUartParams.writeDataMode = UART_DATA_BINARY;
                gUartParams.writeCallback = UartWriteCallback;
                gUartParams.readMode = UART_MODE_CALLBACK;
                gUartParams.readDataMode = UART_DATA_BINARY;
                gUartParams.readCallback = UartReadCallback;

                gUartHandle = UART_open(CONFIG_DISPLAY_UART, &gUartParams);
                if (NULL == gUartHandle)
                {
                    return CRS_FAILURE;
                }
                else
                {
                    UART_read(gUartHandle, gUartRxBuffer, 1);

                }
            }

        }

        gModuleInitialized = true;
if(restartMsg){

#ifdef CLI_CEU_BP
    CLI_writeString("\r\n------Restart CEU_BP------", sizeof("\r\n------Restart CEU_BP------"));
#endif
#ifdef CLI_CEU_CL
    CLI_writeString("\r\n------Restart CEU_CL------", sizeof("\r\n------Restart CEU_CL------"));
#endif
#ifndef CLI_SENSOR
//        CLI_writeString("\r\n------Restart Collector------", sizeof("\r\n------Restart Collector------"));
#else
        CLI_writeString("\r\n------Restart Sensor------", sizeof("\r\n------Restart Sensor------"));
#endif
#ifndef CRS_TMP_SPI
        CLI_writeString(" No SPI", sizeof(" No SPI"));


#endif
}
//        CLI_writeString(CLI_PROMPT, sizeof(CLI_PROMPT));
        return CRS_SUCCESS;
    }

    return CRS_FAILURE;
}
#ifndef CLI_SENSOR
CRS_retVal_t CLI_processCliSendMsgUpdate(void)
{
    ApiMac_sAddr_t dstAddr;
    dstAddr.addr.shortAddr = gTransparentShortAddr;
    dstAddr.addrMode = ApiMac_addrType_short;
    Collector_status_t stat;
//    char tmp[CUI_NUM_UART_CHARS] = {0};
//    gUartTxBuffer[gUartTxBufferIdx - 1] = 0;
//    sprintf(tmp, "fpga%s", gTransparentShortAddr, gUartTxBuffer);
//    gUartTxBuffer[gUartTxBufferIdx - 1] = '\r';
//    CLI_cliPrintf("\r\nsending:%s",gUartTxBuffer);
    stat = Collector_sendCrsMsg(&dstAddr, gUartTxBuffer);
    if (stat != Collector_status_success)
    {
        UART_read(gUartHandle, gUartRxBuffer, 1);
        CLI_writeString("\r\nno connection", strlen("\r\nno connection"));
    }
//    CLI_startREAD();
    memcpy(gCopyUartTxBuffer, gUartTxBuffer, LAST_COMM_COMM_SIZE);
//    gIsNewCommand = true;
    memset(gUartTxBuffer, 0, CUI_NUM_UART_CHARS - 1);
    memset(gTmpUartTxBuffer, 0, LAST_COMM_COMM_SIZE - 1);

    gUartTxBufferIdx = 0;
    gIsRemoteCommand = false;
    gIsTranRemoteCommandSent = true;
//    memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
//    gUartTxBufferIdx = 0;
    return CRS_SUCCESS;
}
#endif

CRS_retVal_t CLI_processCliUpdate(char *line, uint16_t pDstAddr)
{

    if (!gModuleInitialized)
    {
        return CRS_FAILURE;
    }

    if (line == NULL)
    {
        line = (char *)gUartTxBuffer;
        gUartTxBuffer[gUartTxBufferIdx - 1] = 0;

    }
    else if(pDstAddr!=NULL)
    {
        gIsRemoteCommand = true;
#ifdef CLI_SENSOR
        gDstAddr = pDstAddr;
#endif

    }else{//uartCommRcv

        gIsUartCommCommand=true;
#ifdef CLI_CEU_BP
        CLI_cliPrintf("%s",line);
#endif
    }

    if (gIsTransparentBridge == true && line[0] != CONTROL_CH_STOP_TRANS)
    {
        if (line[0] == 0)
        {
            line[0] = '\r';
        }
        if (line[strlen(line) - 1] != '\r')
        {
            line[strlen(line)] = '\r';
        }

        if (memcmp(CLI_CRS_FPGA_TRANSPARENT_END, line,
                   sizeof(CLI_CRS_FPGA_TRANSPARENT_END) - 1) == 0)
        {
            CLI_fpgaTransStopParsing(line);
            return CRS_SUCCESS;

        }

        Fpga_UART_transparentWrite(line, strlen(line));

        return CRS_SUCCESS;

    }
    //CUI_writeString(cliPrompt, sizeof(cliPrompt));

    const char badInputMsg[] = "\r\nINVALID INPUT";

    bool inputBad = true;

    bool is_async_command = false;
#ifdef CLI_CEU_BP
if (gIsUartCommCommand==true) {
//    inputBad=false;
//    is_async_command = true;
}
#endif
#ifndef CLI_SENSOR
    if (memcmp(CLI_FORM_NWK, line, sizeof(CLI_FORM_NWK)) == 0)
    {
        CLI_formNwkParsing(line);
        inputBad = false;
        is_async_command = true;
    }

    if (memcmp(CLI_OPEN_NWK, line, sizeof(CLI_OPEN_NWK)) == 0)
    {
        CLI_openNwkParsing(line);

        inputBad = false;
        is_async_command = true;
    }

    if (memcmp(CLI_CLOSE_NWK, line, sizeof(CLI_CLOSE_NWK)) == 0)
    {

        CLI_closeNwkParsing(line);

        inputBad = false;
        is_async_command = true;
    }

    if (memcmp(CLI_LIST_SENSORS, line, sizeof(CLI_LIST_SENSORS)) == 0)
    {

        CLI_listSensorsParsing(line);

        inputBad = false;
//        CLI_startREAD();

        //is_async_command = true;
    }




    //    if (memcmp(CLI_SEND_PING, gUartTxBuffer, sizeof(CLI_SEND_PING)) == 0)
    //          {
    //        CUI_cliArgs_t cliArgs;
    //
    //
    //        (gCliFnArray[CLI_SENDPING_IDX])(cliArgs);
    //              inputBad = false;
    //              is_async_command = true;
    //
    //          }
    //
    //      if (memcmp(CLI_LIST_SENSORS, gUartTxBuffer, sizeof(CLI_LIST_SENSORS)) == 0)
    //              {
    //          CUI_cliArgs_t cliArgs;
    //
    //                  (gCliFnArray[CLI_GET_ALL_SENSORS])(cliArgs);
    //                  inputBad = false;
    //              }
    //
    //      if (memcmp(CLI_NWK_STATUS, gUartTxBuffer, sizeof(CLI_NWK_STATUS)) == 0)
    //              {
    //          CUI_cliArgs_t cliArgs;
    //
    //                  (gCliFnArray[CLI_GET_NWK_STATUS])(cliArgs);
    //                  inputBad = false;
    //              }
    //
    //      if (memcmp(CLI_LED_TOGGLE, gUartTxBuffer, sizeof(CLI_LED_TOGGLE)) == 0)
    //             {
    //          CUI_cliArgs_t cliArgs;
    //
    //                 (gCliFnArray[CLI_SENDTOGGLE_IDX])(cliArgs);
    //                 inputBad = false;
    //             }

#else
    if (memcmp(CLI_ASSOC, line, sizeof(CLI_ASSOC) - 1) == 0)
    {

        CLI_associate(line);
        is_async_command = true;

        inputBad = false;
    }
    if (memcmp(CLI_DISASSOC, line, sizeof(CLI_DISASSOC) - 1) == 0)
    {
        CLI_disassociate(line);
        is_async_command = true;
        CLI_startREAD();
        inputBad = false;
    }
#endif
//#define CLI_DEVICE "device"

    if (memcmp(CLI_DEVICE, line, sizeof(CLI_DEVICE) - 1) == 0)
      {

          CLI_unit(line);
          inputBad = false;
      }



      if (memcmp(CLI_CRS_ENV_UPDATE, line, sizeof(CLI_CRS_ENV_UPDATE) - 1) == 0)
         {

          CLI_envUpdate(line);

             inputBad = false;
         }

      if (memcmp(CLI_CRS_ENV_RM, line, sizeof(CLI_CRS_ENV_RM) - 1) == 0)
           {

          CLI_envRm(line);

               inputBad = false;
           }

      if (memcmp(CLI_CRS_ENV_LS, line, sizeof(CLI_CRS_ENV_LS) - 1) == 0)
           {

          CLI_envLs(line);

               inputBad = false;
           }
      if (memcmp(CLI_CRS_ENV_FORMAT, line, sizeof(CLI_CRS_ENV_FORMAT) - 1) == 0)
           {

          CLI_envFormat(line);

               inputBad = false;
           }

      if (memcmp(CLI_CRS_ENV_RESTORE, line, sizeof(CLI_CRS_ENV_RESTORE) - 1) == 0)
           {

          CLI_envRestore(line);

               inputBad = false;
           }

      if (memcmp(CLI_SCRIPT_RETVAL,line,sizeof(CLI_SCRIPT_RETVAL) - 1) == 0)
      {
          CLI_scriptRetValParsing(line);
          inputBad = false;
      }


      if (memcmp(CLI_CRS_TRSH_UPDATE, line, sizeof(CLI_CRS_TRSH_UPDATE) - 1) == 0)
             {

              CLI_trshUpdate(line);

                 inputBad = false;
             }

          if (memcmp(CLI_CRS_TRSH_RM, line, sizeof(CLI_CRS_TRSH_RM) - 1) == 0)
               {

              CLI_trshRm(line);

                   inputBad = false;
               }

          if (memcmp(CLI_CRS_TRSH_LS, line, sizeof(CLI_CRS_TRSH_LS) - 1) == 0)
               {

              CLI_trshLs(line);

                   inputBad = false;
               }

          if (memcmp(CLI_CRS_TRSH_FORMAT, line, sizeof(CLI_CRS_TRSH_FORMAT) - 1) == 0)
               {

              CLI_trshFormat(line);

                   inputBad = false;
               }

          if (memcmp(CLI_CRS_TRSH_RESTORE, line, sizeof(CLI_CRS_TRSH_RESTORE) - 1) == 0)
               {

              CLI_trshRestore(line);

                   inputBad = false;
               }






          if (memcmp(CLI_CRS_INIT_GAIN_UPDATE, line, sizeof(CLI_CRS_INIT_GAIN_UPDATE) - 1) == 0)
                 {

                  CLI_cigsUpdate(line);

                     inputBad = false;
                 }

              if (memcmp(CLI_CRS_INIT_GAIN_RM, line, sizeof(CLI_CRS_INIT_GAIN_RM) - 1) == 0)
                   {

                  CLI_cigsRm(line);

                       inputBad = false;
                   }

              if (memcmp(CLI_CRS_INIT_GAIN_LS, line, sizeof(CLI_CRS_INIT_GAIN_LS) - 1) == 0)
                   {

                  CLI_cigsLs(line);

                       inputBad = false;
                   }

              if (memcmp(CLI_CRS_INIT_GAIN_FORMAT, line, sizeof(CLI_CRS_INIT_GAIN_FORMAT) - 1) == 0)
                   {

                  CLI_cigsFormat(line);

                       inputBad = false;
                   }

              if (memcmp(CLI_CRS_INIT_GAIN_RESTORE, line, sizeof(CLI_CRS_INIT_GAIN_RESTORE) - 1) == 0)
                   {

                  CLI_cigsRestore(line);

                       inputBad = false;
                   }







          if (memcmp(CLI_CRS_SET_TIME, line, sizeof(CLI_CRS_SET_TIME) - 1) == 0)
              {

                  CLI_setTimeParsing(line);

                        inputBad = false;
              }

          if (memcmp(CLI_CRS_GET_TIME, line, sizeof(CLI_CRS_GET_TIME) - 1) == 0)
              {

                  CLI_getTimeParsing(line);

                    inputBad = false;
              }

          if (memcmp(CLI_CRS_GET_GAIN, line, sizeof(CLI_CRS_GET_GAIN) - 1) == 0)
                  {

                      CLI_getGainParsing(line);

                        inputBad = false;
                  }


          if (memcmp(CLI_CRS_TDD_OPEN, line, sizeof(CLI_CRS_TDD_OPEN) - 1) == 0)
          {
              CRS_retVal_t retStatus = CLI_tddOpenParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_CLOSE, line, sizeof(CLI_CRS_TDD_CLOSE) - 1) == 0)
          {

              CLI_tddCloseParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_STATUS, line, sizeof(CLI_CRS_TDD_STATUS) - 1) == 0)
          {

              CLI_tddStatusParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_SYNC_MODE, line, sizeof(CLI_CRS_TDD_SYNC_MODE) - 1) == 0)
          {

              CLI_tddSetSyncModeParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_SCS, line, sizeof(CLI_CRS_TDD_SCS) - 1) == 0)
          {

              CLI_tddSetScsParsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_SS_POS, line, sizeof(CLI_CRS_TDD_SS_POS) - 1) == 0)
          {

               CLI_tddSetSsPosParsing(line);

               inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_ALLOC, line, sizeof(CLI_CRS_TDD_ALLOC) - 1) == 0)
          {

              CLI_tddSetAllocParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_FRAME, line, sizeof(CLI_CRS_TDD_FRAME) - 1) == 0)
          {

              CLI_tddSetFrameParsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_PERIOD1, line, sizeof(CLI_CRS_TDD_PERIOD1) - 1) == 0)
          {

              CLI_tddSetPeriod1Parsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_DL1, line, sizeof(CLI_CRS_TDD_DL1) - 1) == 0)
          {

              CLI_tddSetDl1Parsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_DL2, line, sizeof(CLI_CRS_TDD_DL2) - 1) == 0)
          {

              CLI_tddSetDl2Parsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_PERIOD2, line, sizeof(CLI_CRS_TDD_PERIOD2) - 1) == 0)
          {

              CLI_tddSetPeriod2Parsing(line);

              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_PATTERN2, line, sizeof(CLI_CRS_TDD_PATTERN2) - 1) == 0)
          {

              CLI_tddSetPattern2Parsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_RF_RUN, line, sizeof(CLI_CRS_RF_RUN) - 1) == 0)  //new
          {

              if (CRS_SUCCESS == CLI_RfRunParsing(line))
              {
                  is_async_command = true;
                  CLI_startREAD();
              }
              inputBad = false;
          }

          if (memcmp(CLI_CRS_READ_RF_LINE, line, strlen(CLI_CRS_READ_RF_LINE)) == 0)
          {
              CLI_readRfLineParsing(line);
              CLI_startREAD();
              inputBad = false;
          }

          if (memcmp(CLI_CRS_TDD_HOL, line, sizeof(CLI_CRS_TDD_HOL) - 1) == 0)
          {

              CLI_tddSetHolParsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_TTG, line, sizeof(CLI_CRS_TDD_TTG) - 1) == 0)
          {

              CLI_tddSetTtgParsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_RTG, line, sizeof(CLI_CRS_TDD_RTG) - 1) == 0)
          {

              CLI_tddSetRtgParsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_CMD, line, sizeof(CLI_CRS_TDD_CMD) - 1) == 0)
          {

              CLI_tddCommandParsing(line);

              inputBad = false;
          }
          if (memcmp(CLI_CRS_TDD_GET_LOCK, line, sizeof(CLI_CRS_TDD_GET_LOCK) - 1) == 0)
          {

              CLI_tddGetLockParsing(line);

              inputBad = false;
          }


      if (memcmp(CLI_CRS_FPGA_OPEN, line, sizeof(CLI_CRS_FPGA_OPEN) - 1) == 0)
      {
          CRS_retVal_t retStatus = CLI_fpgaOpenParsing(line);
          if (retStatus == CRS_SUCCESS)
          {
              is_async_command = true;
          }
          else
          {
              CLI_startREAD();
          }
          inputBad = false;
      }



      if (memcmp (CLI_CRS_LOCKS, line, sizeof(CLI_CRS_LOCKS) - 1) == 0)
      {
          CLI_locksParsing(line);

          inputBad = false;
      }



      if (memcmp(CLI_CRS_FPGA_WRITELINES, line,
                 sizeof(CLI_CRS_FPGA_WRITELINES) - 1) == 0)
      {
          CLI_fpgaWriteLinesParsing(line);

          is_async_command = true;

          inputBad = false;
      }
      if (memcmp(CLI_CRS_FPGA_READLINES, line,
                    sizeof(CLI_CRS_FPGA_READLINES) - 1) == 0)
         {
             CLI_fpgaReadLinesParsing(line);

             is_async_command = true;

             inputBad = false;
         }

      if (memcmp(CLI_CRS_FPGA_CLOSE, line, sizeof(CLI_CRS_FPGA_CLOSE) - 1) == 0)
      {
          CLI_fpgaCloseParsing(line);

          inputBad = false;
      }
      if (memcmp(CLI_CRS_FPGA_TRANSPARENT_START, line,
                 sizeof(CLI_CRS_FPGA_TRANSPARENT_START) - 1) == 0)
      {
          CLI_fpgaTransStartParsing(line);

          inputBad = false;
      }

      if (memcmp(CLI_CRS_FPGA_TRANSPARENT_END, line,
                 sizeof(CLI_CRS_FPGA_TRANSPARENT_END) - 1) == 0)
      {
          CLI_fpgaTransStopParsing(line);
          inputBad = false;

      }


      if (memcmp(CLI_LIST_ALARMS_LIST, line, sizeof(CLI_LIST_ALARMS_LIST) - 1) == 0)
        {

         CRS_retVal_t retStatus= CLI_AlarmsListParsing(line);

         if (retStatus == CRS_SUCCESS)
             {
                 is_async_command = true;
             }
             else
             {
                 CLI_startREAD();
             }
             inputBad = false;

        }
      if (memcmp(CLI_LIST_ALARMS_SET, line, sizeof(CLI_LIST_ALARMS_SET) - 1) == 0)
          {

            CLI_AlarmsSetParsing(line);
              inputBad = false;

          }

      if (memcmp(CLI_LIST_ALARMS_STOP, line, sizeof(CLI_LIST_ALARMS_STOP) - 1) == 0)
             {

                 Alarms_stopPooling();
                 inputBad = false;
                 CLI_startREAD();
             }
      if (memcmp(CLI_LIST_ALARMS_START, line, sizeof(CLI_LIST_ALARMS_START) - 1) == 0)
               {

                   Alarms_startPooling();
                   inputBad = false;
                   CLI_startREAD();
               }


      if (memcmp(CLI_CRS_RSSI_CHECK, line, sizeof(CLI_CRS_RSSI_CHECK) - 1) == 0)
               {
          int32_t rssiAvg=0;
          char tempLine[512]={0};
             memcpy(tempLine,line,strlen(line));
             const char s[2] = " ";
                   char *token;
                   /* get the first token */
                      token = strtok(tempLine, s);//rssi
                      token = strtok(NULL, s);//check
                      token = strtok(NULL, s);//rssiAvg value
                      rssiAvg= strtoul(token+2,NULL,10);
                   Alarms_checkRssi(rssiAvg);
                   inputBad = false;
                   CLI_startREAD();
               }


      if (memcmp(CLI_CRS_DEBUG_RSSI, line, sizeof(CLI_CRS_DEBUG_RSSI) - 1) == 0)
               {
          int8_t gRssiAvg=9;
          EasyLink_getRssi(&gRssiAvg);
          CLI_cliPrintf("\r\nrssi: 0x%x",gRssiAvg);
                   inputBad = false;
                   CLI_startREAD();
               }

#ifndef CLI_SENSOR
      if (memcmp(CLI_CRS_OAD_RCV_IMG, line, sizeof(CLI_CRS_OAD_RCV_IMG) - 1) == 0)
               {

                  Oad_distributorUartRcvImg(false,false);
                   inputBad = false;
//                   CLI_startREAD();
               }

      if (memcmp(CLI_CRS_UPDATE_IMG, line, sizeof(CLI_CRS_UPDATE_IMG) - 1) == 0)
               {
          uint16_t isResetInt=0;
          char tempLine[512]={0};
                  memcpy(tempLine,line,strlen(line));
                  const char s[2] = " ";
                  char *token;
                  /* get the first token */
                  token = strtok(tempLine, s);//update
                  token = strtok(NULL, s);//img
                  token = strtok(NULL, s);//isReset
                  if (token==NULL) {
                      inputBad = true;
                }else{
                  isResetInt= strtoul(token+2,NULL,10);
                  if (isResetInt) {
                      Oad_distributorUartRcvImg(true,false);
                }else{
                    Oad_distributorUartRcvImg(false,false);
                }
                   inputBad = false;
//                   CLI_startREAD();
                }
               }

      if (memcmp(CLI_CRS_OAD_SEND_IMG, line, sizeof(CLI_CRS_OAD_SEND_IMG) - 1) == 0)
                 {

            CLI_OadSendImgParsing(line);
                     inputBad = false;
                     //CLI_startREAD();
                 }

      if (memcmp(CLI_CRS_OAD_GET_IMG_VER, line, sizeof(CLI_CRS_OAD_GET_IMG_VER) - 1) == 0)
               {
          CLI_OadGetImgParsing(line);

                   inputBad = false;
                   //CLI_startREAD();
               }


      if (memcmp(CLI_CRS_OAD_RCV_FACTORY_IMG, line, sizeof(CLI_CRS_OAD_RCV_FACTORY_IMG) - 1) == 0)
                {

           CRS_retVal_t rsp=  Oad_distributorUartRcvImg(false,true);
           CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                    inputBad = false;
                    CLI_startREAD();
                }

#endif
      if (memcmp(CLI_CRS_OAD_FACTORY_IMG, line, sizeof(CLI_CRS_OAD_FACTORY_IMG) - 1) == 0)
               {
          uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_OAD_FACTORY_IMG) + 2]), NULL,
                                                  16);
                 #ifndef CLI_SENSOR

                     uint16_t addr = 0;
                     Cllc_getFfdShortAddr(&addr);
                     if (addr != shortAddr)
                     {
                         //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                         ApiMac_sAddr_t dstAddr;
                         dstAddr.addr.shortAddr = shortAddr;
                         dstAddr.addrMode = ApiMac_addrType_short;
                         Collector_status_t stat;
                         stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                         if (stat != Collector_status_success)
                         {
                             CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                             CLI_startREAD();
                         }

                 //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                         return CRS_SUCCESS;
                     }
                 #endif
          CRS_retVal_t rsp=  Oad_createFactoryImageBackup();
          CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                   inputBad = false;
                   CLI_startREAD();
               }
      //oad factory check [shortAddr]
      if (memcmp(CLI_CRS_OAD_FACTORY_CHECK, line, sizeof(CLI_CRS_OAD_FACTORY_CHECK) - 1) == 0)
               {
          uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_OAD_FACTORY_CHECK) + 2]), NULL,
                                         16);
        #ifndef CLI_SENSOR

            uint16_t addr = 0;
            Cllc_getFfdShortAddr(&addr);
            if (addr != shortAddr)
            {
                //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                ApiMac_sAddr_t dstAddr;
                dstAddr.addr.shortAddr = shortAddr;
                dstAddr.addrMode = ApiMac_addrType_short;
                Collector_status_t stat;
                stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                if (stat != Collector_status_success)
                {
                    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                    CLI_startREAD();
                }

        //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                return CRS_SUCCESS;
            }
        #endif
           bool rspBool= OADStorage_checkFactoryImage();
           CRS_retVal_t rsp=CRS_FAILURE;
           if (rspBool==true) {
               rsp=CRS_SUCCESS;
        }
          CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                   inputBad = false;
                   CLI_startREAD();
               }


          //oad invalid img [shortAddr]
         if (memcmp(CLI_CRS_OAD_INVALID, line, sizeof(CLI_CRS_OAD_INVALID) - 1) == 0)
                  {

             uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_OAD_INVALID) + 2]), NULL,
                                             16);
            #ifndef CLI_SENSOR

                uint16_t addr = 0;
                Cllc_getFfdShortAddr(&addr);
                if (addr != shortAddr)
                {
                    //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                    ApiMac_sAddr_t dstAddr;
                    dstAddr.addr.shortAddr = shortAddr;
                    dstAddr.addrMode = ApiMac_addrType_short;
                    Collector_status_t stat;
                    stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                    if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }

            //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                    return CRS_SUCCESS;
                }
            #endif

               CRS_retVal_t rsp= Oad_invalidateImg();
              CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                       inputBad = false;
                       CLI_startREAD();
                  }



#ifdef CLI_CEU_BP
         if (memcmp(CLI_CRS_UART_BP_COMM, line, sizeof(CLI_CRS_UART_BP_COMM) - 1) == 0)
                      {
             char tempLine[512]={0};
             char *ptr=line+sizeof(CLI_CRS_UART_BP_COMM);
             int i=0;
             while(1){
                 tempLine[i]=*ptr;
                 if (*ptr==0) {
                     break;
                }
                 ptr++;
                 i++;
             }
             tempLine[i]='\r';
             i++;
             tempLine[i]=0;
             char *tmp=CRS_malloc(i+5);
             memset(tmp, 0, i+5);
             memcpy(tmp, tempLine, i+5);
             Mediator_msgObjSentToAppCli_t msg={0};
             msg.p=tmp;
             msg.len=i;
             Mediator_sendMsgToUartComm(&msg);
                     inputBad = false;
                     is_async_command=true;
                     AGCM_finishedTask();
                     CLI_startREAD();
                      }
#endif

      if (memcmp(CLI_CRS_RESET, line, sizeof(CLI_CRS_RESET) - 1) == 0)
               {
              CLI_OadResetParsing(line);
              inputBad = false;
              CLI_startREAD();
//                  SysCtrlSystemReset();
               }
 if (memcmp(CLI_LOGGER_GET, line, sizeof(CLI_LOGGER_GET)-1) == 0)
     {
         CLI_loggerGetParsing(line);
         inputBad = false;
     }
    if (memcmp(CLI_LOGGER_SET_LEVEL, line, sizeof(CLI_LOGGER_SET_LEVEL)-1) == 0)
     {
         CLI_loggerSetLevelParsing(line);
         inputBad = false;
     }

    if (memcmp(CLI_LOGGER_START, line, sizeof(CLI_LOGGER_START)-1) == 0)
     {
         CLI_loggerStartParsing(line);
         inputBad = false;
     }

    if (memcmp(CLI_LOGGER_STOP, line, sizeof(CLI_LOGGER_STOP)-1) == 0)
     {
         CLI_loggerStopParsing(line);
         inputBad = false;
     }

    if (memcmp(CLI_LOGGER_FLUSH, line, sizeof(CLI_LOGGER_FLUSH)-1) == 0)
     {
         CLI_loggerFlushParsing(line);
         inputBad = false;
     }
    if (memcmp(CLI_LOGGER_SET_BUFFER_TYPE, line, sizeof(CLI_LOGGER_SET_BUFFER_TYPE)-1) == 0)
     {
        CLI_loggerSetBufferTypeParsing(line);
         inputBad = false;
     }
      //ledMode [shortAddr] [on/off: 0x1 | 0x0]
      if (memcmp(CLI_CRS_LED_MODE, line, sizeof(CLI_CRS_LED_MODE) - 1) == 0)
                 {
           CRS_retVal_t ret= CLI_ledModeParsing(line);

               inputBad = false;
                 }
      //set dio [shortAddr] [dioIdx] [value: 0x0 | 0x1]
      if (memcmp(CLI_CRS_SET_DIO, line, sizeof(CLI_CRS_SET_DIO) - 1) == 0)
                 {
           CRS_retVal_t ret= CLI_setDioParsing(line);
               inputBad = false;
                 }

      if (memcmp(CLI_CRS_LED_ON, line, sizeof(CLI_CRS_LED_ON) - 1) == 0)
               {
          CLI_ledOnParsing(line);
//                  Agc_ledOn();
              inputBad = false;
              CLI_startREAD();
               }

      if (memcmp(CLI_CRS_LED_OFF, line, sizeof(CLI_CRS_LED_OFF) - 1) == 0)
                {
          CLI_ledOffParsing(line);
//                   Agc_ledOff();
               inputBad = false;
               CLI_startREAD();
                }



      if (memcmp(CLI_CRS_OAD_FORMAT, line, sizeof(CLI_CRS_OAD_FORMAT) - 1) == 0)
               {
          uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_OAD_FORMAT) + 2]), NULL,
                                                  16);
                 #ifndef CLI_SENSOR

                     uint16_t addr = 0;
                     Cllc_getFfdShortAddr(&addr);
                     if (addr != shortAddr)
                     {
                         //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                         ApiMac_sAddr_t dstAddr;
                         dstAddr.addr.shortAddr = shortAddr;
                         dstAddr.addrMode = ApiMac_addrType_short;
                         Collector_status_t stat;
                         stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                         if (stat != Collector_status_success)
                         {
                             CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                             CLI_startREAD();
                         }

                 //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                         return CRS_SUCCESS;
                     }
                 #endif
                    CRS_retVal_t ret= Oad_flashFormat();
                   CLI_cliPrintf("\r\nStatus: 0x%x", ret);
                            inputBad = false;
                            CLI_startREAD();
               }


      else if (memcmp(CLI_CRS_FS_INSERT, line, sizeof(CLI_CRS_FS_INSERT) - 1)
              == 0)
      {

          CLI_fsInsertParsing(line);

          inputBad = false;
      }

      else if (memcmp(CLI_CRS_FS_LS, line, sizeof(CLI_CRS_FS_LS) - 1) == 0)
      {
          CLI_fsLsParsing(line);

          inputBad = false;
      }

      else if (memcmp(CLI_CRS_FS_READLINE, line, sizeof(CLI_CRS_FS_READLINE) - 1)
              == 0)
      {

          CLI_fsReadLineParsing(line);

          inputBad = false;
      }

      else if (memcmp(CLI_CRS_FS_READFILE, line, sizeof(CLI_CRS_FS_READFILE) - 1)
              == 0)
      {

          CLI_fsReadFileParsing(line);

          inputBad = false;
      }

      else if (memcmp(CLI_CRS_FS_NATIVE, line, sizeof(CLI_CRS_FS_NATIVE) - 1)
              == 0)
      {

          CLI_fsReadFileNative(line);

          inputBad = false;
      }

      else if (memcmp(CLI_CRS_FS_DELETE, line, sizeof(CLI_CRS_FS_DELETE) - 1)
              == 0)
      {

          CLI_fsDeleteParsing(line);

          inputBad = false;
      }

//      else if (memcmp(CLI_CRS_DEBUG, line, sizeof(CLI_CRS_DEBUG) - 1) == 0)
//      {
//          Config_runConfigFile("flat_no_init", fpgaMultiLineCallback);
//          inputBad = false;
//          is_async_command = true;
//
//      }
      else if (memcmp(CLI_CRS_FS_UPLOAD_FPGA, line,
                       sizeof(CLI_CRS_FS_UPLOAD_FPGA) - 1) == 0)
       {

          CLI_fsUploadFpgaParsing(line);
           is_async_command = true;
           inputBad = false;

       }

      else if (memcmp(CLI_CRS_FS_UPLOAD_DIG, line,
                      sizeof(CLI_CRS_FS_UPLOAD_DIG) - 1) == 0)
      {

          CLI_fsUploadDigParsing(line);
          is_async_command = true;
          inputBad = false;

      }


      else if (memcmp(CLI_CRS_FS_UPLOAD_RF, line,
                      sizeof(CLI_CRS_FS_UPLOAD_RF) - 1) == 0)
      {

          CLI_fsUploadRfParsing(line);
          is_async_command = true;
          inputBad = false;

      }

      else if (memcmp(CLI_AGC, line, sizeof(CLI_AGC) - 1) == 0){
          CLI_sensorsParsing(line);
          inputBad = false;
      }

      else if (memcmp(CLI_AGC_DEBUG, line, sizeof(CLI_AGC_DEBUG) - 1) == 0){
          CLI_sensorsDebugParsing(line);
          inputBad = false;
      }

      else if (memcmp(CLI_CSV_SENSOR, line, sizeof(CLI_CSV_SENSOR) - 1) == 0)
      {
        CLI_csvSensorParsing(line);
        inputBad = false;
      }

      else if (memcmp(CLI_AGC_MODE, line, sizeof(CLI_AGC_MODE) - 1) == 0){
          CLI_sensorModeParsing(line);
          inputBad = false;
      }

      else if (memcmp(CLI_AGC_GET_MODE, line, sizeof(CLI_AGC_GET_MODE) - 1) == 0){
          CLI_sensorGetModeParsing(line);
          inputBad = false;
      }
      else if (memcmp(CLI_AGC_SET_GAP, line, sizeof(CLI_AGC_SET_GAP) - 1) == 0){
          CLI_agcSetGapParsing(line);
          inputBad = false;
      }

      else if (memcmp(CLI_AGC_GET_GAP, line, sizeof(CLI_AGC_GET_GAP) - 1) == 0){
                    CLI_agcGetGapParsing(line);
                    inputBad = false;
                }
  else if (memcmp(CLI_AGC_SET_TIME_MINMAX, line, sizeof(CLI_AGC_SET_TIME_MINMAX) - 1) == 0){
      CLI_agcSetTimeMinMaxParsing(line);
          inputBad = false;
      }

  else if (memcmp(CLI_AGC_GET_TIME_MINMAX, line, sizeof(CLI_AGC_GET_TIME_MINMAX) - 1) == 0){
      CLI_agcGetTimeMinMaxParsing(line);
          inputBad = false;
      }

      else if (memcmp(CLI_AGC_CHANNEL, line, sizeof(CLI_AGC_CHANNEL) - 1) == 0){
          CLI_sensorChannelParsing(line);
          inputBad = false;
      }

      if (memcmp(CLI_CRS_FS_FORMAT, line, sizeof(CLI_CRS_FS_FORMAT) - 1) == 0)
      {

          CLI_fsFormat(line);
          inputBad = false;
      }
//      //config direct [shortAddr] [CM | SC | SN] [funcName | scriptName | snapShotname] [param=value ...]
//      if (memcmp(CLI_CRS_CONFIG_DIRECT, line, sizeof(CLI_CRS_CONFIG_DIRECT) - 1) == 0)
//        {
//
//          CLI_config_direct(line);
//            inputBad = false;
//            CLI_startREAD();
//        }


//      if (memcmp(CLI_CRS_CONFIG_DIRECT_SPI, line, sizeof(CLI_CRS_CONFIG_DIRECT_SPI) - 1) == 0)
//        {
//
//          CLI_config_direct_spi(line);
//            inputBad = false;
//            CLI_startREAD();
//        }

//      if (memcmp(CLI_CRS_CONFIG_LINE, line, sizeof(CLI_CRS_CONFIG_LINE) - 1) == 0)
//        {
//
//          CLI_config_line(line);
//            inputBad = false;
//
//        }

//      if (memcmp(CLI_CRS_CONFIG_LINE_SPI, line, sizeof(CLI_CRS_CONFIG_LINE_SPI) - 1) == 0)
//     {
//
//       CLI_config_line_spi(line);
//         inputBad = false;
//
//     }

//      if (memcmp(CLI_CRS_CONFIG_FILE_SPI, line, sizeof(CLI_CRS_CONFIG_FILE_SPI) - 1) == 0)
//           {
//
//             CLI_config_file_spi(line);
//               inputBad = false;
//           }

      if (memcmp (CLI_CRS_CONFIG_FILE, line, sizeof(CLI_CRS_CONFIG_FILE) - 1) == 0)
      {
          CLI_config_file(line);
         inputBad = false;
      }

      if (memcmp(CLI_DISCOVER_MODULES, line, sizeof(CLI_DISCOVER_MODULES) - 1) == 0)
      {
          CLI_discoverModules(line);
          inputBad = false;
      }

      if (memcmp(CLI_CRS_TMP, line, sizeof(CLI_CRS_TMP) - 1) == 0)
      {

          CLI_tmpParsing(line);
                   inputBad = false;
      }
      if (memcmp(CLI_CRS_RSSI, line, sizeof(CLI_CRS_RSSI) - 1) == 0)
      {

          CLI_rssiParsing(line);
                      inputBad = false;
      }

      if (memcmp(CLI_CRS_WATCHDOG_DISABLE, line, sizeof(CLI_CRS_WATCHDOG_DISABLE) - 1) == 0)
           {

          CLI_watchdogDisableParsing(line);
                           inputBad = false;
           }

      if (memcmp(CLI_CRS_MODEM_TEST, line, sizeof(CLI_CRS_MODEM_TEST) - 1) == 0)
           {

          CLI_modemTest(line);
                           inputBad = false;
           }



      if ((memcmp(CLI_CRS_HELP, line, sizeof(CLI_CRS_HELP) - 1) == 0) && memcmp(CLI_CRS_HELP_2, line, sizeof(CLI_CRS_HELP_2) - 1) != 0 )
         {
             CLI_helpParsing(line);
             inputBad = false;
         }

      if (memcmp(CLI_CRS_HELP_2, line, sizeof(CLI_CRS_HELP_2) - 1) == 0)
          {
              CLI_help2Parsing(line);
              inputBad = false;
          }

      if (inputBad && strlen(line) > 0)
      {
          if (gIsTranRemoteCommandSent==false) {
          CLI_cliPrintf(badInputMsg);
          }
          CLI_startREAD();
          return CRS_SUCCESS;
      }
      else if (inputBad)
      {
          CLI_startREAD();
          return CRS_SUCCESS;
      }

  //    gUartTxBufferIdx--;
      // CUI_cliPrintf(0,"avi fraind %s %d %08x", "is the best", 10, 0x34);

      if (is_async_command == true || gIsAsyncCommand == true)
      {
          gIsAsyncCommand = false;
          return CRS_SUCCESS;
      }
  //    CLI_startREAD();
  //    CLI_writeString(CLI_PROMPT, strlen(CLI_PROMPT));
  //    //memset(gUartTxBuffer, gUartRxBuffer, 1);
  //
  //    UART_read(gUartHandle, gUartRxBuffer, sizeof(gUartRxBuffer));
      if (gIsUartCommCommandRemoteCL==false) {
          gIsUartCommCommand=false;
    }
      return CRS_SUCCESS;
}

#ifndef CLI_SENSOR



/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t CLI_formNwkParsing(char *line)
{
//    Csf_formNwkAction();
    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_openNwkParsing(char *line)
{
//    Csf_openNwkAction();
    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_closeNwkParsing(char *line)
{
//    Csf_closeNwkAction();
    return CRS_SUCCESS;
}



static CRS_retVal_t CLI_listSensorsParsing(char *line)
{
    int x = 0;
       for (x = 0; (x < MAX_DEVICES_IN_NETWORK); x++)
       {
           if (0xffff != Cllc_associatedDevList[x].shortAddr)
           {




               uint32_t leftPart = 0, rightPart = 0;
               CLI_convertExtAddrTo2Uint32(&(Cllc_associatedDevList[x].extAddr), &leftPart, &rightPart);

               CLI_cliPrintf("\r\nsensor, 0x%x, 0x%x%x, 0x%x, 0x%x", 0xaabb,
                             leftPart, rightPart,
                             Cllc_associatedDevList[x].shortAddr,
                             Cllc_associatedDevList[x].status);

           }

       }
       CLI_startREAD();

   //    discoverNextSensor();
       return CRS_SUCCESS;;

}


#else
static CRS_retVal_t CLI_associate(char *line)
{
//    Ssf_assocAction();
    return CRS_SUCCESS;

}
static CRS_retVal_t CLI_disassociate(char *line)
{
//    Ssf_disassocAction();
    return CRS_SUCCESS;
}
#endif

//alarm list 0xshortAddr
static CRS_retVal_t CLI_AlarmsListParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_LIST_ALARMS_LIST) + 2]), NULL,
                                     16);


#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);

        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat=Collector_status_deviceNotFound;
//        memset(gCllc_associatedDevListLocal,0,sizeof(Cllc_associated_devices_t)*4);
//        memcpy(gCllc_associatedDevListLocal,Cllc_associatedDevList,sizeof(Cllc_associated_devices_t)*4);
        int x = 0;
        /* Clear any timed out transactions */
        for (x = 0; x < MAX_DEVICES_IN_NETWORK; x++)
        {
            if (shortAddr==Cllc_associatedDevList[x].shortAddr) {


            if ((Cllc_associatedDevList[x].shortAddr != CSF_INVALID_SHORT_ADDR)
                    && (Cllc_associatedDevList[x].status == 0x2201))
            {
                char tempLine2[100]={0};
                memcpy(tempLine2,line,strlen(line));
                if (Cllc_associatedDevList[x].rssiAvgCru) {
                char rssiAvgStr[100]={0};
                sprintf(rssiAvgStr," %d",Cllc_associatedDevList[x].rssiAvgCru);
                strcat(tempLine2,rssiAvgStr);
                }
                stat = Collector_sendCrsMsg(&dstAddr,(uint8_t *) tempLine2);
                break;
            }
        }
        }
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }
        return CRS_SUCCESS;
    }
#endif
    char tempLine[512]={0};
        memcpy(tempLine,line,strlen(line));
        const char s[2] = " ";
              char *token;
              /* get the first token */
                 token = strtok(tempLine, s);//alarms
                 token = strtok(NULL, s);//list
                 token = strtok(NULL, s);//0xshortAddr
                 token = strtok(NULL, s);//rssiAvgValue
                 if (token!=NULL) {
                     int8_t rssiAvg=0;
                  rssiAvg=strtol(token,NULL,10);

                      gRssiAvg=rssiAvg;
                     Alarms_checkRssi(rssiAvg);

                 }
        Alarms_printAlarms();
                CLI_startREAD();
        return CRS_SUCCESS;
}
//alarm set 0xshortAddr 0xid 0xstate
static CRS_retVal_t CLI_AlarmsSetParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_LIST_ALARMS_SET) + 2]), NULL,
                                     16);

    uint8_t id=0;
    uint8_t state=0;
    char tempLine[512]={0};
    memcpy(tempLine,line,strlen(line));
    const char s[2] = " ";
          char *token;
          /* get the first token */
             token = strtok(tempLine, s);//alarm
             token = strtok(NULL, s);//set
             token = strtok(NULL, s);//0xshortAddr
             token = strtok(NULL, s);//0xid
             id= strtoul(token+2,NULL,16);
             token = strtok(NULL, s);//0xstate
             state= strtoul(token+2,NULL,16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;

         }
#endif
         CRS_retVal_t retStatus=CRS_FAILURE;
         if(CHECK_BIT(state,ALARM_ACTIVE_BIT_LOCATION)==0){
              retStatus=Alarms_clearAlarm((Alarms_alarmType_t) (id - 1), ALARM_INACTIVE);
         }if(CHECK_BIT(state,ALARM_STICKY_BIT_LOCATION)==0){
             Alarms_clearAlarm((Alarms_alarmType_t) (id - 1), ALARM_STICKY);
         }
         if (retStatus == CRS_FAILURE)
         {
             CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
         }
         else{
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
                 }
         CLI_startREAD();
         return CRS_SUCCESS;
}


static CRS_retVal_t CLI_unit(char *line)
{
#ifndef CLI_SENSOR
//        CRS_LOG(CRS_DEBUG,"Collector");
        CLI_cliPrintf("\r\ncollector");
#else
        CLI_cliPrintf("\r\nsensor");
#endif
        char envBuff[100] = {0};

//        CRS_retVal_t rspStatus = Env_read("name ver config img", envFile);
//              if (rspStatus == CRS_SUCCESS){
//                  char* token;
//                  const char d[2] = "\n";
        char *requiredEnvs [] = {"name", "ver", "config","img"};
        uint8_t i = 0;
        uint8_t numOfParams = 4;
        CRS_retVal_t ret = CRS_SUCCESS;
        for (i = 0; i < numOfParams; i++)
        {
            ret = Env_read(requiredEnvs[i], envBuff);
            if (ret == CRS_FAILURE)
            {
                CLI_cliPrintf(", 0");
            }
            else
            {
                char envValue [50] = {0};
                strcpy(envValue,  &envBuff[strlen(requiredEnvs[i]) + 1]);
                envValue[strlen(envValue) - 1] = 0;
                CLI_cliPrintf(", %s", envValue);
            }
            memset(envBuff,0,100);
        }
#ifndef CLI_SENSOR
        uint32_t left = 0, right = 0;
        bool rsp = true;
        Llc_netInfo_t nwkInfo;
//        memset(&nwkInfo)
        rsp = Csf_getNetworkInformation(&nwkInfo);
        if (rsp == false)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        }
        else
        {
            CLI_convertExtAddrTo2Uint32(&(nwkInfo.devInfo.extAddress), &left, &right);
            //pan, longaddr,short,env.txt
            CLI_cliPrintf(", 0x%x, 0x%x%x, 0x%x", nwkInfo.devInfo.panID, left, right,  nwkInfo.devInfo.shortAddress);
        }


#else
//        CLI_cliPrintf("\r\nsensor");
//        char envFile[4096] = {0};
//
//        CRS_retVal_t rspStatus = Env_read("name ver config img", envFile);
//              if (rspStatus == CRS_SUCCESS){
//                  char* token;
//                  const char d[2] = "\n";
//                  token = strtok(envFile, d);
//                  while (token != NULL)
//                  {
//                    char * pEqual = strstr(token, "=");
//                    CLI_cliPrintf(", %s",pEqual+1 );
//                    token = strtok(NULL, d);
//                  }
//              }

        uint32_t left = 0, right = 0;
               bool rsp = true;
               ApiMac_deviceDescriptor_t devInfo;
               Llc_netInfo_t nwkInfo;
       //        memset(&nwkInfo)
               rsp = Ssf_getNetworkInfo(&devInfo, &nwkInfo);
               if (rsp == false)
               {
                   uint16_t panID = getPanId();
                   //uint16_t panID = CRS_GLOBAL_PAN_ID;
                   ApiMac_sAddrExt_t  mac = {0};
                   getMac(&mac);
                   CLI_convertExtAddrTo2Uint32(&(mac), &left, &right);
                   CLI_cliPrintf(", 0x%x, 0x%x%x", panID, left, right);

//                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
               }
               else
               {
                   CLI_convertExtAddrTo2Uint32(&(devInfo.extAddress), &left, &right);
                   //pan, longaddr,short,env.txt
                   CLI_cliPrintf(", 0x%x, 0x%x%x, 0x%x", nwkInfo.devInfo.panID, left, right,  devInfo.shortAddress);
               }

#endif
        CLI_startREAD();
        return CRS_SUCCESS;
}



//agc set gap [shortAddr] [start/stop] [rising/falling] [us]
static CRS_retVal_t CLI_agcSetGapParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_SET_GAP)]), s);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR
        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
        uint8_t isStart=0;
        token = strtok(NULL, s); //[start/stop]
        if (memcmp(token, "start", 5)==0) {
            isStart=1;
        }
       uint8_t isRising=0;
    token = strtok(NULL, s);// [rising/falling]
    if (memcmp(token, "rising", 6)==0) {
        isRising=1;
    }

    token = strtok(NULL, s); //[us]
    uint16_t us = strtoul((token), NULL, 10);
    CRS_retVal_t retStatus=Agc_setGap(isStart,isRising, us);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}


//agc get gap [shortAddr] [start/stop] [rising/falling]
static CRS_retVal_t CLI_agcGetGapParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_GET_GAP)]), s);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR
        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
        uint8_t isStart=0;
        token = strtok(NULL, s); //[start/stop]
        if (memcmp(token, "start", 5)==0) {
            isStart=1;
        }
       uint8_t isRising=0;
    token = strtok(NULL, s); //[tx/rx]
    if (memcmp(token, "rising", 6)==0) {
        isRising=1;
    }
    uint16_t result;
    CRS_retVal_t retStatus=Agc_getGap(isStart,isRising,&result);
    CLI_cliPrintf("\r\nGap Value: 0x%x", result);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}



//agc set time [shortAddr] [seconds]
static CRS_retVal_t CLI_agcSetTimeMinMaxParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_SET_TIME_MINMAX)]), s);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR
        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
        token = strtok(NULL, s); //[seconds]
    uint32_t seconds = strtoul((token), NULL, 10);
    CRS_retVal_t retStatus=Agc_setTimeMinMax(seconds);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}


//agc get time [shortAddr]
static CRS_retVal_t CLI_agcGetTimeMinMaxParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_GET_TIME_MINMAX)]), s);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR
        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
    uint32_t seconds= 0;
    CRS_retVal_t retStatus=Agc_getTimeMinMax(&seconds);
    CLI_cliPrintf("\r\nTime Value: 0x%x", seconds);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}


//set dio [shortAddr] [dioIdx] [value: 0x0 | 0x1]
static CRS_retVal_t CLI_setDioParsing(char *line)
{
uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_LED_MODE) + 2]), NULL,
                               16);
#ifndef CLI_SENSOR
  uint16_t addr = 0;
  Cllc_getFfdShortAddr(&addr);
  if (addr != shortAddr)
  {
      //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
      ApiMac_sAddr_t dstAddr;
      dstAddr.addr.shortAddr = shortAddr;
      dstAddr.addrMode = ApiMac_addrType_short;
      Collector_status_t stat;
      stat = Collector_sendCrsMsg(&dstAddr, (uint8_t *)line);
      if (stat != Collector_status_success)
      {
          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
          CLI_startREAD();
      }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
      return CRS_SUCCESS;
  }
#endif
      const char s[2] = " ";
     char *token;
     char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
     memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
     /* get the first token */
     token = strtok(&(tmpBuff[sizeof(CLI_CRS_LED_MODE)]), s); //shortAddr
     token = strtok(NULL, s);//[dioIdx]
     if (token == NULL) {
      return CRS_FAILURE;
  }
     uint32_t dioIdxInt =0;
     dioIdxInt=strtoul(token,NULL,10);
     token = strtok(NULL, s);//[value: 0x0 | 0x1]
     if (token == NULL) {
           return CRS_FAILURE;
       }
     uint32_t value =0;
    value=strtoul(token+2,NULL,16);
  CRS_retVal_t retStatus =Agc_setDio(dioIdxInt,value);
  Agc_readDio(dioIdxInt);
  CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
  CLI_startREAD();
  return retStatus;
}



//ledMode [shortAddr] [on/off: 0x1 | 0x0]
static CRS_retVal_t CLI_ledModeParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_LED_MODE) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t *)line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
        const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       token = strtok(&(tmpBuff[sizeof(CLI_CRS_LED_MODE)]), s); //shortAddr
       token = strtok(NULL, s);//[on/off: 0x1 | 0x0]
       if (token == NULL) {
        return CRS_FAILURE;
    }
       uint16_t ledModeInt=0;
       ledModeInt=strtoul(token+2,NULL,16);

    CRS_retVal_t retStatus = Agc_ledMode(ledModeInt);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}


static CRS_retVal_t CLI_ledOnParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_LED_ON) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t *) line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
    CRS_retVal_t retStatus = Agc_ledOn();
    return retStatus;
}


static CRS_retVal_t CLI_ledOffParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_LED_OFF) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
    CRS_retVal_t retStatus = Agc_ledOff();
    return retStatus;
}



static CRS_retVal_t CLI_fpgaOpenParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_FPGA_OPEN) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
    CRS_retVal_t retStatus = Fpga_UART_init(NULL);
    return retStatus;
}

static CRS_retVal_t CLI_fpgaCloseParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_FPGA_CLOSE) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
    CRS_retVal_t retStatus = Fpga_UART_close();
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_fpgaWriteLinesParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FPGA_WRITELINES)]), s);
    //token = strtok(NULL, s);

    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);

        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);

        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

        return CRS_SUCCESS;
    }
#endif
//    CRS_retVal_t retStatus = CRS_FAILURE;

    char *fpgaLine =
            &(line[sizeof(CLI_CRS_FPGA_WRITELINES) + strlen(token) + 1]);
    char lineToSend[CUI_NUM_UART_CHARS] = { 0 };
    uint32_t len =  strlen(fpgaLine);
    memcpy(lineToSend, fpgaLine, len);

//    CRS_retVal_t rspStatus = Fpga_UART_writeMultiLine(lineToSend,
//                                                 fpgaMultiLineCallback);
    uint32_t rsp = 0;
    if (lineToSend[len - 1] != '\r')
    {
        lineToSend[len] = '\r';
    }
    CRS_retVal_t rspStatus = Fpga_SPI_WriteMultiLine(lineToSend, &rsp);
    CLI_startREAD();

    return rspStatus;
}

static CRS_retVal_t CLI_fpgaReadLinesParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FPGA_READLINES)]), s);
    //token = strtok(NULL, s);

    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);

        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);

        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

        return CRS_SUCCESS;
    }
#endif
//    CRS_retVal_t retStatus = CRS_FAILURE;

    char *fpgaLine =
            &(line[sizeof(CLI_CRS_FPGA_READLINES) + strlen(token) + 1]);
    char lineToSend[CUI_NUM_UART_CHARS] = { 0 };
    uint32_t len =  strlen(fpgaLine);
    memcpy(lineToSend, fpgaLine, len);
//
//    CRS_retVal_t rspStatus = Fpga_UART_readMultiLine(lineToSend,
//                                                 fpgaMultiLineCallback);
    uint32_t rsp = 0;
    if (lineToSend[len - 1] != '\r')
    {
        lineToSend[len] = '\r';
    }
    CRS_retVal_t rspStatus = CRS_FAILURE;
    if (memcmp("ver\r",lineToSend, strlen(lineToSend)) == 0)
    {
       uint32_t version = 0;
       uint32_t date = 0;
       char *readVersionStr = "rd 0x0";
       char *readDateStr = "rd 0x1";
       memset(lineToSend, 0, CUI_NUM_UART_CHARS);
       memcpy(lineToSend, readVersionStr, strlen(readVersionStr));
       rspStatus = Fpga_SPI_WriteMultiLine(lineToSend, &version);
       if (rspStatus != CRS_SUCCESS)
       {
           CLI_startREAD();
           return CRS_FAILURE;
       }

       char versionStr [15] = {0};
       sprintf(versionStr,"%x",version);

       memset(lineToSend, 0, CUI_NUM_UART_CHARS);
       memcpy(lineToSend, readDateStr, strlen(readDateStr));
       rspStatus = Fpga_SPI_WriteMultiLine(lineToSend, &date);
       if (rspStatus != CRS_SUCCESS)
       {
           CLI_startREAD();
           return CRS_FAILURE;
       }

       char dateStr [DATE_STR_LEN] = {0};
       convertUintToDate(date, dateStr);
       CLI_cliPrintf("\r\nUnit boot OK FPGA Ver:%s Date:%s",versionStr, dateStr);
       CLI_startREAD();
       return CRS_SUCCESS;
    }


    rspStatus = Fpga_SPI_WriteMultiLine(lineToSend, &rsp);
    if (rspStatus == CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\n 0x");
        char rspStr[CUI_NUM_UART_CHARS] = {0};
        sprintf(rspStr,"%x",rsp);
        uint32_t i = 0;
        uint32_t len = strlen(rspStr);
        for (i = len; i < 8; i++) // print 0 infront of number
        {
            CLI_cliPrintf("0");
        }
        for (i = 0; i < len ; i++)
        {
            CLI_cliPrintf("%c", rspStr[i]);
        }
    }
    CLI_startREAD();
    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_fpgaTransStartParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FPGA_TRANSPARENT_START)]), s);
    //token = strtok(NULL, s);

    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);




#ifndef CLI_SENSOR
    uint16_t addr = 0;
    gTransparentShortAddr = shortAddr;
    Cllc_getFfdShortAddr(&addr);

    if (addr != shortAddr)
    {
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = gTransparentShortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, gUartTxBuffer);

        gIsTransparentBridge = true;
        if (stat != Collector_status_success)
        {
            gIsTransparentBridge = false;
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }
        //CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif

    Fpga_UART_transparentOpen();
    gIsTransparentBridge = true;
    if (gIsRemoteCommand == true)
    {
        gIsRemoteTransparentBridge = true;
    }
    CLI_startREAD();
return CRS_SUCCESS;
}

static CRS_retVal_t CLI_fpgaTransStopParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FPGA_TRANSPARENT_END)]), s);
    //token = strtok(NULL, s);

    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifdef CLI_SENSOR
    Fpga_UART_transparentClose();
    gIsTransparentBridge = false;
    gIsTransparentBridgeSuspended = false;
    gIsRemoteTransparentBridge = false;
    memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
    gUartTxBufferIdx = 0;
    CLI_startREAD();
    return CRS_SUCCESS;

#else
    uint16_t addr = 0;

    Cllc_getFfdShortAddr(&addr);

    if (addr == gTransparentShortAddr)
    {
        Fpga_UART_transparentClose();
        gIsTransparentBridge = false;
        gIsTransparentBridgeSuspended = false;
        memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
        gUartTxBufferIdx = 0;
        CLI_startREAD();
    }
    else
    {
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = gTransparentShortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, gUartTxBuffer);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }
        gIsTransparentBridge = false;
        gIsTransparentBridgeSuspended = false;
        memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
        gUartTxBufferIdx = 0;

    }
    return CRS_SUCCESS;
#endif

}


static CRS_retVal_t CLI_tddOpenParsing(char *line)
{

    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_TDD_OPEN) + 2]), NULL,
                                 16);
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
        }

//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
        return CRS_SUCCESS;
    }
#endif
    CRS_retVal_t retStatus = Tdd_init(tddOpenCallback);
    if (retStatus == CRS_FAILURE)
    {
        if(Tdd_isOpen() == CRS_SUCCESS){
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
        }
        else{
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        }
        CLI_startREAD();
    }
//    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
    return retStatus;
}
//#define CLI_CRS_TDD_CLOSE "tdd close"
static CRS_retVal_t CLI_tddCloseParsing(char *line)
{
    Tdd_close();
    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
        CLI_startREAD();
    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_tddStatusParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_TDD_STATUS) + 2]), NULL,
                                     16);
    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
        CRS_retVal_t retStatus = Tdd_printStatus(tddCallback);
//        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
//        CLI_startREAD();
        return retStatus;
}

static CRS_retVal_t CLI_tddSetSyncModeParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_SYNC_MODE)]), s);
    //token = strtok(NULL, s);
//    uint32_t commSize = sizeof(CLI_CRS_TDD_SYNC_MODE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif


    //filename
    token = strtok(NULL, s);
    uint32_t mode = strtoul(&(token[2]), NULL, 16);


    CRS_retVal_t retStatus = Tdd_setSyncMode(mode, tddCallback);

            return retStatus;

}

static CRS_retVal_t CLI_tddSetScsParsing(char *line)
{
    const char s[2] = " ";
      char *token;
      char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

      memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
      /* get the first token */
      //0xaabb shortAddr
      token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_SCS)]), s);
      //token = strtok(NULL, s);
//      uint32_t commSize = sizeof(CLI_CRS_TDD_SCS);
      uint32_t addrSize = strlen(token);
      //shortAddr in decimal
      uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

  #ifndef CLI_SENSOR

      uint16_t addr = 0;
      Cllc_getFfdShortAddr(&addr);
      if (addr != shortAddr)
      {
          //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
          ApiMac_sAddr_t dstAddr;
          dstAddr.addr.shortAddr = shortAddr;
          dstAddr.addrMode = ApiMac_addrType_short;
          Collector_status_t stat;
          stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
          if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
  //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

          return CRS_SUCCESS;
      }
  #endif


      //mode
      token = strtok(NULL, s);
      uint32_t scs = strtoul(&(token[2]), NULL, 16);
      // 1:15khz 2:30khz 4:60khz
      CRS_retVal_t retStatus = Tdd_setSCS(scs, tddCallback);

              return retStatus;
}

static CRS_retVal_t CLI_tddSetSsPosParsing(char *line)
{
    const char s[2] = " ";
      char *token;
      char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

      memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
      /* get the first token */
      //0xaabb shortAddr
      token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_SS_POS)]), s);
      //token = strtok(NULL, s);
//      uint32_t commSize = sizeof(CLI_CRS_TDD_SS_POS);
      uint32_t addrSize = strlen(token);
      //shortAddr in decimal
      uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

  #ifndef CLI_SENSOR

      uint16_t addr = 0;
      Cllc_getFfdShortAddr(&addr);
      if (addr != shortAddr)
      {
          //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
          ApiMac_sAddr_t dstAddr;
          dstAddr.addr.shortAddr = shortAddr;
          dstAddr.addrMode = ApiMac_addrType_short;
          Collector_status_t stat;
          stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
          if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
  //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

          return CRS_SUCCESS;
      }
  #endif


      //mode
      token = strtok(NULL, s);
      uint32_t ss_pos = strtoul(&(token[2]), NULL, 16);


      CRS_retVal_t retStatus = Tdd_setSs_pos(ss_pos, tddCallback);

              return retStatus;
}

static CRS_retVal_t CLI_tddSetHolParsing(char *line)
{
    const char s[2] = " ";
         char *token;
         char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

         memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
         /* get the first token */
         //0xaabb shortAddr
         token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_HOL)]), s);
         //token = strtok(NULL, s);
//         uint32_t commSize = sizeof(CLI_CRS_TDD_HOL);
         uint32_t addrSize = strlen(token);
         //shortAddr in decimal
         uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

     #ifndef CLI_SENSOR

         uint16_t addr = 0;
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;
         }
     #endif


         //min
         token = strtok(NULL, s);
         uint32_t min = strtoul(&(token[2]), NULL, 16);
         if(min > 60){
             min = 60;
         }
         //sec
         token = strtok(NULL, s);
         uint32_t sec = strtoul(&(token[2]), NULL, 16);
         if(sec > 59){
             sec = 59;
         }
         if ((min==0) && (sec<5)){
             sec = 5;
         }
         CRS_retVal_t retStatus = Tdd_setHolTime(min, sec, tddCallback);

                 return retStatus;
}

static CRS_retVal_t CLI_tddSetTtgParsing(char *line)
{
    const char s[2] = " ";
         char *token;
         char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

         memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
         /* get the first token */
         //0xaabb shortAddr
         token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_TTG)]), s);
         //token = strtok(NULL, s);
//         uint32_t commSize = sizeof(CLI_CRS_TDD_TTG);
         uint32_t addrSize = strlen(token);
         //shortAddr in decimal
         uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

     #ifndef CLI_SENSOR

         uint16_t addr = 0;
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;
         }
     #endif


         int8_t ttg_vals [4] = {};
         int i;
         for(i=0;i<4;i++){
             token = strtok(NULL, s);
             ttg_vals[i] = strtol(&(token[0]), NULL, 0);
         }


         CRS_retVal_t retStatus = Tdd_setTtg(ttg_vals, tddCallback);

                 return retStatus;
}

static CRS_retVal_t CLI_tddSetRtgParsing(char *line)
{
    const char s[2] = " ";
         char *token;
         char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

         memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
         /* get the first token */
         //0xaabb shortAddr
         token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_RTG)]), s);
         //token = strtok(NULL, s);
//         uint32_t commSize = sizeof(CLI_CRS_TDD_RTG);
         uint32_t addrSize = strlen(token);
         //shortAddr in decimal
         uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

     #ifndef CLI_SENSOR

         uint16_t addr = 0;
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;
         }
     #endif


         int8_t rtg_vals [4] = {};
         int i;
         for(i=0;i<4;i++){
             token = strtok(NULL, s);
             rtg_vals[i] = strtol(&(token[0]), NULL, 0);
         }


         CRS_retVal_t retStatus = Tdd_setRtg(rtg_vals, tddCallback);

                 return retStatus;
}

static CRS_retVal_t CLI_tddCommandParsing(char *line)
{
    const char s[2] = " ";
         char *token;
         char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

         memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
         /* get the first token */
         //0xaabb shortAddr
         token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_CMD)]), s);
         //token = strtok(NULL, s);
//         uint32_t commSize = sizeof(CLI_CRS_TDD_CMD);
         uint32_t addrSize = strlen(token);
         //shortAddr in decimal
         uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

     #ifndef CLI_SENSOR

         uint16_t addr = 0;
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;
         }
     #endif

         CRS_retVal_t retStatus = Tdd_restart(tddCallback);
         CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
         CLI_startREAD();
         return retStatus;
}


static CRS_retVal_t CLI_tddGetLockParsing(char *line)
{
    const char s[2] = " ";
         char *token;
         char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

         memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
         /* get the first token */
         //0xaabb shortAddr
         token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_GET_LOCK)]), s);
         //token = strtok(NULL, s);
//         uint32_t commSize = sizeof(CLI_CRS_TDD_GET_LOCK);
         uint32_t addrSize = strlen(token);
         //shortAddr in decimal
         uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

     #ifndef CLI_SENSOR

         uint16_t addr = 0;
         Cllc_getFfdShortAddr(&addr);
         if (addr != shortAddr)
         {
             //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
             ApiMac_sAddr_t dstAddr;
             dstAddr.addr.shortAddr = shortAddr;
             dstAddr.addrMode = ApiMac_addrType_short;
             Collector_status_t stat;
             stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
             if (stat != Collector_status_success)
                    {
                        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                        CLI_startREAD();
                    }
     //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

             return CRS_SUCCESS;
         }
     #endif
#ifdef CLI_SENSOR
         CRS_retVal_t retStatus = (Tdd_isLocked());

#else
         CRS_retVal_t retStatus = !(Tdd_isLocked());
#endif
         if(retStatus == CRS_SUCCESS){
             CLI_cliPrintf("\r\nLOCKED");
         }
         else{
             CLI_cliPrintf("\r\nNOT_LOCKED");
         }
         CLI_startREAD();
         return retStatus;
}


static CRS_retVal_t CLI_tddSetFrameParsing(char *line)
{
    const char s[2] = " ";
           char *token;
           char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

           memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
           /* get the first token */
           //0xaabb shortAddr
           token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_FRAME)]), s);
           //token = strtok(NULL, s);
//           uint32_t commSize = sizeof(CLI_CRS_TDD_FRAME);
           uint32_t addrSize = strlen(token);
           //shortAddr in decimal
           uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
               if (stat != Collector_status_success)
                      {
                          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                          CLI_startREAD();
                      }
       //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif


           //filename
           token = strtok(NULL, s);
           uint32_t frameFormat = strtoul(&(token[2]), NULL, 16);



           CRS_retVal_t retStatus = Tdd_setFrameFormat(frameFormat, tddCallback);

                   return retStatus;
}

static CRS_retVal_t CLI_tddSetAllocParsing(char *line)
{
    const char s[2] = " ";
            char *token;
            char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

            memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
            /* get the first token */
            //0xaabb shortAddr
            token = strtok(&(tmpBuff[sizeof(CLI_CRS_TDD_ALLOC)]), s);
            //token = strtok(NULL, s);
//            uint32_t commSize = sizeof(CLI_CRS_TDD_ALLOC);
            uint32_t addrSize = strlen(token);
            //shortAddr in decimal
            uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

        #ifndef CLI_SENSOR

            uint16_t addr = 0;
            Cllc_getFfdShortAddr(&addr);
            if (addr != shortAddr)
            {
                //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                ApiMac_sAddr_t dstAddr;
                dstAddr.addr.shortAddr = shortAddr;
                dstAddr.addrMode = ApiMac_addrType_short;
                Collector_status_t stat;
                stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                if (stat != Collector_status_success)
                       {
                           CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                           CLI_startREAD();
                       }
        //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                return CRS_SUCCESS;
            }
        #endif

            token = strtok(NULL, s);
            uint32_t alloc = strtoul(&(token[2]), NULL, 16);


            CRS_retVal_t retStatus = Tdd_setAllocationMode(alloc, tddCallback);

                    return retStatus;
}

/*
 * this function returns a unsigned long shortaddr from given string
 * of this template 0x0000000000000000
   if not valid return 0
*/
static uint32_t MakeULFromHex(char *hex_str)
{
    return strtoul(&(hex_str[2]), NULL, 16);
}


// tdd set period1 [shortAddr] [time]
static CRS_retVal_t CLI_tddSetPeriod1Parsing(char *line)    //new
{
    const char separtor[2] = " ";
     char *token;
     char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
     memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);


     token = strtok(&(tmpBuff[strlen(CLI_CRS_TDD_PERIOD1) + 1]),separtor); // shortAddr
     uint32_t shortAddr = MakeULFromHex(token);

     //collector code
#ifndef CLI_SENSOR
     uint16_t addr = 0;
     Cllc_getFfdShortAddr(&addr);
     if (addr != shortAddr)
     {
         ApiMac_sAddr_t dstAddr;
         dstAddr.addr.shortAddr = shortAddr;
         dstAddr.addrMode = ApiMac_addrType_short;
         Collector_status_t stat;
         stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
         if (stat != Collector_status_success)
         {
           CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
           CLI_startREAD();
         }
         return CRS_SUCCESS;
     }

 #endif
     //sensor code or if shortAddr is the collector
     token = strtok(NULL, separtor); // time
     uint32_t period1 = MakeULFromHex(token);
     CRS_retVal_t retStatus = Tdd_setPeriod1(period1, tddCallback);
     if(CRS_SUCCESS != retStatus)
     {
         CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
         CLI_startREAD();
     }

     return retStatus;
}

// tdd set dl1 [shortAddr] [time]
static CRS_retVal_t CLI_tddSetDl1Parsing(char *line)    //new
{
    const char separtor[2] = " ";
     char *token;
     char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
     memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

     token = strtok(&(tmpBuff[strlen(CLI_CRS_TDD_DL1) + 1]),separtor); // shortAddr
     uint32_t shortAddr = MakeULFromHex(token);

     //collector code
#ifndef CLI_SENSOR
     uint16_t addr = 0;
     Cllc_getFfdShortAddr(&addr);
     if (addr != shortAddr)
     {
         ApiMac_sAddr_t dstAddr;
         dstAddr.addr.shortAddr = shortAddr;
         dstAddr.addrMode = ApiMac_addrType_short;
         Collector_status_t stat;
         stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
         if (stat != Collector_status_success)
         {
           CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
           CLI_startREAD();
         }
         return CRS_SUCCESS;
     }

 #endif
     //sensor code or if shortAddr is the collector
     token = strtok(NULL, separtor); // time
     uint32_t dl1 = MakeULFromHex(token);
     if(0 == dl1)
     {
         CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
         CLI_startREAD();
         return CRS_FAILURE;
     }

     return Tdd_setDl1(dl1, tddCallback);
}

static CRS_retVal_t CLI_tddSetPattern2Parsing(char *line)
{
    const char separtor[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

    token = strtok(&(tmpBuff[strlen(CLI_CRS_TDD_PATTERN2) + 1]),separtor); // shortAddr
    uint32_t shortAddr = MakeULFromHex(token);
    //collector code
#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
          CLI_startREAD();
        }
        return CRS_SUCCESS;
    }

#endif
    token = strtok(NULL, separtor); // time
    uint32_t pattern2 = MakeULFromHex(token);
    if(0 != pattern2 && 1 != pattern2)
    {
        CLI_cliPrintf("\r\nInvalid value for set pattern2");
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }

    CRS_retVal_t ret = Tdd_setPattern2(pattern2,tddCallback);
    if (ret != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
    }

    return ret;
}
static CRS_retVal_t CLI_tddSetPeriod2Parsing(char *line)
{
    const char separtor[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

    token = strtok(&(tmpBuff[strlen(CLI_CRS_TDD_PERIOD2) + 1]),separtor); // shortAddr
    uint32_t shortAddr = MakeULFromHex(token);
    //collector code
#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
          CLI_startREAD();
        }
        return CRS_SUCCESS;
    }

#endif

    token = strtok(NULL, separtor); // time
    uint32_t period2 = MakeULFromHex(token);
    CRS_retVal_t ret = Tdd_setPeriod2(period2,tddCallback);
    if (ret != CRS_SUCCESS)
    {
       CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
       CLI_startREAD();
    }

    return ret;
}
static CRS_retVal_t CLI_tddSetDl2Parsing(char *line)
{
    const char separtor[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

    token = strtok(&(tmpBuff[strlen(CLI_CRS_TDD_DL2) + 1]),separtor); // shortAddr
    uint32_t shortAddr = MakeULFromHex(token);
    //collector code
#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
        {
          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
          CLI_startREAD();
        }
        return CRS_SUCCESS;
    }

#endif
    token = strtok(NULL, separtor); // time
    uint32_t dl2 = MakeULFromHex(token);
    CRS_retVal_t ret = Tdd_setDl2(dl2,tddCallback);
    if (ret != CRS_SUCCESS)
    {
       CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
       CLI_startREAD();
    }

    return ret;
}


static CRS_retVal_t CLI_scriptRetValParsing(char *line)
{
    const char separtor[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

    token = strtok(&(tmpBuff[strlen(CLI_SCRIPT_RETVAL) + 1]),separtor); // shortAddr
    uint32_t shortAddr = MakeULFromHex(token);
    //collector code
#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
      ApiMac_sAddr_t dstAddr;
      dstAddr.addr.shortAddr = shortAddr;
      dstAddr.addrMode = ApiMac_addrType_short;
      Collector_status_t stat;
      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
      if (stat != Collector_status_success)
      {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
      }
      return CRS_SUCCESS;
    }
#endif

    CRS_retVal_t retStatus = CRS_SUCCESS;
    token = strtok(NULL, separtor); // key
    if (NULL == token)
    {
        retStatus = ScriptRetVals_printAll();
        CLI_startREAD();
        return retStatus;
    }

    char val[RETVAL_ELEMENT_VAL_SZ] = {0};
    char key[RETVAL_ELEMENT_KEY_SZ] = {0};
    memcpy(key, token,RETVAL_ELEMENT_KEY_SZ);
    retStatus = ScriptRetVals_getValue(key, val);
    if (CRS_SUCCESS == retStatus)
    {
        CLI_cliPrintf("\r\n%s: %s", key, val);
    }
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();

    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_fsInsertParsing(char *line)
{

//    Task_sleep(100000);

    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_INSERT)]), s);
    //token = strtok(NULL, s);
    uint32_t commSize = sizeof(CLI_CRS_FS_INSERT);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }

        return CRS_SUCCESS;
    }
#endif
    char filename[CRS_NVS_LINE_BYTES] = { 0 };

    //filename
    token = strtok(NULL, s);
    if (strlen(token)>CRS_NVS_LINE_BYTES){
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }
    memcpy(filename, token, strlen(token));
    uint32_t filenameSize = strlen(token);

    uint32_t totalSize = commSize + addrSize + filenameSize + 2;

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);

    CRS_retVal_t rspStatus = Nvs_writeFile(filename, &tmpBuff[totalSize]);
    CLI_cliPrintf("\r\nStatus: 0x%x", rspStatus);


//    const char d[2] = "\n";
//
//    char *token2 = strtok((&tmpBuff[totalSize]), d);
//    char lineToSend[CRS_FS_LINE_BYTES] = { 0 };
//    memcpy(lineToSend, token2, strlen(token2));
//    Fs_insertLine(filename, lineToSend);
//
////         CLI_cliPrintf("\r\n %s\r\n", token2 );
//    token2 = strtok(NULL, d);
//    /* walk through other tokens */
//    while ((token2 != NULL) && (strlen(token2) != 0))
//    {
//        memset(lineToSend, 0, CRS_FS_LINE_BYTES);
//        memcpy(lineToSend, token2, strlen(token2));
//        Fs_insertLine(filename, lineToSend);
//        // CLI_cliPrintf("\r\n %s\r\n", token2);
//        token2 = strtok(NULL, d);
//    }
    CLI_startREAD();
    return rspStatus;
}

static CRS_retVal_t CLI_fsLsParsing(char *line)
{
    //uint32_t shortAddr = CUI_convertStrUint(char *st)
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_LS)]), s);


    //uint32_t commSize = sizeof(CLI_CRS_FS_READLINE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif
    token = strtok(NULL, " ");
    uint32_t pageNum = strtoul(token, NULL, 0);

    CRS_retVal_t retStatus = Nvs_ls(pageNum);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
return CRS_SUCCESS;
}

static CRS_retVal_t CLI_fsReadLineParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_READLINE)]), s);
    //token = strtok(NULL, s);
//    uint32_t commSize = sizeof(CLI_CRS_FS_READLINE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif
    char filename[CRS_NVS_LINE_BYTES] = { 0 };
    char respLine[CRS_NVS_LINE_BYTES] = { 0 };

    //filename
    token = strtok(NULL, s);
    memcpy(filename, token, strlen(token));
    uint32_t filenameSize = strlen(token);

    token = strtok(NULL, s);

    uint32_t lineNumber = strtoul(&(token[2]), NULL, 16);

//    Fs_readLine(filename, lineNumber, respLine);
    Nvs_readLine(filename, lineNumber, respLine);
    CLI_cliPrintf("%s line #%d:%s  strlen:0x%x\n", filename, lineNumber,
                  respLine, strlen(respLine));
    CLI_startREAD();
    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_fsReadFileParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_READFILE)]), s);
    //token = strtok(NULL, s);
//    uint32_t commSize = sizeof(CLI_CRS_FS_READFILE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif
    char filename[CRS_NVS_LINE_BYTES] = { 0 };

    //filename
    token = strtok(NULL, s);
    memcpy(filename, token, strlen(token));
    uint32_t filenameSize = strlen(token);
    token = strtok(NULL, s);
    uint32_t fileIndex = strtoul(&(token[2]), NULL, 16);
    token = strtok(NULL, s);
    uint32_t readSize = strtoul(&(token[2]), NULL, 16);
//    Fs_readFile(filename);
    CRS_retVal_t rsp = CRS_FAILURE;
    if(token){
        if(readSize<650 && fileIndex<4096){
            rsp = Nvs_catSegment(filename, fileIndex, readSize);
        }
    }
    else{
        rsp = Nvs_cat(filename);
    }
    CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
    CLI_startREAD();
    return rsp;
}

static CRS_retVal_t CLI_fsReadFileNative(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_NATIVE)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }

        return CRS_SUCCESS;
    }
#endif
//    Fs_readFileNative(&(line[sizeof(CLI_CRS_FS_NATIVE) + strlen(token)]));
    CLI_startREAD();

    return (CRS_SUCCESS);
}

static CRS_retVal_t CLI_fsDeleteParsing(char *line)
{

    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_DELETE)]), s);
    //token = strtok(NULL, s);
//    uint32_t commSize = sizeof(CLI_CRS_FS_DELETE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif
    char filename[CRS_NVS_LINE_BYTES] = { 0 };

    //filename
    token = strtok(NULL, s);
    memcpy(filename, token, strlen(token));
    uint32_t filenameSize = strlen(token);

//    Fs_deleteFile(filename);
    CRS_retVal_t rsp = Nvs_rm(filename);
    CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
    CLI_startREAD();
    return rsp;
}

static CRS_retVal_t CLI_sensorsParsing(char *line){

    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_AGC)]), s);
    uint32_t addrSize = strlen(token);

    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
           if (stat != Collector_status_success)
          {
              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
              CLI_startREAD();
          }
          // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
    #endif

    AGC_max_results_t agcResults = Agc_getMaxResults();
#ifndef CLI_SENSOR
            CLI_cliPrintf("\r\nDLDetMaxInPwr=%s",agcResults.RfMaxDL);
            CLI_cliPrintf("\r\nDLDetectorMaxCableIFPower=%s",agcResults.IfMaxDL);
            CLI_cliPrintf("\r\nULDetMaxPwr=%s", agcResults.RfMaxUL);
            CLI_cliPrintf("\r\nULDetectorMaxCableIFPower=%s",agcResults.IfMaxUL);
#else
            CLI_cliPrintf("\r\nDLDetMaxOutPwr=%s",agcResults.RfMaxDL);
            CLI_cliPrintf("\r\nDLDetectorMaxCableIFPower=%s",agcResults.IfMaxDL);
            CLI_cliPrintf("\r\nULDetMaxInPwr=%s",agcResults.RfMaxUL);
            CLI_cliPrintf("\r\nULDetectorMaxCableIFPower=%s",agcResults.IfMaxUL);
#endif
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
            CLI_startREAD();
            return CRS_SUCCESS;
}

static CRS_retVal_t CLI_csvSensorParsing(char *line)
{
    const char s[2] = " ";
        char *token;
        char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
        memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
        /* get the first token */
        //0xaabb shortAddr
        token = strtok(&(tmpBuff[sizeof(CLI_CSV_SENSOR)]), s);
        uint32_t addrSize = strlen(token);
        uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

          uint16_t addr = 0;
          Cllc_getFfdShortAddr(&addr);
          if (addr != shortAddr)
          {
              // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
              ApiMac_sAddr_t dstAddr;
              dstAddr.addr.shortAddr = shortAddr;
              dstAddr.addrMode = ApiMac_addrType_short;
              Collector_status_t stat;
              stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
              if (stat != Collector_status_success)
             {
                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                 CLI_startREAD();
             }
             // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

              return CRS_SUCCESS;
          }
    #endif
        printADCOutputCSV();
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
        CLI_startREAD();
        return CRS_SUCCESS;
}

static CRS_retVal_t CLI_sensorsDebugParsing(char *line){

    const char s[2] = " ";
    char *token;
    uint16_t mode = Agc_getMode();
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_AGC_DEBUG)]), s);
    uint32_t addrSize = strlen(token);

    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
           if (stat != Collector_status_success)
          {
              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
              CLI_startREAD();
          }
          // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
    #endif
CRS_retVal_t retStatus=CRS_SUCCESS;
     printADCOutput();
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;
}



static CRS_retVal_t CLI_sensorGetModeParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_GET_MODE)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_AGC_GET_MODE);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            return CRS_SUCCESS;
        }
    #endif
   uint16_t mode = Agc_getMode();
    CLI_cliPrintf("\r\nsensorMode: 0x%x", mode);
    CLI_startREAD();
    return CRS_SUCCESS;
}


//set sensor [shortAddr] [mode]
static CRS_retVal_t CLI_sensorModeParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_MODE)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_AGC_MODE);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif

    token = strtok(NULL, s);
    uint32_t tempMode = strtoul(&(token[2]), NULL, 16);
//    if ((tempMode>AGC_UL) ||(!Agc_isInitialized())){
//        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//        CLI_startREAD();
//        return CRS_FAILURE;
//    }
    AGC_sensorMode_t mode = (AGC_sensorMode_t)tempMode;
    CRS_retVal_t retStatus = Agc_setMode(mode);
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
//    if(retStatus == CRS_SUCCESS){
//        CLI_cliPrintf("\r\nSensorMode=%x", mode);
//    }
//    else{
//        CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
//    }
    CLI_startREAD();
    return retStatus;

}



//fs upload dig [shortAddr] [filename] [mode (slave/native)] [chipNumber] [nameVals]
static CRS_retVal_t CLI_fsUploadDigParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_UPLOAD_DIG)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }

#endif
    token = strtok(NULL, s);    //filename
    char filename[FILENAME_SZ] = { 0 };
    memcpy(filename, token, strlen(token));

    token = strtok(NULL, s); //mode slave/native
    char modeStr[10] = {0};
    CRS_chipMode_t chipMode;
    memcpy(modeStr, token, strlen(token));

    if (memcmp(CLI_MODE_SLAVE, modeStr, sizeof(CLI_MODE_SLAVE) - 1) == 0)
    {
        chipMode = MODE_SPI_SLAVE;
    }
    else if (memcmp(CLI_MODE_NATIVE, modeStr, sizeof(CLI_MODE_NATIVE) - 1) == 0)
    {
        chipMode = MODE_NATIVE;
    }
    else
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_SUCCESS;

    }
    token = strtok(NULL, s);//[chipNumber]
    if(!token){
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }
    uint32_t chipNum=strtoul(&token[2], NULL, 16);

        token = strtok(NULL, s);//nameVals
        CRS_nameValue_t nameVals[NAME_VALUES_SZ];
        memset(nameVals,0,sizeof(CRS_nameValue_t)*NAME_VALUES_SZ);
        if(token!=NULL){
            char *ptr=token;
                   int idx=0;
                   while(*ptr){
                       char value[NAMEVALUE_NAME_SZ] = { 0 };
                       int j=0;
                       while(*ptr!='='){
                           nameVals[idx].name[j]=*ptr;
                           j++;
                           ptr++;
                       }
                       ptr++;//skip '='
                       j=0;
                       while(*ptr!=' ' && *ptr!=0){
                                       value[j]=*ptr;
                                      j++;
                                      ptr++;
                                  }
                       nameVals[idx].value=strtol(value, NULL, 10);
                       idx++;
                       ptr++;//skip ' '
                   }
        }


    CRS_retVal_t retStatus = DigSPI_uploadSnapDig(filename, chipMode,chipNum, nameVals);
    if (retStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
    }
    CLI_startREAD();

    return retStatus;
}





//fs upload fpga [shortAddr] [filename] [mode (slave/native)] [nameVals]
static CRS_retVal_t CLI_fsUploadFpgaParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_UPLOAD_FPGA)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR

    uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
   //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
#endif
    token = strtok(NULL, s);    //filename
    char filename[FILENAME_SZ] = { 0 };
    memcpy(filename, token, strlen(token));

    token = strtok(NULL, s); //mode slave/native
    char modeStr[10] = {0};
    CRS_chipMode_t chipMode;
    memcpy(modeStr, token, strlen(token));

    if (memcmp(CLI_MODE_SLAVE, modeStr, sizeof(CLI_MODE_SLAVE) - 1) == 0)
    {
        chipMode = MODE_SPI_SLAVE;
    }
    else if (memcmp(CLI_MODE_NATIVE, modeStr, sizeof(CLI_MODE_NATIVE) - 1) == 0)
    {
        chipMode = MODE_NATIVE;
    }
    else
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_SUCCESS;

    }
//    token = strtok(NULL, s);//[chipNumber]
//    uint32_t chipNum=strtol(token, NULL, 10);


        token = strtok(NULL, s);//nameVals
        CRS_nameValue_t nameVals[NAME_VALUES_SZ];
        memset(nameVals,0,sizeof(CRS_nameValue_t)*NAME_VALUES_SZ);
        if(token!=NULL){
            char *ptr=token;
                   int idx=0;
                   while(*ptr){
                       char value[NAMEVALUE_NAME_SZ] = { 0 };
                       int j=0;
                       while(*ptr!='='){
                           nameVals[idx].name[j]=*ptr;
                           j++;
                           ptr++;
                       }
                       ptr++;//skip '='
                       j=0;
                       while(*ptr!=' ' && *ptr!=0){
                                       value[j]=*ptr;
                                      j++;
                                      ptr++;
                                  }
                       nameVals[idx].value=strtol(value, NULL, 10);
                       idx++;
                       ptr++;//skip ' '
                   }
        }


#ifdef CRS_TMP_SPI
    CRS_retVal_t retStatus = DigSPI_uploadSnapFpga(filename, chipMode,nameVals);
#else
    CRS_retVal_t retStatus = DIG_uploadSnapFpga(filename, chipMode, nameVals,
                                                fpgaMultiLineCallback);
#endif
    if (retStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
    }
#ifdef CRS_TMP_SPI
    CLI_startREAD();
#endif

    return retStatus;
}

//rf run 0xaabb filename
static CRS_retVal_t CLI_RfRunParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_RF_RUN)]), s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR
   uint16_t addr = 0;
   Cllc_getFfdShortAddr(&addr);
   if (addr != shortAddr)
   {
       //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
       ApiMac_sAddr_t dstAddr;
       dstAddr.addr.shortAddr = shortAddr;
       dstAddr.addrMode = ApiMac_addrType_short;
       Collector_status_t stat;
       stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
       if (stat != Collector_status_success)
       {
          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
          CLI_startREAD();
       }
       return CRS_SUCCESS;
   }

#endif
   token = strtok(NULL, s);    //filename
   char filename[FILENAME_SZ] = { 0 };
   memcpy(filename, token, strlen(token));

   token = strtok(NULL, s); // chipNumber
    if (token == NULL)
    {
        CLI_cliPrintf("\r\nno chip number entered");
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();

        return CRS_FAILURE;
    }
    uint32_t chipNumber = 0;
    sscanf(token, "%x", &chipNumber);

    token = strtok(NULL, s); // lineNumber
    if (token == NULL)
    {
        CLI_cliPrintf("\r\nno line number entered");
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();

        return CRS_FAILURE;
    }
    uint32_t lineNumber = 0;
    sscanf(token, "%x", &lineNumber);



   token = strtok(NULL, s); //nameVals
   CRS_nameValue_t nameVals[NAME_VALUES_SZ] = {0};
   memset(nameVals,0,sizeof(CRS_nameValue_t)*NAME_VALUES_SZ);
   if(token!=NULL)
   {
       char *ptr=token;
       int idx=0;
       while(*ptr)
       {
          char value[NAMEVALUE_NAME_SZ] = { 0 };
          int j=0;
          while(*ptr!='=')
          {
              nameVals[idx].name[j]=*ptr;
              j++;
              ptr++;
          }
          ptr++;//skip '='
          j=0;
          while(*ptr!=' ' && *ptr!=0)
          {
                          value[j]=*ptr;
                         j++;
                         ptr++;
          }
          nameVals[idx].value=strtol(value, NULL, 10);
          idx++;
          ptr++;//skip ' '
       }
   }

   CRS_retVal_t retStatus = scriptRf_runFile((uint8_t*)filename, nameVals, chipNumber, lineNumber, false);
   if (retStatus != CRS_SUCCESS)
   {
       CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
       CLI_startREAD();
   }

   return retStatus;
}

// read rf line [shortAddr] [chipNumber] [lineNumber]
static CRS_retVal_t CLI_readRfLineParsing(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_CRS_READ_RF_LINE)]), s);
       uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR
       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
           if (stat != Collector_status_success)
           {
              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
              CLI_startREAD();
           }
           return CRS_SUCCESS;
       }
#endif
       token = strtok(NULL, s); // chipNumber 0x...
       if (token == NULL)
       {
           CLI_cliPrintf("\r\nno chip number entered");
           CLI_startREAD();
           return CRS_FAILURE;
       }
       uint32_t chipNumber = CLI_convertStrUint(&(token[2]));

       token = strtok(NULL, s); // lineNumber 0x...
       if (token == NULL)
       {
           CLI_cliPrintf("\r\nno line number entered");
           CLI_startREAD();
           return CRS_FAILURE;
       }
       uint32_t lineNumber = CLI_convertStrUint(&(token[2]));

       SPI_readRfRegs(chipNumber, lineNumber);


}

//fs upload native 0xaabb filename rfAddr(fake) LUTLineNumber param=value
static CRS_retVal_t CLI_fsUploadRfParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_FS_UPLOAD_RF)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
#ifndef CLI_SENSOR

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr)
    {
        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
        ApiMac_sAddr_t dstAddr;
        dstAddr.addr.shortAddr = shortAddr;
        dstAddr.addrMode = ApiMac_addrType_short;
        Collector_status_t stat;
        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
        if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
//        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

        return CRS_SUCCESS;
    }
#endif
    token = strtok(NULL, s);    //filename
    char filename[FILENAME_SZ] = { 0 };
    memcpy(filename, token, strlen(token));

    token = strtok(NULL, s); //mode slave/native
        char modeStr[10] = {0};
        CRS_chipMode_t chipMode;
        memcpy(modeStr, token, strlen(token));

        if (memcmp(CLI_MODE_SLAVE, modeStr, sizeof(CLI_MODE_SLAVE) - 1) == 0)
        {
            chipMode = MODE_SPI_SLAVE;
        }
        else if (memcmp(CLI_MODE_NATIVE, modeStr, sizeof(CLI_MODE_NATIVE) - 1) == 0)
        {
            chipMode = MODE_NATIVE;
        }
        else
        {
            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
            CLI_startREAD();
            return CRS_SUCCESS;

        }

        token = strtok(NULL, s);
        char parsingTypeStr[10] = {0};
            memcpy(parsingTypeStr, token, strlen(token));



    token = strtok(NULL, s); //rfAddr
    uint32_t rfAddr = CLI_convertStrUint(&(token[2]));
    token = strtok(NULL, s); //LUT line number
    uint32_t LUTLineNumber = CLI_convertStrUint(&(token[2]));

    token = strtok(NULL, s); //nameVals
    CRS_nameValue_t nameVals[NAME_VALUES_SZ];
    memset(nameVals,0,sizeof(CRS_nameValue_t)*NAME_VALUES_SZ);
    if(token!=NULL){
        char *ptr=token;
                   int idx=0;
                   while(*ptr){
                       char value[NAMEVALUE_NAME_SZ] = { 0 };
                       int j=0;
                       while(*ptr!='='){
                           nameVals[idx].name[j]=*ptr;
                           j++;
                           ptr++;
                       }
                       ptr++;//skip '='
                       j=0;
                       while(*ptr!=' ' && *ptr!=0){
                                       value[j]=*ptr;
                                      j++;
                                      ptr++;
                                  }
                       nameVals[idx].value=strtol(value, NULL, 10);
                       idx++;
                       ptr++;//skip ' '
                   }
    }

#ifdef CRS_TMP_SPI
    CRS_retVal_t retStatus = CRS_SUCCESS;
    if(0 == memcmp(parsingTypeStr, "script",strlen(parsingTypeStr)))
    {
        retStatus = scriptRf_runFile(filename, nameVals, rfAddr, LUTLineNumber, false);
    }
    else
    {
        retStatus = SPI_RF_uploadSnapRf(filename, rfAddr, LUTLineNumber, chipMode, nameVals,false);
    }
#else

    CRS_retVal_t retStatus = RF_uploadSnapRf(filename, rfAddr, LUTLineNumber, chipMode, nameVals, fpgaMultiLineCallback,false);
#endif

    if (retStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
    }

#ifdef CRS_TMP_SPI
    CLI_startREAD();
#endif
    return retStatus;
}


////config direct [shortAddr] [CM | SC | SN] [funcName | scriptName | snapShotname] [param=value ...]
//static CRS_retVal_t CLI_config_direct(char *line)
//{
//    const char s[2] = " ";
//       char *token;
//       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
//
//       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//       /* get the first token */
//       //0xaabb shortAddr
//       token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_DIRECT)]), s);
//       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_CRS_CONFIG_DIRECT);
//       uint32_t addrSize = strlen(token);
//       //shortAddr in decimal
//       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//   #ifndef CLI_SENSOR
//
//       uint16_t addr = 0;
//       Cllc_getFfdShortAddr(&addr);
//       if (addr != shortAddr)
//       {
//           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//           ApiMac_sAddr_t dstAddr;
//           dstAddr.addr.shortAddr = shortAddr;
//           dstAddr.addrMode = ApiMac_addrType_short;
//           Collector_status_t stat;
//           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
//           if (stat != Collector_status_success)
//                  {
//                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//                      CLI_startREAD();
//                  }
////           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//           return CRS_SUCCESS;
//       }
//   #endif
//       //type
//       char type[4]={0};
//       token = strtok(NULL, s);
//       memcpy(type, token, strlen(token));
//       uint32_t typeSize = strlen(token);
//
//
//       //filename
//       char filename[CRS_NVS_LINE_BYTES] = { 0 };
//       token = strtok(NULL, s);
//       memcpy(filename, token, strlen(token));
//       uint32_t filenameSize = strlen(token);
//       //lineNumber
////       char lineNumStr[10]={0};
//       token = strtok(NULL, s);
//       uint32_t lineNumSize = strlen(token);
//       uint32_t lineNum=strtoul(&(token[2]), NULL, 16);
//       //fileInfo
//       char fileInfos[CUI_NUM_UART_CHARS] = {0};
//       memcpy(fileInfos, line + commSize+ addrSize+typeSize+filenameSize+lineNumSize+ 3, strlen(line + commSize+ addrSize+filenameSize+lineNumSize+ 3));
//
//       if(memcmp(type,"CM",2)==0){
////           tdd_open();
////         bool res=  setSCS(2); //scs
////         res=setSyncMode(true); //detect- manual | auto
////         res= setAllocationMode(0); //configaurtion
////         res=  setFrameFormat(0); //frame
////         res=  setCPType(0); //1  extended | 0 normal
////           tdd_close();
//
//       }else if(memcmp(type,"SC",2)==0){
////           DIG_uploadSnapFpga();
//       }else if(memcmp(type,"SN",2)==0){
////           crs_package_t packageLineStruct;
////           packageLineStruct.fileInfos[0].name=filename;
////           packageLineStruct.fileInfos[0].type=SC;
////           packageLineStruct.fileInfos[0].nameValues[0].name=filename;
////           packageLineStruct.fileInfos[0].nameValues[0].value=0;
////           MultiFiles_runMultiFiles(&packageLineStruct,
////                                                       RF,
////                                                       MODE_NATIVE,
////                                                       uploadPackageSingleLineCb);
//       }
//
////       Config_runConfigDirect(filename, lineNum, fileInfos, fpgaMultiLineCallback);
//       return CRS_SUCCESS;
//}




//config line [short addr] filename lineNumber scriptName:name=val scriptName:name=val
//static CRS_retVal_t CLI_config_line(char *line)
//{
//    const char s[2] = " ";
//       char *token;
//       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
//
//       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//       /* get the first token */
//       //0xaabb shortAddr
//       token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_LINE)]), s);
//       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_CRS_CONFIG_LINE);
//       uint32_t addrSize = strlen(token);
//       //shortAddr in decimal
//       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//   #ifndef CLI_SENSOR
//
//       uint16_t addr = 0;
//       Cllc_getFfdShortAddr(&addr);
//       if (addr != shortAddr)
//       {
//           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//           ApiMac_sAddr_t dstAddr;
//           dstAddr.addr.shortAddr = shortAddr;
//           dstAddr.addrMode = ApiMac_addrType_short;
//           Collector_status_t stat;
//           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
//           if (stat != Collector_status_success)
//                  {
//                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//                      CLI_startREAD();
//                  }
////           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//           return CRS_SUCCESS;
//       }
//   #endif
//       char filename[CRS_NVS_LINE_BYTES] = { 0 };
//
//       //filename
//       token = strtok(NULL, s);
//       memcpy(filename, token, strlen(token));
//       uint32_t filenameSize = strlen(token);
//       //lineNumber
////       char lineNumStr[10]={0};
//       token = strtok(NULL, s);
//       uint32_t lineNumSize = strlen(token);
//       uint32_t lineNum=strtoul(&(token[2]), NULL, 16);
//       //fileInfo
//       char fileInfos[CUI_NUM_UART_CHARS] = {0};
//       memcpy(fileInfos, line + commSize+ addrSize+filenameSize+lineNumSize+ 3, strlen(line + commSize+ addrSize+filenameSize+lineNumSize+ 3));
//
//       Config_runConfigFileLine(filename, lineNum, fileInfos, fpgaMultiLineCallback);
//       return CRS_SUCCESS;
//}





static CRS_retVal_t CLI_config_file(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_FILE)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_CRS_CONFIG_FILE);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

   #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
//           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
   #endif
       char filename[CRS_NVS_LINE_BYTES] = { 0 };

       //filename
       token = strtok(NULL, s);
       memcpy(filename, token, strlen(token));
       uint32_t filenameSize = strlen(token);
       SPI_Config_runConfigFile(filename);
       return CRS_SUCCESS;
}

static CRS_retVal_t CLI_discoverModules(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_DISCOVER_MODULES)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_DISCOVER_MODULES);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

   #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
//           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
   #endif
       char filename[CRS_NVS_LINE_BYTES] = { 0 };

       //filename
       token = strtok(NULL, s);
       if (token == NULL)
       {
           CLI_cliPrintf("\r\n no file name entered");
           CLI_startREAD();
           return CRS_FAILURE;
       }
       memcpy(filename, token, strlen(token));
       uint32_t filenameSize = strlen(token);
#ifdef CRS_TMP_SPI
      SPI_Config_runConfigFileDiscovery(filename);
#else
       Config_runConfigFileDiscovery(filename, fpgaMultiLineCallback);
#endif
       CLI_startREAD();
       return CRS_SUCCESS;
}

//static CRS_retVal_t CLI_config_direct_spi(char *line)
//{
//    const char s[2] = " ";
//          char *token;
//          char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
//
//          memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//          /* get the first token */
//          //0xaabb shortAddr
//          token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_DIRECT_SPI)]), s);
//          //token = strtok(NULL, s);
//          uint32_t commSize = sizeof(CLI_CRS_CONFIG_DIRECT_SPI);
//          uint32_t addrSize = strlen(token);
//          //shortAddr in decimal
//          uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//      #ifndef CLI_SENSOR
//
//          uint16_t addr = 0;
//          Cllc_getFfdShortAddr(&addr);
//          if (addr != shortAddr)
//          {
//              //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//              ApiMac_sAddr_t dstAddr;
//              dstAddr.addr.shortAddr = shortAddr;
//              dstAddr.addrMode = ApiMac_addrType_short;
//              Collector_status_t stat;
//              stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
//              if (stat != Collector_status_success)
//                     {
//                         CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//                         CLI_startREAD();
//                     }
//   //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//              return CRS_SUCCESS;
//          }
//      #endif
//          //type
//          char type[4]={0};
//          token = strtok(NULL, s);
//          memcpy(type, token, strlen(token));
//          uint32_t typeSize = strlen(token);
//
//
//          //filename
//          char filename[CRS_NVS_LINE_BYTES] = { 0 };
//          token = strtok(NULL, s);
//          memcpy(filename, token, strlen(token));
//          uint32_t filenameSize = strlen(token);
//          //lineNumber
//   //       char lineNumStr[10]={0};
//          token = strtok(NULL, s);
//          uint32_t lineNumSize = strlen(token);
//          uint32_t lineNum=strtoul(&(token[2]), NULL, 16);
//          //fileInfo
//          char fileInfos[CUI_NUM_UART_CHARS] = {0};
//          memcpy(fileInfos, line + commSize+ addrSize+typeSize+filenameSize+lineNumSize+ 3, strlen(line + commSize+ addrSize+filenameSize+lineNumSize+ 3));
//
//          if(memcmp(type,"CM",2)==0){
//   //           tdd_open();
//   //         bool res=  setSCS(2); //scs
//   //         res=setSyncMode(true); //detect- manual | auto
//   //         res= setAllocationMode(0); //configaurtion
//   //         res=  setFrameFormat(0); //frame
//   //         res=  setCPType(0); //1  extended | 0 normal
//   //           tdd_close();
//
//          }else if(memcmp(type,"SC",2)==0){
//   //           DIG_uploadSnapFpga();
//          }else if(memcmp(type,"SN",2)==0){
//   //           crs_package_t packageLineStruct;
//   //           packageLineStruct.fileInfos[0].name=filename;
//   //           packageLineStruct.fileInfos[0].type=SC;
//   //           packageLineStruct.fileInfos[0].nameValues[0].name=filename;
//   //           packageLineStruct.fileInfos[0].nameValues[0].value=0;
//   //           MultiFiles_runMultiFiles(&packageLineStruct,
//   //                                                       RF,
//   //                                                       MODE_NATIVE,
//   //                                                       uploadPackageSingleLineCb);
//          }
//
//   //       Config_runConfigDirect(filename, lineNum, fileInfos, fpgaMultiLineCallback);
//          return CRS_SUCCESS;
//}
//static CRS_retVal_t CLI_config_file_spi(char *line)
//{
//    const char s[2] = " ";
//    char *token;
//    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
//
//    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//    /* get the first token */
//    //0xaabb shortAddr
//    token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_FILE_SPI)]), s);
//    //token = strtok(NULL, s);
//    //       uint32_t commSize = sizeof(CLI_CRS_CONFIG_FILE);
//    uint32_t addrSize = strlen(token);
//    //shortAddr in decimal
//    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//    #ifndef CLI_SENSOR
//
//    uint16_t addr = 0;
//    Cllc_getFfdShortAddr(&addr);
//    if (addr != shortAddr)
//    {
//       //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//       ApiMac_sAddr_t dstAddr;
//       dstAddr.addr.shortAddr = shortAddr;
//       dstAddr.addrMode = ApiMac_addrType_short;
//       Collector_status_t stat;
//       stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
//       if (stat != Collector_status_success)
//              {
//                  CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//                  CLI_startREAD();
//              }
//    //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//       return CRS_SUCCESS;
//    }
//    #endif
//    char filename[CRS_NVS_LINE_BYTES] = { 0 };
//
//    //filename
//    token = strtok(NULL, s);
//    memcpy(filename, token, strlen(token));
//    uint32_t filenameSize = strlen(token);
//    SPI_Config_runConfigFile(filename);
//    CLI_startREAD();
//
//    return CRS_SUCCESS;
//}
//static CRS_retVal_t CLI_config_line_spi(char *line)
//{
//    const char s[2] = " ";
//    char *token;
//    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };
//
//    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//    /* get the first token */
//    //0xaabb shortAddr
//    token = strtok(&(tmpBuff[sizeof(CLI_CRS_CONFIG_LINE_SPI)]), s);
//    //token = strtok(NULL, s);
//    uint32_t commSize = sizeof(CLI_CRS_CONFIG_LINE_SPI);
//    uint32_t addrSize = strlen(token);
//    //shortAddr in decimal
//    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//    #ifndef CLI_SENSOR
//
//    uint16_t addr = 0;
//    Cllc_getFfdShortAddr(&addr);
//    if (addr != shortAddr)
//    {
//     //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//     ApiMac_sAddr_t dstAddr;
//     dstAddr.addr.shortAddr = shortAddr;
//     dstAddr.addrMode = ApiMac_addrType_short;
//     Collector_status_t stat;
//     stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
//     if (stat != Collector_status_success)
//            {
//                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//                CLI_startREAD();
//            }
//    //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//     return CRS_SUCCESS;
//    }
//    #endif
//    char filename[CRS_NVS_LINE_BYTES] = { 0 };
//
//    //filename
//    token = strtok(NULL, s);
//    memcpy(filename, token, strlen(token));
//    uint32_t filenameSize = strlen(token);
//    //lineNumber
//    //       char lineNumStr[10]={0};
//    token = strtok(NULL, s);
//    uint32_t lineNumSize = strlen(token);
//    uint32_t lineNum=strtoul(&(token[2]), NULL, 16);
//    //fileInfo
//    char fileInfos[CUI_NUM_UART_CHARS] = {0};
//    memcpy(fileInfos, line + commSize+ addrSize+filenameSize+lineNumSize+ 3, strlen(line + commSize+ addrSize+filenameSize+lineNumSize+ 3));
//
////    Config_runConfigFileLine(filename, lineNum, fileInfos, fpgaMultiLineCallback);
//    SPI_Config_runConfigFileLine(filename, lineNum, fileInfos);
//    CLI_startREAD();
//
//    return CRS_SUCCESS;
//}
static CRS_retVal_t CLI_discoverModules_spi(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_DISCOVER_MODULES)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_DISCOVER_MODULES);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

   #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
//           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
   #endif
       char filename[CRS_NVS_LINE_BYTES] = { 0 };

       //filename
       token = strtok(NULL, s);
       memcpy(filename, token, strlen(token));
       uint32_t filenameSize = strlen(token);
       SPI_Config_runConfigFileDiscovery(filename);
       CLI_startREAD();
//       Config_runConfigFileDiscovery(filename, fpgaMultiLineCallback);
       return CRS_SUCCESS;
}


















static CRS_retVal_t CLI_fsFormat(char *line)
{
    //uint32_t shortAddr = CUI_convertStrUint(char *st)
      uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_FS_FORMAT) + 2]), NULL, 16);
  #ifndef CLI_SENSOR

      uint16_t addr = 0;
      Cllc_getFfdShortAddr(&addr);
      if (addr != shortAddr)
      {
          //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
          ApiMac_sAddr_t dstAddr;
          dstAddr.addr.shortAddr = shortAddr;
          dstAddr.addrMode = ApiMac_addrType_short;
          Collector_status_t stat;
          stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
          if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
//          CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

          return CRS_SUCCESS;
      }
  #endif
  //    CRS_retVal_t retStatus = Fs_ls();
      CRS_retVal_t retStatus = Nvs_format();
      CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
      CLI_startREAD();
      return retStatus;
}

static CRS_retVal_t CLI_envUpdate(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_UPDATE)]), s);
    //token = strtok(NULL, s);
    uint32_t commSize = sizeof(CLI_CRS_ENV_UPDATE);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
           // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
    #endif
    uint32_t command_len = commSize + addrSize+ 1;
    char vars[CUI_NUM_UART_CHARS] = {0};
    memcpy(vars, line + command_len, strlen(line + command_len));
    CRS_retVal_t rspStatus = Env_write(vars);
    if (rspStatus != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }

    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);

    CLI_startREAD();
    return CRS_SUCCESS;



}


static CRS_retVal_t CLI_locksParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_LOCKS)]), s);
    //token = strtok(NULL, s);
    uint32_t commSize = sizeof(CLI_CRS_LOCKS);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

   #ifndef CLI_SENSOR

       uint16_t addr = 0;
       Cllc_getFfdShortAddr(&addr);
       if (addr != shortAddr)
       {
           //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
           ApiMac_sAddr_t dstAddr;
           dstAddr.addr.shortAddr = shortAddr;
           dstAddr.addrMode = ApiMac_addrType_short;
           Collector_status_t stat;
           stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
           if (stat != Collector_status_success)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                  }
//           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

           return CRS_SUCCESS;
       }
   #endif

        Locks_checkLocks(printLocksStatus);

       return CRS_SUCCESS;
}


static void printLocksStatus(void)
{
#ifndef CLI_SENSOR
    bool tddIsLocked = Locks_getTddLockVal();
    char *answer = tddIsLocked == true ? "LOCKED" : "UNLOCKED";
    CLI_cliPrintf("\r\nTDD LOCK: %s", answer);
#else
    bool adfIsLocked = Locks_getAdfLockVal();
    char *answer = adfIsLocked == true ? "LOCKED" : "UNLOCKED";
    CLI_cliPrintf("\r\nADF LOCK: %s", answer);
#endif
    bool tiIsLocked = Locks_getTiLockVal();
    answer = tiIsLocked == true ? "LOCKED" : "UNLOCKED";
    CLI_cliPrintf("\r\nTI LOCK: %s", answer);

    CLI_startREAD();
}

static CRS_retVal_t CLI_envRm(char *line)
{
    const char s[2] = " ";
               char *token;
               char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

               memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
               /* get the first token */
               //0xaabb shortAddr
               token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_RM)]), s);
               //token = strtok(NULL, s);
               uint32_t commSize = sizeof(CLI_CRS_ENV_RM);
               uint32_t addrSize = strlen(token);
               //shortAddr in decimal
               uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

           #ifndef CLI_SENSOR

               uint16_t addr = 0;
               Cllc_getFfdShortAddr(&addr);
               if (addr != shortAddr)
               {
                   //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                   ApiMac_sAddr_t dstAddr;
                   dstAddr.addr.shortAddr = shortAddr;
                   dstAddr.addrMode = ApiMac_addrType_short;
                   Collector_status_t stat;
                   stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                   if (stat != Collector_status_success)
                          {
                              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                              CLI_startREAD();
                          }
        //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                   return CRS_SUCCESS;
               }
           #endif

           char vars[CUI_NUM_UART_CHARS] = {0};
             memcpy(vars, line + commSize+ addrSize+ 1, strlen(line + commSize+ addrSize+ 1));
             CRS_retVal_t rspStatus = Env_delete(vars);
             if (rspStatus != CRS_SUCCESS)
                        {
                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);

                            CLI_startREAD();
                            return CRS_FAILURE;

                        }
             CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);

             CLI_startREAD();
                        return CRS_SUCCESS;
}

static CRS_retVal_t CLI_envLs(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_LS)]), s);
    //token = strtok(NULL, s);
    uint32_t commSize = sizeof(CLI_CRS_ENV_LS);
    uint32_t addrSize = strlen(token);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif



    char envFile[CUI_NUM_UART_CHARS] = {0};
    char envTmp[4096] = {0};
    memcpy(envFile, line + commSize+ addrSize+ 1, strlen(line));
    CRS_retVal_t rsp =Env_read(envFile, envTmp); //Env_read(envFile, envTmp);
    if (rsp != CRS_SUCCESS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;

    }

    const char d[2] = "\n";
    token = strtok(envTmp, d);

    while (token != NULL)
    {
        CLI_cliPrintf("\r\n%s",token );
        token = strtok(NULL, d);
    }

    CLI_startREAD();
    return CRS_SUCCESS;

}


//static CRS_retVal_t CLI_varsCmdParser(char *line, varsCmd_t varsCmdType, varsType_t varsType)
//{
//    const char s[2] = " ";
//    char *token;
//    char tmpBuff[CUI_NUM_UART_CHARS] = {0};
//    uint16_t flag=0;
//    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
//    /* get the first token */
//    // 0xaabb shortAddr
//
//    if (varsCmdType == varsCmd_ls)
//    {
//        if (varsType == varsType_env)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_LS)]), s);
//            flag=0;
//        }
//        else if (varsType == varsType_thrsh)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_THRSH_LS)]), s);
//            flag=1;
//        }
//        else if (varsType == varsType_initGain)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_LS)]), s);
//            flag=2;
//        }
//    }
//    else if (varsCmdType == varsCmd_format)
//    {
//        if (varsType == varsType_env)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_FORMAT)]), s);
//            flag=3;
//        }
//        else if (varsType == varsType_thrsh)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_THRSH_FORMAT)]), s);
//            flag=4;
//        }
//        else if (varsType == varsType_initGain)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_FORMAT)]), s);
//            flag=5;
//        }
//    }
//    else if (varsCmdType == varsCmd_rm)
//    {
//        if (varsType == varsType_env)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_RM)]), s);
//            flag=6;
//        }
//        else if (varsType == varsType_thrsh)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_THRSH_RM)]), s);
//            flag=7;
//        }
//        else if (varsType == varsType_initGain)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_RM)]), s);
//            flag=8;
//        }
//    }
//    else if (varsCmdType == varsCmd_update)
//    {
//        if (varsType == varsType_env)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_UPDATE)]), s);
//            flag=9;
//        }
//        else if (varsType == varsType_thrsh)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_THRSH_UPDATE)]), s);
//            flag=10;
//        }
//        else if (varsType == varsType_initGain)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_UPDATE)]), s);
//            flag=11;
//        }
//    }
//    else if (varsCmdType == varsCmd_restore)
//    {
//        if (varsType == varsType_env)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_RESTORE)]), s);
//            flag=12;
//        }
//        else if (varsType == varsType_thrsh)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_THRSH_RESTORE)]), s);
//            flag=13;
//        }
//        else if (varsType == varsType_initGain)
//        {
//            token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_RESTORE)]), s);
//            flag=14;
//        }
//    }
//
//    // token = strtok(NULL, s);
//    //                  uint32_t commSize = sizeof(CLI_CRS_ENV_FORMAT);
//    uint32_t addrSize = strlen(token);
//    // shortAddr in decimal
//    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
//
//#ifndef CLI_SENSOR
//
//    uint16_t addr = 0;
//    Cllc_getFfdShortAddr(&addr);
//    if (addr != shortAddr)
//    {
//        //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
//        ApiMac_sAddr_t dstAddr;
//        dstAddr.addr.shortAddr = shortAddr;
//        dstAddr.addrMode = ApiMac_addrType_short;
//        Collector_status_t stat;
//        stat = Collector_sendCrsMsg(&dstAddr, (uint8_t *)line);
//        if (stat != Collector_status_success)
//        {
//            CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
//            CLI_startREAD();
//        }
//        //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
//
//        return CRS_SUCCESS;
//    }
//#endif
//    if (varsCmdType == varsCmd_ls)
//    {
//        if (varsType == varsType_env)
//        {
//
//        }
//        else if (varsType == varsType_thrsh)
//        {
//
//        }
//        else if (varsType == varsType_initGain)
//        {
//
//        }
//    }
//    else if (varsCmdType == varsCmd_format)
//    {
//        if (varsType == varsType_env)
//        {
//
//        }
//        else if (varsType == varsType_thrsh)
//        {
//
//        }
//        else if (varsType == varsType_initGain)
//        {
//
//        }
//    }
//    else if (varsCmdType == varsCmd_rm)
//    {
//        if (varsType == varsType_env)
//        {
//
//        }
//        else if (varsType == varsType_thrsh)
//        {
//
//        }
//        else if (varsType == varsType_initGain)
//        {
//
//        }
//    }
//    else if (varsCmdType == varsCmd_update)
//    {
//        if (varsType == varsType_env)
//        {
//
//        }
//        else if (varsType == varsType_thrsh)
//        {
//
//        }
//        else if (varsType == varsType_initGain)
//        {
//
//        }
//    }
//    else if (varsCmdType == varsCmd_restore)
//    {
//        if (varsType == varsType_env)
//        {
//
//        }
//        else if (varsType == varsType_thrsh)
//        {
//          ;
//        }
//        else if (varsType == varsType_initGain)
//        {
//
//        }
//    }
//
////    CRS_retVal_t rsp = Env_format();
////    CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
//    CLI_startREAD();
//    return rsp;
//}


static CRS_retVal_t CLI_envFormat(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_FORMAT)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_ENV_FORMAT);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif
                  CRS_retVal_t rsp = Env_format();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;
}

static CRS_retVal_t CLI_envRestore(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_ENV_RESTORE)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_ENV_RESTORE);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif
                  CRS_retVal_t rsp = Env_restore();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;

}


static CRS_retVal_t CLI_trshUpdate(char *line)
{
    const char s[2] = " ";
           char *token;
           char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

           memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
           /* get the first token */
           //0xaabb shortAddr
           token = strtok(&(tmpBuff[sizeof(CLI_CRS_TRSH_UPDATE)]), s);
           //token = strtok(NULL, s);
           uint32_t commSize = sizeof(CLI_CRS_TRSH_UPDATE);
           uint32_t addrSize = strlen(token);
           //shortAddr in decimal
           uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
               if (stat != Collector_status_success)
                      {
                          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                          CLI_startREAD();
                      }
    //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif
           uint32_t command_len = commSize + addrSize+ 1;
           char vars[CUI_NUM_UART_CHARS] = {0};
           memcpy(vars, line + command_len, strlen(line + command_len));
           bool highTmpFlag=false;
           bool lowTmpFlag=false;
           bool tempOffsetFlag=false;

           if(strstr(vars,"UpperTempThr") != NULL){
               highTmpFlag=true;
           }
           if(strstr(vars,"LowerTempThr") != NULL){
               lowTmpFlag=true;
                      }
           if(strstr(vars, "TempOffset") != NULL){
               tempOffsetFlag=true;
           }
           CRS_retVal_t rspStatus = Thresh_write(vars);
           if (rspStatus != CRS_SUCCESS)
           {
               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
               CLI_startREAD();
               return CRS_FAILURE;

           }
           if(highTmpFlag||tempOffsetFlag){
               char envFile[4096]={0};
               //System Temperature : ID=4, thrshenv= tmp
               Thresh_read("UpperTempThr", envFile);
               int16_t highTempThrsh = strtol(envFile + strlen("UpperTempThr="),
               NULL, 10);
               memset(envFile,0,4096);
               Thresh_read("TempOffset", envFile);
               int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
               NULL, 10);
               Alarms_setTemperatureHigh(highTempThrsh+tempOffset);
               int16_t currentTemperature=0;
               Alarms_getTemperature(&currentTemperature);
               if (currentTemperature<=(highTempThrsh+tempOffset)) {
                   Alarms_clearAlarm(SystemTemperatureHigh, ALARM_INACTIVE);
            }
           }
           if(lowTmpFlag||tempOffsetFlag){
                      char envFile[4096]={0};
                      //System Temperature : ID=4, thrshenv= tmp
                      Thresh_read("LowerTempThr", envFile);
                      int16_t lowTempThrsh = strtol(envFile + strlen("LowerTempThr="),
                      NULL, 10);
                      memset(envFile,0,4096);
                      Thresh_read("TempOffset", envFile);
                      int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
                                 NULL, 10);
                      Alarms_setTemperatureLow(lowTempThrsh+tempOffset);
                      int16_t currentTemperature=0;
                      Alarms_getTemperature(&currentTemperature);
                      if (currentTemperature>=(lowTempThrsh+tempOffset)) {
                          Alarms_clearAlarm(SystemTemperatureLow, ALARM_INACTIVE);
                   }
                  }


           CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);

           CLI_startREAD();
                      return CRS_SUCCESS;



}

static CRS_retVal_t CLI_trshRm(char *line)
{
    const char s[2] = " ";
               char *token;
               char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

               memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
               /* get the first token */
               //0xaabb shortAddr
               token = strtok(&(tmpBuff[sizeof(CLI_CRS_TRSH_RM)]), s);
               //token = strtok(NULL, s);
               uint32_t commSize = sizeof(CLI_CRS_TRSH_RM);
               uint32_t addrSize = strlen(token);
               //shortAddr in decimal
               uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

           #ifndef CLI_SENSOR

               uint16_t addr = 0;
               Cllc_getFfdShortAddr(&addr);
               if (addr != shortAddr)
               {
                   //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                   ApiMac_sAddr_t dstAddr;
                   dstAddr.addr.shortAddr = shortAddr;
                   dstAddr.addrMode = ApiMac_addrType_short;
                   Collector_status_t stat;
                   stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                   if (stat != Collector_status_success)
                          {
                              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                              CLI_startREAD();
                          }
        //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                   return CRS_SUCCESS;
               }
           #endif

           char vars[CUI_NUM_UART_CHARS] = {0};
             memcpy(vars, line + commSize+ addrSize+ 1, strlen(line + commSize+ addrSize+ 1));
             CRS_retVal_t rsp = Thresh_delete(vars);
             CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
             CLI_startREAD();
             return rsp;
}



static CRS_retVal_t CLI_trshLs(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_TRSH_LS)]), s);
                  //token = strtok(NULL, s);
                  uint32_t commSize = sizeof(CLI_CRS_TRSH_LS);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif



                  char envFile[CUI_NUM_UART_CHARS] = {0};
                  char envTmp[4096] = {0};
                  memcpy(envFile, line + commSize+ addrSize+ 1, strlen(line));
                  CRS_retVal_t rsp = Thresh_read(envFile, envTmp);
                  if (rsp != CRS_SUCCESS)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                      return CRS_FAILURE;

                  }

                  const char d[2] = "\n";
                  token = strtok(envTmp, d);

                  while (token != NULL)
                  {
                      CLI_cliPrintf("\r\n%s",token );
                      token = strtok(NULL, d);
                  }


                  //filename

                  CLI_startREAD();
                  return CRS_SUCCESS;

}

static CRS_retVal_t CLI_trshFormat(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_TRSH_FORMAT)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_TRSH_FORMAT);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif


                  CRS_retVal_t rsp = Thresh_format();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;
}

static CRS_retVal_t CLI_trshRestore(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_TRSH_RESTORE)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_TRSH_RESTORE);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif
                  CRS_retVal_t rsp = Thresh_restore();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;

}





static CRS_retVal_t CLI_cigsUpdate(char *line)
{
    const char s[2] = " ";
           char *token;
           char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

           memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
           /* get the first token */
           //0xaabb shortAddr
           token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_UPDATE)]), s);
           //token = strtok(NULL, s);
           uint32_t commSize = sizeof(CLI_CRS_INIT_GAIN_UPDATE);
           uint32_t addrSize = strlen(token);
           //shortAddr in decimal
           uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
               if (stat != Collector_status_success)
                      {
                          CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                          CLI_startREAD();
                      }
    //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif
           uint32_t command_len = commSize + addrSize+ 1;
           char vars[CUI_NUM_UART_CHARS] = {0};
           memcpy(vars, line + command_len, strlen(line + command_len));

           CRS_retVal_t rspStatus = CIGS_write(vars);
           if (rspStatus != CRS_SUCCESS)
           {
               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
               CLI_startREAD();
               return CRS_FAILURE;

           }
           CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
           CLI_startREAD();
                      return CRS_SUCCESS;



}

static CRS_retVal_t CLI_cigsRm(char *line)
{
    const char s[2] = " ";
               char *token;
               char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

               memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
               /* get the first token */
               //0xaabb shortAddr
               token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_RM)]), s);
               //token = strtok(NULL, s);
               uint32_t commSize = sizeof(CLI_CRS_INIT_GAIN_RM);
               uint32_t addrSize = strlen(token);
               //shortAddr in decimal
               uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

           #ifndef CLI_SENSOR

               uint16_t addr = 0;
               Cllc_getFfdShortAddr(&addr);
               if (addr != shortAddr)
               {
                   //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                   ApiMac_sAddr_t dstAddr;
                   dstAddr.addr.shortAddr = shortAddr;
                   dstAddr.addrMode = ApiMac_addrType_short;
                   Collector_status_t stat;
                   stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                   if (stat != Collector_status_success)
                          {
                              CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                              CLI_startREAD();
                          }
        //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                   return CRS_SUCCESS;
               }
           #endif

           char vars[CUI_NUM_UART_CHARS] = {0};
             memcpy(vars, line + commSize+ addrSize+ 1, strlen(line + commSize+ addrSize+ 1));
             CRS_retVal_t rsp = CIGS_delete(vars);
             CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
             CLI_startREAD();
             return rsp;
}



static CRS_retVal_t CLI_cigsLs(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_LS)]), s);
                  //token = strtok(NULL, s);
                  uint32_t commSize = sizeof(CLI_CRS_INIT_GAIN_LS);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif



                  char envFile[CUI_NUM_UART_CHARS] = {0};
                  char envTmp[4096] = {0};
                  memcpy(envFile, line + commSize+ addrSize+ 1, strlen(line));
                  CRS_retVal_t rsp = CIGS_read(envFile, envTmp);
                  if (rsp != CRS_SUCCESS)
                  {
                      CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                      CLI_startREAD();
                      return CRS_FAILURE;

                  }

                  const char d[2] = "\n";
                  token = strtok(envTmp, d);

                  while (token != NULL)
                  {
                      CLI_cliPrintf("\r\n%s",token );
                      token = strtok(NULL, d);
                  }


                  //filename

                  CLI_startREAD();
                  return CRS_SUCCESS;

}

static CRS_retVal_t CLI_cigsFormat(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_FORMAT)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_TRSH_FORMAT);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif


                  CRS_retVal_t rsp = CIGS_format();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;
}

static CRS_retVal_t CLI_cigsRestore(char *line)
{
    const char s[2] = " ";
                  char *token;
                  char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

                  memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
                  /* get the first token */
                  //0xaabb shortAddr
                  token = strtok(&(tmpBuff[sizeof(CLI_CRS_INIT_GAIN_RESTORE)]), s);
                  //token = strtok(NULL, s);
//                  uint32_t commSize = sizeof(CLI_CRS_TRSH_RESTORE);
                  uint32_t addrSize = strlen(token);
                  //shortAddr in decimal
                  uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

              #ifndef CLI_SENSOR

                  uint16_t addr = 0;
                  Cllc_getFfdShortAddr(&addr);
                  if (addr != shortAddr)
                  {
                      //               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                      ApiMac_sAddr_t dstAddr;
                      dstAddr.addr.shortAddr = shortAddr;
                      dstAddr.addrMode = ApiMac_addrType_short;
                      Collector_status_t stat;
                      stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                      if (stat != Collector_status_success)
                             {
                                 CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                                 CLI_startREAD();
                             }
           //           CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                      return CRS_SUCCESS;
                  }
              #endif
                  CRS_retVal_t rsp = CIGS_restore();
                  CLI_cliPrintf("\r\nStatus: 0x%x", rsp);
                  CLI_startREAD();
                  return rsp;

}






static CRS_retVal_t CLI_setTimeParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_SET_TIME)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));

    #ifndef CLI_SENSOR

          uint16_t addr = 0;
          Cllc_getFfdShortAddr(&addr);
          if (addr != shortAddr)
          {
              // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
              ApiMac_sAddr_t dstAddr;
              dstAddr.addr.shortAddr = shortAddr;
              dstAddr.addrMode = ApiMac_addrType_short;
              Collector_status_t stat;
              stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
              if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
              // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

              return CRS_SUCCESS;
          }
     #endif
    token = strtok(NULL, s);

    unsigned long time_seconds = strtoul(&(token[2]), NULL, 16);
    Seconds_set(time_seconds);

    CLI_cliPrintf("\r\n%x", Seconds_get());
    CLI_startREAD();
    return CRS_SUCCESS;


}

static CRS_retVal_t CLI_modemTest(char *line)
{
    const char s[2] = " ";
        char *token;
        char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

        memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
        /* get the first token */
        //0xaabb shortAddr
        token = strtok(&(tmpBuff[sizeof(CLI_CRS_MODEM_TEST)]), s);
        //token = strtok(NULL, s);

        uint32_t timeMs = strtoul(&(token[2]), NULL, 16);
        #ifndef CLI_SENSOR
        Collector_sendCrsMsgTest(timeMs);

      #else

      #endif


        return CRS_SUCCESS;

}


static CRS_retVal_t CLI_getTimeParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_GET_TIME)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
    #ifndef CLI_SENSOR

          uint16_t addr = 0;
          Cllc_getFfdShortAddr(&addr);
          if (addr != shortAddr)
          {
              // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
              ApiMac_sAddr_t dstAddr;
              dstAddr.addr.shortAddr = shortAddr;
              dstAddr.addrMode = ApiMac_addrType_short;
              Collector_status_t stat;
              stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
              if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
              // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

              return CRS_SUCCESS;
          }
     #endif


    CLI_cliPrintf("\r\n %x", Seconds_get());
    CLI_startREAD();
    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_getGainParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_CRS_GET_GAIN)]), s);
    //token = strtok(NULL, s);

    //shortAddr in decimal
    uint32_t shortAddr = CLI_convertStrUint(&(token[2]));
    #ifndef CLI_SENSOR

          uint16_t addr = 0;
          Cllc_getFfdShortAddr(&addr);
          if (addr != shortAddr)
          {
              // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
              ApiMac_sAddr_t dstAddr;
              dstAddr.addr.shortAddr = shortAddr;
              dstAddr.addrMode = ApiMac_addrType_short;
              Collector_status_t stat;
              stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
              if (stat != Collector_status_success)
                 {
                     CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                     CLI_startREAD();
                 }
              // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

              return CRS_SUCCESS;
          }
     #endif
    CRS_retVal_t ret = CRS_FAILURE;
    token = strtok(NULL, s);
    char *rfAddr=token;
    int rfAddrInt=0;
    int gain;
    if(memcmp("dc_rf_high_freq_hb_rx",token, sizeof("dc_rf_high_freq_hb_rx")-1) == 0){
        rfAddr+=sizeof("dc_rf_high_freq_hb_rx_chip");
        rfAddrInt=strtol(rfAddr, NULL, 10);
switch (rfAddrInt) {
    case 0:
        gain = CRS_cbGainStates.dc_rf_high_freq_hb_rx_chip_0;
        break;
    case 2:
        gain = CRS_cbGainStates.dc_rf_high_freq_hb_rx_chip_2;
        break;
    case 4:
            gain = CRS_cbGainStates.dc_rf_high_freq_hb_rx_chip_4;
            break;
    case 6:
        gain = CRS_cbGainStates.dc_rf_high_freq_hb_rx_chip_6;
        break;
    default:
        CRS_LOG(CRS_ERR, "\r\nINVALID rfAddr! rfAddr : %d", rfAddr);
        break;
}
//        gain = CRS_cbGainStates.dc_rf_high_freq_hb_rx;
        ret = CRS_SUCCESS;
    } else if(memcmp("uc_rf_high_freq_hb_tx", token,sizeof("uc_rf_high_freq_hb_tx")-1) == 0){
        rfAddr+=sizeof("uc_rf_high_freq_hb_tx_chip");
         rfAddrInt=strtol(rfAddr, NULL, 10);
 switch (rfAddrInt) {
     case 0:
         gain = CRS_cbGainStates.uc_rf_high_freq_hb_tx_chip_0;
         break;
     case 2:
         gain = CRS_cbGainStates.uc_rf_high_freq_hb_tx_chip_2;
         break;
     case 4:
             gain = CRS_cbGainStates.uc_rf_high_freq_hb_tx_chip_4;
             break;
     case 6:
         gain = CRS_cbGainStates.uc_rf_high_freq_hb_tx_chip_6;
         break;
     default:
         CRS_LOG(CRS_ERR, "\r\nINVALID rfAddr! rfAddr : %d", rfAddr);
         break;
 }
//        gain = CRS_cbGainStates.uc_rf_high_freq_hb_tx;
        ret = CRS_SUCCESS;
    } else if(memcmp("uc_if_low_freq_rx", token,sizeof("uc_if_low_freq_rx")-1) == 0){
        rfAddr+=sizeof("uc_if_low_freq_rx_chip");
         rfAddrInt=strtol(rfAddr, NULL, 10);
 switch (rfAddrInt) {
     case 1:
         gain = CRS_cbGainStates.uc_if_low_freq_rx_chip_1;
         break;
     case 3:
         gain = CRS_cbGainStates.uc_if_low_freq_rx_chip_3;
         break;
     case 5:
             gain = CRS_cbGainStates.uc_if_low_freq_rx_chip_5;
             break;
     case 7:
         gain = CRS_cbGainStates.uc_if_low_freq_rx_chip_7;
         break;
     default:
         CRS_LOG(CRS_ERR, "\r\nINVALID rfAddr! rfAddr : %d", rfAddr);
         break;
 }
//        gain = CRS_cbGainStates.uc_if_low_freq_rx;
        ret = CRS_SUCCESS;
    } else if(memcmp("dc_if_low_freq_tx", token,sizeof("dc_if_low_freq_tx")-1) == 0){

        rfAddr+=sizeof("dc_if_low_freq_tx_chip");
          rfAddrInt=strtol(rfAddr, NULL, 10);
  switch (rfAddrInt) {
      case 1:
          gain = CRS_cbGainStates.dc_if_low_freq_tx_chip_1;
          break;
      case 3:
          gain = CRS_cbGainStates.dc_if_low_freq_tx_chip_3;
          break;
      case 5:
              gain = CRS_cbGainStates.dc_if_low_freq_tx_chip_5;
              break;
      case 7:
          gain = CRS_cbGainStates.dc_if_low_freq_tx_chip_7;
          break;
      default:
          CRS_LOG(CRS_ERR, "\r\nINVALID rfAddr! rfAddr : %d", rfAddr);
          break;
  }
//        gain = CRS_cbGainStates.dc_if_low_freq_tx;
        ret = CRS_SUCCESS;
    }
    if(ret==CRS_SUCCESS){
        CLI_cliPrintf("\r\nGain=%d", gain);
    }
    CLI_cliPrintf("\r\nStatus: 0x%x", ret);
    CLI_startREAD();
    return ret;
}

static CRS_retVal_t CLI_sensorChannelParsing(char *line)
{
     const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_AGC_CHANNEL)]), s);
       //token = strtok(NULL, s);
//       uint32_t commSize = sizeof(CLI_AGC_CHANNEL);
       uint32_t addrSize = strlen(token);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }

    //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
            return CRS_SUCCESS;
        }
    #endif
    uint16_t channel = 0;
    token = strtok(NULL, s);
    if(token){
        channel = strtoul(&(token[2]), NULL, 16);
    }
    CRS_retVal_t retStatus;
    if (!Agc_isInitialized() || channel>4){
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }
    retStatus = Agc_setChannel((AGC_channels_t)channel);
//    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
//    if(retStatus == CRS_SUCCESS){
//        CLI_cliPrintf("\r\nSensorStatus=OK");
//    }
//    else{
//        CLI_cliPrintf("\r\nSensorStatus=0x%x", retStatus);
//    }
    CLI_cliPrintf("\r\nStatus: 0x%x", retStatus);
    CLI_startREAD();
    return retStatus;

}

static CRS_retVal_t CLI_tmpParsing(char *line)
{
//    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_TMP) + 2]), NULL,
//                                         16);
        const char s[2] = " ";
        char *token;
        char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

        memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
        /* get the first token */
        //0xaabb shortAddr
        token = strtok(&(tmpBuff[sizeof(CLI_CRS_TMP)]), s);
        //token = strtok(NULL, s);
        uint32_t addrSize = strlen(token);
        //shortAddr in decimal
        uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);
        #ifndef CLI_SENSOR

            uint16_t addr = 0;
            Cllc_getFfdShortAddr(&addr);
            if (addr != shortAddr)
            {
                //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                ApiMac_sAddr_t dstAddr;
                dstAddr.addr.shortAddr = shortAddr;
                dstAddr.addrMode = ApiMac_addrType_short;
                Collector_status_t stat;
                stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                if (stat != Collector_status_success)
                {
                    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                    CLI_startREAD();
                }

        //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                return CRS_SUCCESS;
            }
        #endif
            int16_t temp = 0;
            Alarms_getTemperature(&temp);
            token = strtok(NULL, s);
            if(memcmp(token, "offset", strlen("offset"))==0){
                char envFile[4096] = { 0 };
                Thresh_read("TempOffset", envFile);
                int16_t tempOffset = strtol(envFile + strlen("TempOffset="),
                NULL, 10);
                temp = temp - tempOffset;
            }
            CLI_cliPrintf("\r\n%d", (int)temp);
            CLI_startREAD();
            return CRS_SUCCESS;
}

//#define CLI_CRS_RSSI "rssi"
static CRS_retVal_t CLI_rssiParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_RSSI) + 2]), NULL,
                                            16);
#ifndef CLI_SENSOR
    Csf_sensorsDataPrint(shortAddr);
#else

#endif

               CLI_startREAD();
               return CRS_SUCCESS;

}


static CRS_retVal_t CLI_watchdogDisableParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_WATCHDOG_DISABLE) + 2]), NULL,
                                            16);
           #ifndef CLI_SENSOR

               uint16_t addr = 0;
               Cllc_getFfdShortAddr(&addr);
               if (addr != shortAddr)
               {
                   //        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
                   ApiMac_sAddr_t dstAddr;
                   dstAddr.addr.shortAddr = shortAddr;
                   dstAddr.addrMode = ApiMac_addrType_short;
                   Collector_status_t stat;
                   stat = Collector_sendCrsMsg(&dstAddr,(uint8_t*) line);
                   if (stat != Collector_status_success)
                   {
                       CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                       CLI_startREAD();
                   }

           //        CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);
                   return CRS_SUCCESS;
               }
           #endif
               CRS_watchdogDisable();
               CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
               CLI_startREAD();
               return CRS_SUCCESS;
}
#ifndef CLI_SENSOR

//oad send img 0xshortAddr 0xisReset 0xisFactory
static CRS_retVal_t CLI_OadSendImgParsing(char *line)
{
    uint16_t dstAddr=0;
    char tempLine[512]={0};
    memcpy(tempLine,line,strlen(line));
    uint16_t isResetInt=0;
    uint16_t isFactoryInt=0;
    bool isReset=true;
    bool isFactory=false;
    const char s[2] = " ";
          char *token;
          /* get the first token */
             token = strtok(tempLine, s);//oad
             token = strtok(NULL, s);//send
             token = strtok(NULL, s);//0ximg
             token = strtok(NULL, s);//0xshortAddr
             dstAddr= strtoul(token+2,NULL,16);
             token = strtok(NULL, s);//0xisReset
             CRS_retVal_t retstatus=CRS_FAILURE;
             if (token==NULL) {
                 token = strtok(NULL, s);//0xisFactory
                 isFactoryInt=strtoul(token+2,NULL,16);
                 if (isFactoryInt!=0) {
                     isFactory=true;
                }
                 retstatus = Oad_distributorSendImg(dstAddr,isReset,isFactory);
            }else{
                isResetInt=strtoul(token+2,NULL,16);
                if (isResetInt==0) {
                    isReset=false;
                }
                token = strtok(NULL, s);//0xisFactory
                        isFactoryInt=strtoul(token+2,NULL,16);
                        if (isFactoryInt!=0) {
                            isFactory=true;
                       }
                retstatus = Oad_distributorSendImg(dstAddr,isReset,isFactory);
            }

             if(retstatus==CRS_FAILURE){
                 CLI_cliPrintf("\r\nStatus: 0x%x", retstatus);
             }
             return retstatus;

}

//oad get img 0xshortAddr
static CRS_retVal_t CLI_OadGetImgParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_OAD_GET_IMG_VER) + 2]), NULL,
                                  16);

    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr){
        uint16_t dstAddr=0;
          char tempLine[512]={0};
          memcpy(tempLine,line,strlen(line));
          const char s[2] = " ";
                char *token;
                /* get the first token */
                   token = strtok(tempLine, s);//oad
                   token = strtok(NULL, s);//send
                   token = strtok(NULL, s);//0ximg
                   token = strtok(NULL, s);//0xshortAddr
                   dstAddr= strtoul(token+2,NULL,16);
                   Oad_distributorSendTargetFwVerReq(dstAddr);
                   return CRS_SUCCESS;
    }

        Oad_distributorGetFwVer();
        CLI_startREAD();
        return CRS_SUCCESS;
}

#endif

//reset 0xshortAddr
static CRS_retVal_t CLI_OadResetParsing(char *line)
{
    uint32_t shortAddr = strtoul(&(line[sizeof(CLI_CRS_RESET) + 2]), NULL,
                                  16);
#ifndef CLI_SENSOR
    uint16_t addr = 0;
    Cllc_getFfdShortAddr(&addr);
    if (addr != shortAddr){
        uint16_t dstAddr=0;
          char tempLine[512]={0};
          memcpy(tempLine,line,strlen(line));
          const char s[2] = " ";
                char *token;
                /* get the first token */
                    token = strtok(tempLine, s);//reset
                   token = strtok(NULL, s);//0xshortAddr
                   dstAddr= strtoul(token+2,NULL,16);
                   Oad_targetReset(dstAddr);
                   return CRS_SUCCESS;
    }
#endif
    CLI_startREAD();
    Task_sleep(1000);
        SysCtrlSystemReset();
        return CRS_SUCCESS;
}

static CRS_retVal_t CLI_loggerGetParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_GET)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif

    Logger_print();
    CLI_startREAD();

    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_loggerSetLevelParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_SET_LEVEL)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif

    logLevels_t lvl = logLevel_NUMOFLEVELS;
    token = strtok(NULL,s);
    char *options [] = {"NO_LOG","ERR","WRN","INF","DEB"};
    uint8_t i = 0;
    for (i = 0; i < logLevel_NUMOFLEVELS; i++)
    {
        if (memcmp(token, options[i],strlen(options[i])) == 0)
        {
            lvl = (logLevels_t) i;
            break;
        }
    }

    if (lvl == logLevel_NUMOFLEVELS)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }
    Logger_setThrshLogLevel(lvl);
    CLI_startREAD();

    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_loggerStartParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_START)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif

    uint32_t lvl = 0;
    token = strtok(NULL,s);
    lvl = strtoul(&(token[2]),NULL,16);
    if (lvl > logLevel_DEBUG)
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }
    Logger_start((logLevels_t)lvl);
    CLI_startREAD();

    return CRS_SUCCESS;
}
static CRS_retVal_t CLI_loggerStopParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_STOP)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif


    Logger_stop();
    CLI_startREAD();

    return CRS_SUCCESS;
}
static CRS_retVal_t CLI_loggerFlushParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_FLUSH)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif


    Logger_flush();
    CLI_startREAD();

    return CRS_SUCCESS;
}

static CRS_retVal_t CLI_loggerSetBufferTypeParsing(char *line)
{
    const char s[2] = " ";
    char *token;
    char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

    memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
    /* get the first token */
    //0xaabb shortAddr
    token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_SET_BUFFER_TYPE)]), s);
    //shortAddr in decimal
    uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

    #ifndef CLI_SENSOR

        uint16_t addr = 0;
        Cllc_getFfdShortAddr(&addr);
        if (addr != shortAddr)
        {
            // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
            ApiMac_sAddr_t dstAddr;
            dstAddr.addr.shortAddr = shortAddr;
            dstAddr.addrMode = ApiMac_addrType_short;
            Collector_status_t stat;
            stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
            if (stat != Collector_status_success)
            {
                CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                CLI_startREAD();
            }
            // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

            return CRS_SUCCESS;
        }
    #endif

    token = strtok(NULL,s);
    char *oneShot = "oneshot";
    char *circular = "circular";
    if (memcmp(token, oneShot, strlen(oneShot))==0)
    {
        Logger_setCircular(false);
    }
    else if (memcmp(token, circular, strlen(circular))==0)
    {
        Logger_setCircular(true);
    }
    else
    {
        CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
        CLI_startREAD();
        return CRS_FAILURE;
    }

    CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SUCCESS);
    CLI_startREAD();

    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_loggerGetBufferTypeParsing(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_GET_BUFFER_TYPE)]), s);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
               if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
               // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif

       bool isCirc = Logger_getbufferType();
       if (isCirc)
       {
           CLI_cliPrintf("\r\nCircular");
       }
       else
       {
           CLI_cliPrintf("\r\nOne Shot");
       }
       CLI_startREAD();
       return CRS_SUCCESS;
}
static CRS_retVal_t CLI_loggerGetTimeParsing(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_GET_TIME)]), s);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
               if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
               // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif

           uint32_t time = Logger_getTime();
           CLI_cliPrintf("\r\n%d", time);
           CLI_startREAD();
           return CRS_SUCCESS;
}
static CRS_retVal_t CLI_loggerGetLevelParsing(char *line)
{
    const char s[2] = " ";
       char *token;
       char tmpBuff[CUI_NUM_UART_CHARS] = { 0 };

       memcpy(tmpBuff, line, CUI_NUM_UART_CHARS);
       /* get the first token */
       //0xaabb shortAddr
       token = strtok(&(tmpBuff[sizeof(CLI_LOGGER_GET_LEVEL)]), s);
       //shortAddr in decimal
       uint32_t shortAddr = strtoul(&(token[2]), NULL, 16);

       #ifndef CLI_SENSOR

           uint16_t addr = 0;
           Cllc_getFfdShortAddr(&addr);
           if (addr != shortAddr)
           {
               // CLI_cliPrintf("\r\nStatus: 0x%x", CRS_SHORT_ADDR_NOT_VALID);
               ApiMac_sAddr_t dstAddr;
               dstAddr.addr.shortAddr = shortAddr;
               dstAddr.addrMode = ApiMac_addrType_short;
               Collector_status_t stat;
               stat = Collector_sendCrsMsg(&dstAddr, (uint8_t*)line);
               if (stat != Collector_status_success)
               {
                   CLI_cliPrintf("\r\nStatus: 0x%x", CRS_FAILURE);
                   CLI_startREAD();
               }
               // CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

               return CRS_SUCCESS;
           }
       #endif
           CLI_cliPrintf("\r\n");
           logLevels_t level = Logger_getThrshLogLevel();
           Logger_printLogLevel(level);
           CLI_startREAD();
           return CRS_SUCCESS;
}


static CRS_retVal_t CLI_helpParsing(char *line)
{
    CLI_printCommInfo("\r\nCOMMAND", strlen("COMMAND"), "PARAMS");

    CLI_cliPrintf("\r\n");
    CLI_printCommInfo(CLI_CRS_HELP, strlen(CLI_CRS_HELP), "");
    CLI_printCommInfo(CLI_CRS_HELP_2, strlen(CLI_CRS_HELP_2),"");
    CLI_printCommInfo(CLI_LIST_ALARMS_LIST, strlen(CLI_LIST_ALARMS_LIST), "[shortAddr]");
    CLI_printCommInfo(CLI_LIST_ALARMS_SET, strlen(CLI_LIST_ALARMS_SET), "[shortAddr] [id] [state]");
    //CLI_printCommInfo(CLI_LIST_ALARMS_START, strlen(CLI_LIST_ALARMS_START), "");
    //CLI_printCommInfo(CLI_LIST_ALARMS_STOP, strlen(CLI_LIST_ALARMS_STOP), "");

    CLI_printCommInfo(CLI_CRS_CONFIG_FILE, strlen(CLI_CRS_CONFIG_FILE), "[shortAddr] [flat filename]");

    CLI_printCommInfo(CLI_CRS_ENV_FORMAT, strlen(CLI_CRS_ENV_FORMAT), "[shortAddr] ");
    CLI_printCommInfo(CLI_CRS_ENV_LS, strlen(CLI_CRS_ENV_LS), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_ENV_RESTORE, strlen(CLI_CRS_ENV_RESTORE), "[shortAddr] ");
    CLI_printCommInfo(CLI_CRS_ENV_RM, strlen(CLI_CRS_ENV_RM), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_ENV_UPDATE, strlen(CLI_CRS_ENV_UPDATE), "[shortAddr] [key1=value1 key2=value2 ...]");

    CLI_printCommInfo(CLI_CRS_FPGA_CLOSE, strlen(CLI_CRS_FPGA_CLOSE), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_FPGA_OPEN, strlen(CLI_CRS_FPGA_OPEN), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_FPGA_WRITELINES, strlen(CLI_CRS_FPGA_WRITELINES), "[shortAddr] [lines seperated by new line char]");
    CLI_printCommInfo(CLI_CRS_FPGA_READLINES, strlen(CLI_CRS_FPGA_READLINES),"[shortAddr]");
    //CLI_printCommInfo(CLI_CRS_FPGA_TRANSPARENT_START, strlen(CLI_CRS_FPGA_TRANSPARENT_START), "[shortAddr]");
    //CLI_cliPrintf("%");
    //CLI_printCommInfo(CLI_CRS_FPGA_TRANSPARENT_END, strlen(CLI_CRS_FPGA_TRANSPARENT_END), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_FS_DELETE, strlen(CLI_CRS_FS_DELETE), "[shortAddr] [filename]");
    CLI_printCommInfo(CLI_CRS_FS_FORMAT, strlen(CLI_CRS_FS_FORMAT), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_FS_INSERT, strlen(CLI_CRS_FS_INSERT), "[shortAddr] [filename] [lines seperated by new line char]");
    CLI_printCommInfo(CLI_CRS_FS_LS, strlen(CLI_CRS_FS_LS), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_FS_READFILE, strlen(CLI_CRS_FS_READFILE), "[shortAddr] [filename] [startIndex] [readSize]");
//#define CLI_MODE_SLAVE "slave"
//#define CLI_MODE_NATIVE "native"
//
//#define CLI_SNAP "snap"
//#define CLI_SCRIPT "script"

    CLI_printCommInfo(CLI_CRS_FS_UPLOAD_DIG, strlen(CLI_CRS_FS_UPLOAD_DIG), "[shortAddr] [filename] [mode (" CLI_MODE_SLAVE "/" CLI_MODE_NATIVE ")] [chip number] [param=value]");
    CLI_printCommInfo(CLI_CRS_FS_UPLOAD_FPGA, strlen(CLI_CRS_FS_UPLOAD_FPGA), "[shortAddr] [filename] [mode (" CLI_MODE_SLAVE "/" CLI_MODE_NATIVE ")] [param=value]");
    CLI_printCommInfo(CLI_CRS_FS_UPLOAD_RF, strlen(CLI_CRS_FS_UPLOAD_RF), "[shortAddr] [filename] [mode (" CLI_MODE_SLAVE "/" CLI_MODE_NATIVE ")] [filetype (" CLI_SNAP "/" CLI_SCRIPT ")] [rf address] [lut line number] [param=value]");

    CLI_printCommInfo(CLI_CRS_OAD_FORMAT, strlen(CLI_CRS_OAD_FORMAT), "");
#ifndef CLI_SENSOR
    CLI_printCommInfo(CLI_CRS_OAD_GET_IMG_VER, strlen(CLI_CRS_OAD_GET_IMG_VER), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_OAD_RCV_IMG, strlen(CLI_CRS_OAD_RCV_IMG), "");
    CLI_printCommInfo(CLI_CRS_OAD_SEND_IMG, strlen(CLI_CRS_OAD_SEND_IMG), "[shortAddr] [reset](0x0:False, 0x1:True) [factory](0x0:False, 0x1:True)");
    CLI_printCommInfo(CLI_CRS_UPDATE_IMG, strlen(CLI_CRS_UPDATE_IMG), "[shortAddr] [reset](0x0:False, 0x1:True)");
    CLI_printCommInfo(CLI_CRS_OAD_RCV_FACTORY_IMG, strlen(CLI_CRS_OAD_RCV_FACTORY_IMG), "");
#endif
    CLI_printCommInfo(CLI_CRS_OAD_FACTORY_IMG, strlen(CLI_CRS_OAD_FACTORY_IMG), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_OAD_FACTORY_CHECK, strlen(CLI_CRS_OAD_FACTORY_CHECK), "[shortAddr]");


    CLI_printCommInfo(CLI_CRS_RESET, strlen(CLI_CRS_RESET), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_RSSI, strlen(CLI_CRS_RSSI), "[shortAddr]");



    CLI_printCommInfo(CLI_CRS_TDD_ALLOC, strlen(CLI_CRS_TDD_ALLOC), "[shortAddr] [alloc]");
    CLI_printCommInfo(CLI_CRS_TDD_CLOSE, strlen(CLI_CRS_TDD_CLOSE), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TDD_FRAME, strlen(CLI_CRS_TDD_FRAME), "[shortAddr] [frame]");
    CLI_printCommInfo(CLI_CRS_TDD_GET_LOCK, strlen(CLI_CRS_TDD_GET_LOCK), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TDD_HOL, strlen(CLI_CRS_TDD_HOL), "[shortAddr] [min] [sec]");
    CLI_printCommInfo(CLI_CRS_TDD_OPEN, strlen(CLI_CRS_TDD_OPEN), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TDD_CMD, strlen(CLI_CRS_TDD_CMD), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TDD_RTG, strlen(CLI_CRS_TDD_RTG), "[shortAddr] [val 1] [val 2] [val 3] [val 4](-0x80 to 0x7f)");
    CLI_printCommInfo(CLI_CRS_TDD_SCS, strlen(CLI_CRS_TDD_SCS), "[shortAddr] [scs](0x1:15Hz, 0x2:30Hz, 0x4:60Hz)");
    CLI_printCommInfo(CLI_CRS_TDD_SS_POS, strlen(CLI_CRS_TDD_SS_POS), "[shortAddr] [ss_pos]");
    CLI_printCommInfo(CLI_CRS_TDD_STATUS, strlen(CLI_CRS_TDD_STATUS), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TDD_SYNC_MODE, strlen(CLI_CRS_TDD_SYNC_MODE), "[shortAddr] [mode](0x0:Auto, 0x1:Manual)");
    CLI_printCommInfo(CLI_CRS_TDD_TTG, strlen(CLI_CRS_TDD_TTG), "[shortAddr] [val 1] [val 2] [val 3] [val 4](-0x80 to 0x7f)");
    CLI_printCommInfo(CLI_CRS_TDD_PERIOD1, strlen(CLI_CRS_TDD_PERIOD1), "[shortAddr] [time]");
    CLI_printCommInfo(CLI_CRS_TDD_DL1, strlen(CLI_CRS_TDD_DL1), "[shortAddr] [time]");

    CLI_printCommInfo(CLI_CRS_TRSH_FORMAT, strlen(CLI_CRS_TRSH_FORMAT), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TRSH_LS, strlen(CLI_CRS_TRSH_LS), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_TRSH_RESTORE, strlen(CLI_CRS_TRSH_RESTORE), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_TRSH_RM, strlen(CLI_CRS_TRSH_RM), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_TRSH_UPDATE, strlen(CLI_CRS_TRSH_UPDATE), "[shortAddr] [key1=value1 key2=value2 ...]");

    CLI_printCommInfo(CLI_CRS_GET_TIME, strlen(CLI_CRS_GET_TIME), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_SET_TIME, strlen(CLI_CRS_SET_TIME), "[shortAddr] [time](unix time)");

    CLI_printCommInfo(CLI_CRS_GET_GAIN, strlen(CLI_CRS_GET_GAIN), "[shortAddr] [state](dc_rf_high_freq_hb_rx, uc_rf_high_freq_hb_tx, uc_if_low_freq_rx, dc_if_low_freq_tx)");

    CLI_printCommInfo(CLI_CRS_TMP, strlen(CLI_CRS_TMP), "[shortAddr]");

    CLI_printCommInfo(CLI_CRS_LED_MODE, strlen(CLI_CRS_LED_MODE), "[shortAddr] [mode](0x0:Off, 0x1:On)");
    CLI_printCommInfo(CLI_CRS_RF_RUN, strlen(CLI_CRS_RF_RUN), "[shortAddr] [filename] [chipNumber] [lineNumber] [params]");

    //CLI_printCommInfo(CLI_CRS_LED_ON, strlen(CLI_CRS_LED_ON), "[shortAddr]");
    //CLI_printCommInfo(CLI_CRS_LED_OFF, strlen(CLI_CRS_LED_OFF), "[shortAddr]");

    //CLI_printCommInfo(CLI_CRS_WATCHDOG_DISABLE, strlen(CLI_CRS_WATCHDOG_DISABLE), "[shortAddr]");

    CLI_cliPrintf("\r\n");

    CLI_startREAD();
    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_help2Parsing(char *line)
{
    CLI_printCommInfo(CLI_CRS_LED_MODE, strlen(CLI_CRS_LED_MODE), "[shortAddr] [mode](0x0:Off, 0x1:On)");
#ifndef CLI_SENSOR
    CLI_printCommInfo(CLI_LIST_SENSORS, strlen(CLI_LIST_SENSORS), "");
#endif
    CLI_printCommInfo(CLI_DEVICE, strlen(CLI_DEVICE), "");
    CLI_printCommInfo(CLI_DISCOVER_MODULES, strlen(CLI_DISCOVER_MODULES), "[shortAddr] [fileName]");
    CLI_printCommInfo(CLI_CRS_MODEM_TEST, strlen(CLI_CRS_MODEM_TEST), "[time]");
    CLI_printCommInfo(CLI_CRS_LOCKS, strlen(CLI_CRS_LOCKS), "[shortAddr]");

    CLI_printCommInfo(CLI_AGC, strlen(CLI_AGC), "[shortAddr]");
    CLI_printCommInfo(CLI_AGC_DEBUG, strlen(CLI_AGC_DEBUG), "[shortAddr] [channel](0x1-0x4, 0x0: All channels) [DL/UL](0x0: Both, 0x1: DL, 0x2:UL) [RF/IF](0x0: Both, 0x1: RF, 0x2:IF) [type](0x0: All, 0x1:Max, 0x2:Avg, 0x3: Min)");
    CLI_printCommInfo(CLI_CSV_SENSOR, strlen(CLI_CSV_SENSOR),"[shortAddr]");
    CLI_printCommInfo(CLI_AGC_MODE, strlen(CLI_AGC_MODE), "[shortAddr] [mode](0x0: Auto, 0x1: DL, 0x2: UL)");
    CLI_printCommInfo(CLI_AGC_GET_MODE, strlen(CLI_AGC_GET_MODE), "[shortAddr]");
    CLI_printCommInfo(CLI_AGC_CHANNEL, strlen(CLI_AGC_CHANNEL), "[shortAddr] [channel](0x0: All channels, 0x1-0x4 select channel)");
    CLI_printCommInfo(CLI_AGC_SET_GAP, strlen(CLI_AGC_SET_GAP), "[shortAddr] ['start'|'stop'] ['rising'|'falling'] [0xus]");
    CLI_printCommInfo(CLI_AGC_GET_GAP, strlen(CLI_AGC_GET_GAP), "[shortAddr] ['start'|'stop'] ['rising'|'falling']");
    CLI_printCommInfo(CLI_AGC_SET_TIME_MINMAX, strlen(CLI_AGC_SET_TIME_MINMAX), "[shortAddr] [seconds]");


    CLI_printCommInfo(CLI_CRS_INIT_GAIN_FORMAT, strlen(CLI_CRS_INIT_GAIN_FORMAT), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_INIT_GAIN_LS, strlen(CLI_CRS_INIT_GAIN_LS), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_INIT_GAIN_RESTORE, strlen(CLI_CRS_INIT_GAIN_RESTORE), "[shortAddr]");
    CLI_printCommInfo(CLI_CRS_INIT_GAIN_RM, strlen(CLI_CRS_INIT_GAIN_RM), "[shortAddr] [key1 key2 ...]");
    CLI_printCommInfo(CLI_CRS_INIT_GAIN_UPDATE, strlen(CLI_CRS_INIT_GAIN_UPDATE), "[shortAddr] [key1=value1 key2=value2 ...]");
    CLI_printCommInfo(CLI_SCRIPT_RETVAL, strlen(CLI_SCRIPT_RETVAL), "[shortAddr] [key]");

    CLI_printCommInfo(CLI_CRS_TDD_DL2, strlen(CLI_CRS_TDD_DL2), "[shortAddr] [value]");
    CLI_printCommInfo(CLI_CRS_TDD_PATTERN2, strlen(CLI_CRS_TDD_PATTERN2), "[shortAddr] [value]");
    CLI_printCommInfo(CLI_CRS_TDD_PERIOD2, strlen(CLI_CRS_TDD_PERIOD2), "[shortAddr] [value]");



    CLI_cliPrintf("\r\n");

    CLI_startREAD();
    return CRS_SUCCESS;
}


static CRS_retVal_t CLI_printCommInfo(char *command, uint32_t commSize, char* description)
{
    if (commSize > 30)
    {
        return CRS_FAILURE;
    }

    uint32_t numSpaces = 30;
    char spacesStr[100] = {0};

    memset(spacesStr, ' ', numSpaces - commSize);

    if (strlen(command) == strlen(CLI_CRS_FPGA_TRANSPARENT_END) && memcmp(CLI_CRS_FPGA_TRANSPARENT_END, command, strlen(CLI_CRS_FPGA_TRANSPARENT_END) == 0))
    {
        CLI_cliPrintf("\r\n%c%s%s%s", CONTROL_CH_STOP_TRANS, command, spacesStr, description);

    }
    else
    {
        CLI_cliPrintf("\r\n%s%s%s", command, spacesStr, description);

    }
    return CRS_SUCCESS;

}


CRS_retVal_t CLI_startREAD()
{
    if ((gUartHandle == NULL) && gIsRemoteCommand == false )
       {
           return CRS_UART_FAILURE;
       }
#ifdef CLI_SENSOR

    if (gIsRemoteCommand == true || (gIsRemoteTransparentBridge == true && gRspBuff != 0))
    {
        gIsRemoteCommand = false;

        Msgs_sendMsgs(gDstAddr);
//        Sensor_sendCrsRsp(gDstAddr, gRspBuff);
//        memset(gRspBuff, 0, RSP_BUFFER_SIZE);
//        gRspBuffIdx = 0;
        return CRS_SUCCESS;
    }
#endif

#ifdef CLI_CEU_CL
if (gIsUartCommCommand) {
    gIsUartCommCommand=false;
    Msgs_sendMsgs();
    if (gIsUartCommCommand) {
                  gIsUartCommCommand=false;
                  gIsUartCommCommandRemoteCL=false;
              }
    return CRS_SUCCESS;
//      Mediator_msgObjSentToAppCli_t msg={0};
//      uint8_t* tmp=CRS_malloc(gRspIdxUartComm);
//      memset(tmp, 0, gRspIdxUartComm);
//      memcpy(tmp, gRspBuffUartComm, gRspIdxUartComm);
//      memset(gRspBuffUartComm, 0, gRspIdxUartComm);
//      msg.p=tmp;
//      msg.len=gRspIdxUartComm;
//      gRspIdxUartComm=0;
//      Mediator_sendMsgToUartComm(&msg);
}
#endif

//
    if (gIsTranRemoteCommandSent)
    {
        UART_read(gUartHandle, gUartRxBuffer, 1);

        gIsTranRemoteCommandSent = false;
        return CRS_SUCCESS;
    }
    if (gIsTransparentBridge == false && gIsUartCommInParts==false)
    {
        CLI_writeString(CLI_PROMPT, strlen(CLI_PROMPT));
    }
//    gIsNewCommand = true;
    memset(gUartTxBuffer, 0, CUI_NUM_UART_CHARS - 1);
    memset(gTmpUartTxBuffer, 0, LAST_COMM_COMM_SIZE - 1);

    gUartTxBufferIdx = 0;
    gIsRemoteCommand = false;
    if (gReadNextCommand == false)
    {
        while(gIsDoneWriting == false){};
        gReadNextCommand = true;
//        UART_read(gUartHandle, gUartRxBuffer, 1);

    }



    AGCM_finishedTask();

    return CRS_SUCCESS;

}


static CRS_retVal_t defaultTestLog( const log_level level, const char* file, const int line, const char* format, ... )
{
    if (strlen(format) >= 512)
       {
           return CRS_FAILURE;
       }
       char printBuff[512] = { 0 };
       if (format == NULL)
       {
           return CRS_SUCCESS;
       }
                va_list args;

                switch( level )
                {
                case CRS_INFO:
                    CLI_cliPrintf( "\r\n[INFO   ] %s:%d : ", file, line);
                        break;
                case CRS_DEBUG:
                   return CRS_SUCCESS;
//                   CLI_cliPrintf( "\r\n[DEBUG  ] %s:%d : ", file, line);
//                        break;
                case CRS_ERR:
//                  return;

                  CLI_cliPrintf( "\r\n[ERROR  ] %s:%d : ", file, line);
                        break;
                case CRS_WARN:
//                   return;

                   CLI_cliPrintf( "\r\n[WARNING] %s:%d : ", file, line);
                        break;
                }
//                if (level == ERR || level == WARN)
//                {
//                        va_start(args, format);
//                        vprintf(format, args);
//                        va_end(args);
//                        printf("\n");
//                }
                va_start(args, format);
                SystemP_vsnprintf(printBuff, sizeof(printBuff), format, args);
                va_end(args);
                CLI_cliPrintf(printBuff);
                return CRS_SUCCESS;

}

/*********************************************************************
 * @param       _clientHandle - Client handle that owns the status line
 *              ... - Var args to be formated by _format
 *
 * @return      CUI_retVal_t representing success or failure.
 */
CRS_retVal_t CLI_cliPrintf(const char *_format, ...)
{
    if ((gUartHandle == NULL) && gIsRemoteCommand == false)
       {
           return CRS_UART_FAILURE;
       }
    if (strlen(_format) >= 1024)
    {
        return CRS_FAILURE;
    }
    char printBuff[1024] = { 0 };
    if (_format == NULL)
    {
        return CRS_SUCCESS;
    }
    va_list args;

    va_start(args, _format);
    SystemP_vsnprintf(printBuff, sizeof(printBuff), _format, args);
    va_end(args);
#ifdef CLI_SENSOR

    if (gIsRemoteCommand == true
            || (gIsRemoteTransparentBridge == true
                    && strstr(printBuff, "AP>") != NULL))
    {
        Msgs_addMsg((uint8_t *)printBuff, strlen(printBuff));
    }
#endif

#ifdef CLI_CEU_CL
if (gIsUartCommCommand) {

    Msgs_addMsg((uint8_t *)printBuff, strlen(printBuff));


//    memcpy((gRspBuffUartComm+gRspIdxUartComm), printBuff, strlen(printBuff));
//    gRspIdxUartComm+=strlen(printBuff);


}
#endif

if (gIsRemoteCommand == false) {
    CLI_writeString(printBuff, strlen(printBuff));
}


    return CRS_SUCCESS;
}


/*********************************************************************
 * Private Functions
 */

static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size)
{

    if (_size != gWriteNowBuffSize)
    {
        gWriteNowBuffIdx = gWriteNowBuffIdx + _size;
        gWriteNowBuffSize = gWriteNowBuffSize - _size;
        UART_write(gUartHandle, (void *)&gWriteNowBuff[gWriteNowBuffIdx],
                   gWriteNowBuffSize);

        return;
    }
    gIsDoneWriting = true;

    if (gIsDoneFilling)
    {

        memset((void *)gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy((void *)gWriteNowBuff, (void *)gWriteWaitingBuff, gWriteWaitingBuffIdx);
        gWriteNowBuffSize = gWriteWaitingBuffIdx;
        memset((void *)gWriteWaitingBuff, 0, gWriteWaitingBuffIdx + 1);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
        gIsDoneFilling = false;
        UART_write(gUartHandle, (void *)gWriteNowBuff, gWriteNowBuffSize);

        return ;
    }

    if (gIsNoPlaceForPrompt == true)
    {
        gIsNoPlaceForPrompt = false;
        gIsDoneWriting = false;

        memset((void *)gWriteNowBuff, 0, strlen(CLI_PROMPT) + 1);
        memcpy((void *)gWriteNowBuff, CLI_PROMPT, strlen(CLI_PROMPT));
        gWriteNowBuffSize = strlen(CLI_PROMPT);
        gWriteNowBuffIdx = 0;

        UART_write(gUartHandle,(void *) gWriteNowBuff, gWriteNowBuffSize);
    }

    //    }
}

static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size)
{
    if (_size)
    {
        if (gReadNextCommand == false)
        {
            UART_read(gUartHandle, gUartRxBuffer, 1);
            return;
        }
        if (gUartTxBufferIdx == CUI_NUM_UART_CHARS-10)
        {
            if ( ((uint8_t*) _buf)[0] != '\r')
            {
                UART_read(gUartHandle, gUartRxBuffer, 1);

                return;
            }

        }
        if (gIsTransparentBridge == true)
        {
            gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*) _buf)[0];
            gUartTxBufferIdx++;
            uint8_t input = ((uint8_t*) _buf)[0];
            memset(_buf, '\0', _size);

            if (gIsTransparentBridgeSuspended == true)
            {
                if (input == '\r')
                {
                    gUartTxBuffer[gUartTxBufferIdx - 1] = 0;

                    if (memcmp(CLI_CRS_FPGA_TRANSPARENT_END, gUartTxBuffer,
                               sizeof(CLI_CRS_FPGA_TRANSPARENT_END) - 1) == 0)
                    {
                        gReadNextCommand = false;
                                    UART_read(gUartHandle, gUartRxBuffer, 1);
#ifndef CLI_SENSOR
                        Csf_processCliUpdate();
#else
                        Ssf_processCliUpdate();
#endif
                        return ;
                    }

                    memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
                    gUartTxBufferIdx = 0;
                    gIsTransparentBridgeSuspended = false;
                    UART_read(gUartHandle, gUartRxBuffer, 1);

                    return ;
                }

                if (input == '\b')
                {
                    if (gUartTxBufferIdx - 1)
                    {
                        //need to conncatinate all of the strings to one.
                        gUartTxBufferIdx--;
                        gUartTxBuffer[gUartTxBufferIdx] = '\0';

                        gUartTxBuffer[gUartTxBufferIdx - 1] = input;
#ifndef CLI_NO_ECHO
                        CLI_writeString(&gUartTxBuffer[gUartTxBufferIdx - 1],
                                        1);
                        CLI_writeString(" ", 1);
                        CLI_writeString(&gUartTxBuffer[gUartTxBufferIdx - 1],
                                        1);
#endif
                        gUartTxBuffer[gUartTxBufferIdx - 1] = '\0';
                        gIsDoneFilling = true;
                    }

                    gUartTxBufferIdx--;

                    UART_read(gUartHandle, gUartRxBuffer, 1);
                    return ;
                }

#ifndef CLI_NO_ECHO

                CLI_writeString(&input, 1);
#endif

                UART_read(gUartHandle, gUartRxBuffer, 1);
                return;
            }
            if (input == '\r')
            {
#ifndef CLI_SENSOR
                uint16_t addr = 0;
                Cllc_getFfdShortAddr(&addr);
                if (addr != gTransparentShortAddr)
                {
                    gReadNextCommand = false;
                                UART_read(gUartHandle, gUartRxBuffer, 1);

                    Csf_processCliSendMsgUpdate();
                    //CLI_cliPrintf("\r\nSent req. stat: 0x%x", stat);

                    return;
                }
                gReadNextCommand = false;
                            UART_read(gUartHandle, gUartRxBuffer, 1);
#endif
#ifndef CLI_SENSOR
                Csf_processCliUpdate();
#else
                Ssf_processCliUpdate();
#endif

                return;

            }

            if (input == CONTROL_CH_STOP_TRANS)
            {
                gIsTransparentBridgeSuspended = true;
                memset(gUartTxBuffer, '\0', CUI_NUM_UART_CHARS - 1);
                gUartTxBufferIdx = 0;
#ifndef CLI_NO_ECHO

                CLI_writeString(&input, 1);
#endif
                UART_read(gUartHandle, gUartRxBuffer, 1);

                return ;
            }
            UART_read(gUartHandle, gUartRxBuffer, 1);

            return ;
        }


        gUartTxBuffer[gUartTxBufferIdx] = ((uint8_t*) _buf)[0];
        gUartTxBufferIdx++;
        memset(_buf, '\0', _size);

        uint8_t input = gUartTxBuffer[gUartTxBufferIdx - 1];

        if (input != '\r')
        {
            //memset(gUartTxBuffer, '\0', sizeof(gUartTxBuffer));
            if (input == '\b')
            {
                if (gUartTxBufferIdx - 1)
                {
                    //need to conncatinate all of the strings to one.
                    gUartTxBufferIdx--;
                    gUartTxBuffer[gUartTxBufferIdx] = '\0';

                    gUartTxBuffer[gUartTxBufferIdx - 1] = input;
#ifndef CLI_NO_ECHO
                    CLI_writeString(&gUartTxBuffer[gUartTxBufferIdx - 1], 1);
                    CLI_writeString(" ", 1);
                    CLI_writeString(&gUartTxBuffer[gUartTxBufferIdx - 1], 1);
#endif
                    gUartTxBuffer[gUartTxBufferIdx - 1] = '\0';
                    gIsDoneFilling = true;
                }

                gUartTxBufferIdx--;

                UART_read(gUartHandle, gUartRxBuffer, 1);
                return ;
            }

#ifndef CLI_NO_ECHO

            CLI_writeString(&input, 1);
#endif

            UART_read(gUartHandle, gUartRxBuffer, 1);
            return;

        }
        else
        {
            gReadNextCommand = false;
            UART_read(gUartHandle, gUartRxBuffer, 1);

#ifndef CLI_SENSOR
            AGCM_runTask(Csf_processCliUpdate);
#else
            AGCM_runTask(Ssf_processCliUpdate);
#endif

        }

    }
    else
    {
        UART_read(gUartHandle, gUartRxBuffer, 1);

        // Handle error or call to UART_readCancel()
//        UART_readCancel(gUartHandle);
    }
}

static CRS_retVal_t CLI_writeString(void *_buffer, size_t _size)
{
    /*
     * Since the UART driver is in Callback mode which is non blocking.
     *  If UART_write is called before a previous call to UART_write
     *  has completed it will not be printed. By taking a quick
     *  nap we can attempt to perform the subsequent write. If the
     *  previous call still hasn't finished after this nap the write
     *  will be skipped as it would have been before.
     */

    //Error if no buffer
    if ((gUartHandle == NULL))
    {
        return CRS_UART_FAILURE;
    }

    if (_buffer == NULL)
    {
        return CRS_SUCCESS;
    }

    if (gIsDoneWriting == true)
    {
        if (gWriteWaitingBuffIdx + _size < UART_WRITE_BUFF_SIZE)
        {
            memcpy((void *)&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
            gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;
        }

        gWriteNowBuffSize = gWriteWaitingBuffIdx;

        memset((void *)gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy((void *)gWriteNowBuff, (void *)gWriteWaitingBuff, gWriteWaitingBuffIdx);
        memset((void *)gWriteWaitingBuff, 0, gWriteWaitingBuffIdx);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
//        gWriteNowBuffTotalSize = gWriteNowBuffSize;
        UART_write(gUartHandle, (void *)gWriteNowBuff, gWriteNowBuffSize);

        return CRS_SUCCESS;
    }
    else
    {
        if (gWriteWaitingBuffIdx + _size >= UART_WRITE_BUFF_SIZE)
        {
            if (_size >= strlen(CLI_PROMPT)
                    && memcmp(CLI_PROMPT, _buffer, strlen(CLI_PROMPT)) == 0)
            {
                gIsDoneFilling = true;
                gIsNoPlaceForPrompt = true;
            }
            return CRS_SUCCESS;
        }
//        bool flag = false;
        while (gIsDoneFilling == true)
        {
//flag  = true;
        }



        memcpy((void *)&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
        gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;

        if (_size >= strlen(CLI_PROMPT)
                && memcmp(CLI_PROMPT, _buffer, strlen(CLI_PROMPT)) == 0)
        {
            gIsDoneFilling = true;
        }
        char *cliPrompt = CLI_PROMPT;
        if (_size >= strlen(CLI_PROMPT)
                && memcmp(cliPrompt + 2,(char *) _buffer + 1, strlen(CLI_PROMPT) - 2)
                        == 0)
        {
            gIsDoneFilling = true;
        }

        if (((gIsTransparentBridge == true)
                && (((uint8_t*) _buffer)[_size - 1] == '>'))
                || (gIsTransparentBridge == true && _size >= 2
                        && ((uint8_t*) _buffer)[_size - 2] == '>'))
        {
            gIsDoneFilling = true;
        }
        if (gIsDoneWriting && gIsDoneFilling)
                {



                    gWriteNowBuffSize = gWriteWaitingBuffIdx;

                    memset((void *)gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
                    memcpy((void *)gWriteNowBuff,(void *) gWriteWaitingBuff, gWriteWaitingBuffIdx);
                    memset((void *)gWriteWaitingBuff, 0, gWriteWaitingBuffIdx);
                    gWriteWaitingBuffIdx = 0;
                    gWriteNowBuffIdx = 0;
                    gIsDoneWriting = false;
//                    gWriteNowBuffTotalSize = gWriteNowBuffSize;
                    UART_write(gUartHandle, (void *)gWriteNowBuff, gWriteNowBuffSize);

                    return CRS_SUCCESS;
                }

        return CRS_SUCCESS;
    }
}



static int CLI_hex2int(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

uint32_t CLI_convertStrUint(char *st)
{
    char *x;

    uint32_t i = 0;
    if(strlen(st) == 0){
            return 0;
        }
    uint32_t len = strlen(st) - 1;
    uint32_t base;
    (len) ? (base = 16) : (base = 1);
    for (i = 1; i < len; i++)
    {
        base *= 16;
    }
    uint32_t result = 0;
    for (x = st; *x; x++)
    {
        result += base * CLI_hex2int(*x);
        base /= 16;
    }
    return result;
}

static void fpgaMultiLineCallback(const FPGA_cbArgs_t _cbArgs)
{
    CLI_startREAD();
}

static void tddCallback(const TDD_cbArgs_t _cbArgs)
{
    CLI_cliPrintf("\r\nStatus: 0x%x", _cbArgs.status);
      CLI_startREAD();
}

static void tddOpenCallback(const TDD_cbArgs_t _cbArgs)
{
    if(Tdd_isOpen() != CRS_SUCCESS){
           Tdd_close();
       }
       CLI_cliPrintf("\r\nStatus: 0x%x", _cbArgs.status);
       CLI_startREAD();
}

char* int2hex(uint32_t num, char *outbuf)
{
    sprintf(outbuf, "%x", num);
    return outbuf;
}

static CRS_retVal_t CLI_convertExtAddrTo2Uint32(ApiMac_sAddrExt_t  *extAddr, uint32_t* left, uint32_t* right)
{
    uint32_t leftPart = 0, rightPart = 0;
    uint32_t mask = 0;

    rightPart = 0;
    leftPart = 0;
    rightPart |= (*extAddr)[7];

    mask |= (*extAddr)[6];
    mask = mask << 8;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[5];
    mask = mask << 16;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[4];
    mask = mask << 24;
    rightPart |= mask;
    mask = 0;

    mask |= (*extAddr)[3];
    mask = mask << 0;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[2];
    mask = mask << 8;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[1];
    mask = mask << 16;
    leftPart |= mask;
    mask = 0;

    mask |= (*extAddr)[0];
    mask = mask << 24;
    leftPart |= mask;
    mask = 0;

    *left = leftPart;
    *right = rightPart;
    return CRS_SUCCESS;
}

static void convertUintToDate(uint32_t date, char dateStr[DATE_STR_LEN])
{
    int8_t dateFormatIter = DATE_STR_LEN - 1;

    char dateNumberStr [CUI_NUM_UART_CHARS] = {0};
    sprintf(dateNumberStr, "%x", date);
    uint8_t dateNumberSize = strlen(dateNumberStr);
    int8_t dateNumberIter = dateNumberSize - 1;

    if (DATE_STR_LEN - 2 > dateNumberSize) // if day is less than 9
    {
        dateStr[0] = '0';
    }

    for (; dateFormatIter >= 0 && dateNumberIter >= 0; dateFormatIter--) // date string should be 03/01/2023, extra 2 slots
    {

        if (2 == dateFormatIter || 5 == dateFormatIter)
        {
            dateStr[dateFormatIter] = '/';
            continue;
        }
        dateStr[dateFormatIter] = dateNumberStr[dateNumberIter];
        dateNumberIter--;
    }
}

//void CLI_printHeapStatus()
//{
//    /**
//     * @brief   obtain heap usage metrics
//     * @param   BlkMax   pointer to a variable to store max cnt of all blocks ever seen at once
//     * @param   BlkCnt   pointer to a variable to store current cnt of all blocks
//     * @param   BlkFree  pointer to a variable to store current cnt of free blocks
//     * @param   MemAlo   pointer to a variable to store current total memory allocated
//     * @param   MemMax   pointer to a variable to store max total memory ever allocated at once
//     * @param   MemUB    pointer to a variable to store the upper bound of memory usage
//     */
//    uint32_t BlkMax = 0;
//    uint32_t BlkCnt = 0;
//    uint32_t BlkFree = 0;
//    uint32_t MemAlo = 0;
//    uint32_t MemMax = 0;
//    uint32_t MemUB = 0;
//
//    HEAPMGR_GETMETRICS(&BlkMax, &BlkCnt, &BlkFree, &MemAlo, &MemMax, &MemUB);
//    CLI_cliPrintf("BlkMax:0x%x\r\n", BlkMax);
//    CLI_cliPrintf("BlkCnt:0x%x\r\n", BlkCnt);
//    CLI_cliPrintf("BlkFree:0x%x\r\n", BlkFree);
//     CLI_cliPrintf("MemAlo:0x%x\r\n", MemAlo);
//     CLI_cliPrintf("MemMax:0x%x\r\n", MemMax);
//      CLI_cliPrintf("MemUB:0x%x\r\n", MemUB);
//}

