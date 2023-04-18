/*
 * crs_logger.h
 *
 *  Created on: Feb 14, 2023
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_CRS_LOGGER_H_
#define APPLICATION_CRS_CRS_LOGGER_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs.h"

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define PARAM_BYTES 16

typedef enum logLevels{
    logLevel_NOLOG,
    logLevel_ERR,
    logLevel_WARN,
    logLevel_INFO,
    logLevel_DEBUG,
    logLevel_NUMOFLEVELS
}logLevels_t;

typedef enum logMsgs{
    LogMsg_EMPTY,
    LogMsg_MALLOCFAIL,
    LogMsg_MALLOCSUCCESS,
    LogMsg_FILEREADERR,
    LogMsg_INVALIDSCRIPTLINE,
    LogMsg_VALIDSCRIPTLINE,
    LogMsg_CONFIGSUCCESS,
    LogMsg_CONFIGFAIL,
    LogMsg_FLATPARSEFINISH,
    LogMsg_TESTMSG,
    LogMsg_TDDNOTLOCKED,
    LogMsg_TINOTLOCKED,
    LogMsg_SENDINGBEACON,
    LogMsg_RECEIVEDBEACON,
    LogMsg_NEWNODE,
    LogMsg_ERASENODE,
    LogMsg_ASSOCREQTIMEOUT,
    LogMsg_SENTASSOCREQMSG,
    LogMsg_RECEIVEDASSOCRSPMSG,
    logMsg_NUMOFLOGMSGS
}logMsgs_t;


typedef CRS_retVal_t logZeroHandlerFunc_t(logLevels_t level, logMsgs_t msg);
typedef CRS_retVal_t logOneHandlerFunc_t(logLevels_t level, logMsgs_t msg, char p [PARAM_BYTES]);
typedef CRS_retVal_t logTwoHandlerFunc_t(logLevels_t level, logMsgs_t msg, char p1 [PARAM_BYTES/2], char p2 [PARAM_BYTES/2]);
typedef CRS_retVal_t logThreeHandlerFunc_t(logLevels_t level, logMsgs_t msg, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);
typedef CRS_retVal_t logFourHandlerFunc_t(logLevels_t level, logMsgs_t msg, char str[PARAM_BYTES - (sizeof(uint32_t))], uint32_t num);
typedef CRS_retVal_t logFiveHandlerFunc_t(logLevels_t level, logMsgs_t msg, uint8_t macAddr[PARAM_BYTES/2], char logMsg[PARAM_BYTES/2]);
typedef CRS_retVal_t logGeneralHandlerFunc_t(logLevels_t level, logMsgs_t msg, char *str, uint32_t num);



extern logZeroHandlerFunc_t *gLogZeroHandler;
extern logOneHandlerFunc_t *gLogOneHandler;
extern logTwoHandlerFunc_t *gLogTwoHandler;
extern logThreeHandlerFunc_t *gLogThreeHandler;
extern logFourHandlerFunc_t *gLogFourHandler;
extern logGeneralHandlerFunc_t *gLogGeneralHandler;
extern logFiveHandlerFunc_t *gLogFiveHandler;

// 0 param
#define LOG_0(LEVEL, MSG)\
    if(gLogZeroHandler)\
        gLogZeroHandler(LEVEL,MSG)

// 1 param, 16 byte string
#define LOG_1(LEVEL, MSG, STR) \
    if (gLogOneHandler) \
        gLogOneHandler(LEVEL,MSG,STR)

// 2 params, 6 byte strings
#define LOG_2(LEVEL, MSG, STR1, STR2) \
    if (gLogTwoHandler) \
        gLogTwoHandler(LEVEL,MSG,STR1, STR2)

// 4 params, 4 byte ints
#define LOG_3(LEVEL, MSG, UINT1, UINT2, UINT3, UINT4) \
    if (gLogThreeHandler) \
        gLogThreeHandler(LEVEL,MSG,UINT1, UINT2, UINT3,UINT4)

// 4 params, 12 byte string, 4 byte num
#define LOG_4(LEVEL, MSG, STR, UINT) \
    if(gLogFourHandler) \
        gLogFourHandler(LEVEL, MSG, STR, UINT)

// 4 params, 12 byte string, 4 byte num
#define LOG_5(LEVEL, MSG, UINTARR8, STR) \
    if(gLogFiveHandler) \
        gLogFiveHandler(LEVEL, MSG,UINTARR8, STR)

#define LOG_GENERAL(LEVEL,MSG)\
        if(gLogGeneralHandler) \
            gLogGeneralHandler(LEVEL, MSG,__FILE__, __LINE__)

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Logger_init(void);
void Logger_print(bool isPb);
CRS_retVal_t Logger_flush(void);
CRS_retVal_t Logger_stop(void);
CRS_retVal_t Logger_start(logLevels_t level);

CRS_retVal_t Logger_setThrshLogLevel(logLevels_t level);
CRS_retVal_t Logger_setTime(uint32_t time);
CRS_retVal_t Logger_setCircular(bool b);

logLevels_t Logger_getThrshLogLevel(void);
uint32_t Logger_getTime(void);
bool Logger_getbufferType(void);
void Logger_printLogLevel(logLevels_t level);




#endif /* APPLICATION_CRS_CRS_LOGGER_H_ */
