/*
 * crs_logger.c
 *
 *  Created on: Feb 14, 2023
 *      Author: yardenr
 */


/******************************************************************************
 Includes
 *****************************************************************************/
#include <ti/sysbios/hal/Seconds.h>
#include <ti/drivers/dpl/SystemP.h>

#include <stdbool.h>
#include <string.h>

#include "crs_logger.h"
#include"application/crs/crs_cli.h"

#include "nano_pb/pb_encode.h"
#include "nano_pb/pb_decode.h"
#include "nano_pb/loggerMessage.pb.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
#define NUM_OF_LOGS 5
#define MSG_DESC_LEN    8

typedef struct timeStamp{
    uint16_t sec;
    uint16_t mSec;
}timeStamp_t;

typedef struct loggerMessage{
    timeStamp_t timeStamp;
    logLevels_t logLevel;
    uint8_t opCode;
    uint16_t msg;
    union Params{
        uint32_t splitInts[PARAM_BYTES/sizeof(uint32_t)]; // 12 bytes divided by 4 bytes (sizeof uint32_t)
        char oneStr [PARAM_BYTES];
        char twoStrs [2] [PARAM_BYTES/2];
        struct strInt{
            char str [PARAM_BYTES-(sizeof(uint32_t))]; // 12 - 4 = 8
            uint32_t num; // 4
        } oneStrOneInt;

        struct uint8ArrStr{
            uint8_t uintArr[PARAM_BYTES/2];
            char str[PARAM_BYTES/2];
        }uint8ArrStr;
    } params;
}loggerMessage_t;

typedef struct circularBuffer{
    loggerMessage_t buffer[NUM_OF_LOGS];
    uint8_t head;
    uint8_t tail;
    uint8_t size;
}circularBuffer_t;

typedef struct logger{
    uint16_t time;
    circularBuffer_t circBuff;
}logger_t;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static void circBuff_clear(circularBuffer_t *circBuff);
static CRS_retVal_t circBuff_push(circularBuffer_t *circBuff, loggerMessage_t log);
static CRS_retVal_t circBuff_pop(circularBuffer_t *circBuff, loggerMessage_t *outLog);
static bool circBuff_isFull(circularBuffer_t *circBuff);


static inline CRS_retVal_t logZero(logLevels_t level, logMsgs_t msg);
static inline CRS_retVal_t logOne(logLevels_t level, logMsgs_t msg, char p[PARAM_BYTES]);
static inline CRS_retVal_t logTwo(logLevels_t level, logMsgs_t msg, char p1[PARAM_BYTES/2], char p2[PARAM_BYTES/2]);
static inline CRS_retVal_t logThree(logLevels_t level, logMsgs_t msg,uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);
static inline CRS_retVal_t logFour(logLevels_t level, logMsgs_t msg,char str[PARAM_BYTES - (sizeof(uint32_t))], uint32_t num);
static inline CRS_retVal_t logFive(logLevels_t level, logMsgs_t msg, uint8_t macAddr[PARAM_BYTES/2], char logMsg[PARAM_BYTES/2]);
static inline CRS_retVal_t logGeneral(logLevels_t level, logMsgs_t msg,char *str, uint32_t num);

inline static timeStamp_t getTimeStamp(void);
inline static bool isOverThrsh(logLevels_t level);

static void printLogEntry(loggerMessage_t *logPtr);
static CRS_retVal_t printLogEntryPb(loggerMessage_t *logPtr);
/******************************************************************************
 Local variables
 *****************************************************************************/
logger_t gLogger;

logZeroHandlerFunc_t *gLogZeroHandler = &logZero;
logOneHandlerFunc_t *gLogOneHandler = &logOne;
logTwoHandlerFunc_t *gLogTwoHandler = &logTwo;
logThreeHandlerFunc_t *gLogThreeHandler = &logThree;
logFourHandlerFunc_t *gLogFourHandler = &logFour;
logFiveHandlerFunc_t *gLogFiveHandler = &logFive;
logGeneralHandlerFunc_t *gLogGeneralHandler = &logGeneral;


logLevels_t gLogLevelThrsh = logLevel_INFO;

static bool gIsCircular = true;

char *gLoggerMsgDesc[logMsg_NUMOFLOGMSGS] = {
      "EMPTY",
      "MALC_F",
      "MALC_S",
      "FLRD_E",
      "SCT_IVD",
      "SCT_VD",
      "CFG_S",
      "CFG_F",
      "FLT_FIN",
      "TST_MSG",
      "TDD_ULK",
      "TI_ULK",
      "BKN_SND",
      "BKN_RCV",
      "NEW_NOD",
      "DEL_NOD",
      "RQ_TO",
      "S_RQMSG"
      "R_RSPMS",
};

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t Logger_init(void)
{
    Seconds_set(0);
    circBuff_clear(&(gLogger.circBuff));

    return CRS_SUCCESS;
}


void Logger_print(bool isPb)
{
    uint8_t count = 0;
    uint8_t pos = gLogger.circBuff.tail;
    if (!gIsCircular)
    {
        pos = 0;
    }
    loggerMessage_t log = gLogger.circBuff.buffer[pos];
    CLI_cliPrintf("\r\n");
    for (count = 0; count < gLogger.circBuff.size; count++)
    {
        log = gLogger.circBuff.buffer[pos];
        if(isPb)
        {
            printLogEntryPb(&log);
        }
        else
        {
            printLogEntry(&log);
        }
        if (pos + 1 == NUM_OF_LOGS)
        {
            pos = 0;
        }
        else
        {
            pos ++;
        }
    }
}

CRS_retVal_t Logger_flush(void)
{
    circBuff_clear(&(gLogger.circBuff));

    return CRS_SUCCESS;
}

CRS_retVal_t Logger_setThrshLogLevel(logLevels_t level)
{
    gLogLevelThrsh = level;

    return CRS_SUCCESS;
}

CRS_retVal_t Logger_setTime(uint32_t time)
{
    gLogger.time = (time & 0xFFFF0000) >> 0x10;
    return CRS_SUCCESS;
}

CRS_retVal_t Logger_stop(void)
{
    Logger_setThrshLogLevel(logLevel_NOLOG);

    return CRS_SUCCESS;
}
CRS_retVal_t Logger_start(logLevels_t level)
{
    Logger_setThrshLogLevel(level);

    return CRS_SUCCESS;
}

CRS_retVal_t Logger_setCircular(bool b)
{
    Logger_flush();
    gIsCircular = b;

    return CRS_SUCCESS;
}

logLevels_t Logger_getThrshLogLevel(void)
{
    return gLogLevelThrsh;
}
uint32_t Logger_getTime(void)
{
    return gLogger.time;
}

bool Logger_getbufferType(void)
{
    return gIsCircular;
}

void Logger_printLogLevel(logLevels_t level)
{
    switch(level)
    {
    case logLevel_DEBUG:
        CLI_cliPrintf("DEB");
        break;
    case logLevel_ERR:
            CLI_cliPrintf("ERR");
            break;
    case logLevel_WARN:
            CLI_cliPrintf("WAR");
            break;
    case logLevel_INFO:
            CLI_cliPrintf("INF");
            break;
    }
}

/******************************************************************************
 Local Functions
 *****************************************************************************/


inline static timeStamp_t getTimeStamp(void)
{
    Seconds_Time t = {0};
    uint32_t ret = 0;
    ret = Seconds_getTime(&t);

    timeStamp_t timeStamp = {0};
    timeStamp.sec = t.secs & 0x0000FFFF;
    timeStamp.mSec = (t.nsecs / 1000000);

    return timeStamp;
}


inline static bool isOverThrsh(logLevels_t level)
{
    return level > gLogLevelThrsh;
}



static void printLogEntry(loggerMessage_t *logPtr)
{
    loggerMessage_t log = *logPtr;

    if (LogMsg_EMPTY == log.msg)
    {
        return;
    }
    CLI_cliPrintf("%ds %dms, ", ((gLogger.time << 0x10) + log.timeStamp.sec) , log.timeStamp.mSec);
    Logger_printLogLevel(log.logLevel);
    CLI_cliPrintf(", ");
//        CLI_cliPrintf("opCode: %x, ",log.opCode); // doesnt matter to the print as of now
    CLI_cliPrintf("%s, ", gLoggerMsgDesc[log.msg]);
//    CLI_cliPrintf("%x, ", log.msg);
    switch(log.opCode)
    {
            case 0:
            {
               break;
            }

            case 1:
            {
                CLI_cliPrintf("%.*s", PARAM_BYTES, log.params.oneStr);
               break;
            }

            case 2:
            {
                CLI_cliPrintf("%.*s, %.*s",(PARAM_BYTES / 2), log.params.twoStrs[0],(PARAM_BYTES / 2), log.params.twoStrs[1]);
                break;
            }

            case 3:
            {
                uint8_t i = 0;
                for (i = 0; i < (PARAM_BYTES/sizeof(uint32_t)); i++)
                {
                    CLI_cliPrintf("%d: %d ",i, log.params.splitInts[i]);
                }
                break;
            }

            case 4:
            {
                CLI_cliPrintf("%d, ", log.params.oneStrOneInt.num);
                CLI_cliPrintf("%.*s",(PARAM_BYTES - sizeof(uint32_t)),log.params.oneStrOneInt.str);
                break;
            }

            case 5:
            {
                CLI_cliPrintf("%.*s, ", PARAM_BYTES/2, log.params.uint8ArrStr.str);
                CLI_cliPrintf("0x");
                uint8_t i = 0;
                for (i = 0; i < PARAM_BYTES/2; i++)
                {
                    CLI_cliPrintf("%x", log.params.uint8ArrStr.uintArr[i]);
                }

                break;
            }

    }
    CLI_cliPrintf("\r\n");

}

static CRS_retVal_t printLogEntryPb(loggerMessage_t *logPtr)
{
    /* This is the buffer where we will store our message. */
      uint8_t buffer[128];
      size_t message_length;
      bool status;

      /* Encode our message */

          /* Allocate space on the stack to store the message data.
           *
           * Nanopb generates simple struct definitions for all the messages.
           * - check out the contents of simple.pb.h!
           * It is a good idea to always initialize your structures
           * so that you do not have garbage data from RAM in there.
           */
      loggerMessage log = loggerMessage_init_zero;
      /* Create a stream that will write to our buffer. */
      pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

      /* Fill in the params */


//      char *p = "logMsg";
//      uint8_t arr [] = {1,2,3,4,5,6,7,8};
//
//      message.timeStamp.mSec=0;
//      message.timeStamp.sec=70;
//      message.logLevel=logLevels_logLevel_INFO;
//      message.msg=19;
//      message.opCode=5;
//      memcpy(message.params.uint8ArrStr.str,p,8);
//      memcpy(message.params.uint8ArrStr.uintArr,arr,8);
//
//      message.which_params = loggerMessage_uint8ArrStr_tag;

      log.timeStamp.sec = logPtr->timeStamp.sec;
      log.timeStamp.mSec = logPtr->timeStamp.mSec;
      log.logLevel = logPtr->logLevel;
      log.msg = logPtr->msg;
      log.opCode = logPtr->opCode;
      switch(log.opCode)
      {
      case 0:
          break;
      case 1:
          log.which_params = loggerMessage_oneStr_tag;
          memcpy(log.params.oneStr,logPtr->params.oneStr, PARAM_BYTES);
          break;
      case 2:
          log.which_params = loggerMessage_twoStrs_tag;
          memcpy(log.params.twoStrs.twoStrs[0],logPtr->params.twoStrs[0], PARAM_BYTES/2);
          memcpy(log.params.twoStrs.twoStrs[1],logPtr->params.twoStrs[1], PARAM_BYTES/2);
          break;
      case 3:
          log.which_params = loggerMessage_splitInts_tag;
          uint8_t i = 0;
          for (i = 0; i < PARAM_BYTES/sizeof(uint8_t); i++)
          {
              log.params.splitInts.splitInts[i] = logPtr->params.splitInts[i];
          }
          break;
      case 4:
          log.which_params = loggerMessage_oneStrOneInt_tag;
          log.params.oneStrOneInt.num = logPtr->params.oneStrOneInt.num;
          memcpy(log.params.oneStrOneInt.str, logPtr->params.oneStrOneInt.str, PARAM_BYTES-(sizeof(uint32_t)));
          break;

      case 5:
          log.which_params = loggerMessage_uint8ArrStr_tag;
          memcpy(log.params.uint8ArrStr.str, logPtr->params.uint8ArrStr.str,PARAM_BYTES/2);
          memcpy(log.params.uint8ArrStr.uintArr, logPtr->params.uint8ArrStr.uintArr,PARAM_BYTES/2);
          break;
      }


      /* Now we are ready to encode the message! */
      status = pb_encode(&stream, loggerMessage_fields, &log);

      message_length = stream.bytes_written;
      int i=0;

      CLI_cliPrintf("\r\n");
      for (i = 0; i <message_length; ++i) {
          CLI_cliPrintf("%02x",buffer[i]);
      }

      /* Then just check for any errors.. */
      if (!status)
      {
          return CRS_FAILURE;
      }


  return CRS_SUCCESS;
}

static void circBuff_clear(circularBuffer_t *circBuff)
{
//    memset(circBuff->buffer, 0, NUM_OF_LOGS * sizeof(loggerMessage_t));
    circBuff->head = 0;
    circBuff->tail = 0;
    circBuff->size = 0;
}


static CRS_retVal_t circBuff_push(circularBuffer_t *circBuff, loggerMessage_t log)
{
    if (circBuff_isFull(circBuff))  // if the circular buffer is full
    {
          return CRS_FAILURE;
    }

    uint8_t next = 0;

    next = circBuff->head + 1;  // next is where head will point to after this write.
    if (next >= NUM_OF_LOGS)
    {
        next = 0;
    }



    memcpy(&circBuff->buffer[circBuff->head], &log, sizeof(loggerMessage_t)); // Load data and then move

    circBuff->head = next;             // head to next data offset.

    circBuff->size++;
    return CRS_SUCCESS;  // return success to indicate successful push.
}

static CRS_retVal_t circBuff_pop(circularBuffer_t *circBuff, loggerMessage_t *outLog)
{
    uint8_t next = 0;

    if (circBuff->size == 0)  //we don't have any data
    {
        return CRS_FAILURE;
    }

    next = circBuff->tail + 1;  // next is where tail will point to after this read.
    if(next >= NUM_OF_LOGS)
    {
        next = 0;
    }

    *outLog = circBuff->buffer[circBuff->tail];  // Read data and then move
    circBuff->tail = next;              // tail to next offset.
    circBuff->size--;

    return CRS_SUCCESS;  // return success to indicate successful push.
}

static bool circBuff_isFull(circularBuffer_t *circBuff)
{
    return (circBuff->size == NUM_OF_LOGS);  // circular buffer is full
}


static inline CRS_retVal_t logZero(logLevels_t level, logMsgs_t msg)
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 0;
    log.timeStamp = getTimeStamp();

    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}


static inline CRS_retVal_t logOne(logLevels_t level, logMsgs_t msg, char p[PARAM_BYTES])
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 1;
    log.timeStamp = getTimeStamp();
    memcpy(log.params.oneStr,p,PARAM_BYTES);

    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}


static inline CRS_retVal_t logTwo(logLevels_t level, logMsgs_t msg, char p1[PARAM_BYTES/2], char p2[PARAM_BYTES/2])
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 2;
    log.timeStamp = getTimeStamp();
    memcpy(log.params.twoStrs[0],p1,PARAM_BYTES/2);
    memcpy(log.params.twoStrs[2],p2,PARAM_BYTES/2);


    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}


static inline CRS_retVal_t logThree(logLevels_t level, logMsgs_t msg,uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 3;
    log.timeStamp = getTimeStamp();
    log.params.splitInts[0] = p1;
    log.params.splitInts[1] = p2;
    log.params.splitInts[2] = p3;
    log.params.splitInts[3] = p4;


    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}

static inline CRS_retVal_t logFour(logLevels_t level, logMsgs_t msg,char str[PARAM_BYTES - (sizeof(uint32_t))], uint32_t num)
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 4;
    log.timeStamp = getTimeStamp();
    log.params.oneStrOneInt.num = num;
    memcpy(log.params.oneStrOneInt.str,str,PARAM_BYTES - (sizeof(uint32_t)));



    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}

static inline CRS_retVal_t logFive(logLevels_t level, logMsgs_t msg, uint8_t macAddr[PARAM_BYTES/2], char logMsg[PARAM_BYTES/2])
{
    if (isOverThrsh(level))
    {
        return CRS_FAILURE;
    }

    loggerMessage_t log = {0};
    memset(&log.params,0,PARAM_BYTES);

    log.logLevel = level;
    log.msg = msg;
    log.opCode = 5;
    log.timeStamp = getTimeStamp();
    memcpy (log.params.uint8ArrStr.uintArr,macAddr, PARAM_BYTES/2);
    memcpy (log.params.uint8ArrStr.str,logMsg, PARAM_BYTES/2);


    if (circBuff_isFull(&(gLogger.circBuff)))
    {
        if (!gIsCircular)
        {
            return CRS_FAILURE;
        }
        loggerMessage_t outLog = {0};
        circBuff_pop(&(gLogger.circBuff), &outLog);
    }

    return circBuff_push(&(gLogger.circBuff), log);
}

static inline CRS_retVal_t logGeneral(logLevels_t level, logMsgs_t msg,char *str, uint32_t num)
{
    str = &(str[(strlen(str)) - (PARAM_BYTES-(sizeof(uint32_t)))]); // last char - ".c" - last (PARAM_BYTES-(sizeof(uint32_t)) chars

    return logFour(level, msg, str, num);
}
