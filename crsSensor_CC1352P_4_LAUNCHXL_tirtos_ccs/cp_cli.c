/*
 * cp_cli.c
 *
 *  Created on: 29 Mar 2022
 *      Author: epc_4
 */

/*
 * sensor pesudo code:
 * start with cli recv-enterRx with sepcial cb that will send ack, using txsendpckat which uses a speical cb that enterRx(this cb will simply send ack if it has the same seq number) and start timer that wait 5 seconds, in the timer cb rasie event of ready_content and in the while loop in if of ready_content abortRx and send random content, and in the cb of content_send enterRx to wait for ack from collector and this rxcb will be rxIdle
 *
 *
 *
 *
 */


#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ti/drivers/dpl/SystemP.h>
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>
#include "ti_drivers_config.h"


#include "cp_cli.h"
#include "mac/macTask.h"
#include "mac/collectorLink.h"
#include "mac/crs_rx.h"
#include "mac/crs_tx.h"

//#include "mac/node.h"

#define CUI_NUM_UART_CHARS 1024

#define CLI_PROMPT "\r\nCM> "

#define CLI_DEBUG "debug"

#define CLI_SEND_PACKET "send"
#define CLI_RECIVE_PACKET "rec"
#define CLI_STOP_RECIVE_PACKET "stop"


//#define CLI_ADD_NODE "add node"
//#define CLI_LIST_NODES "list nodes"

#define CLI_ADD_COLLECTOR "add collector"
#define CLI_LIST_COLLECTOR "show collector"


//static void recivePacketCommand(char *line);
//static void sendPacketCommand(char *line);
//static void stopRecivePacketCommand(char *line);


static bool gModuleInitialized = false;

static UART_Params gUartParams;
static UART_Handle gUartHandle = NULL;
static uint8_t gUartTxBuffer[CUI_NUM_UART_CHARS] = { 0 };
static uint32_t gUartTxBufferIdx = 0;

static uint8_t gUartRxBuffer[2] = { 0 };



#define UART_WRITE_BUFF_SIZE 2000
#define TMP_BUFF_SZIE 512






static volatile uint8_t gWriteNowBuff[UART_WRITE_BUFF_SIZE];
static volatile uint8_t gWriteWaitingBuff[UART_WRITE_BUFF_SIZE];

static volatile uint32_t gWriteNowBuffIdx = 0;
static volatile uint32_t gWriteNowBuffSize = 0;
static volatile uint32_t gWriteWaitingBuffIdx = 0;


static volatile bool gIsDoneWriting = true;
static volatile bool gIsDoneFilling = false;
static volatile bool gIsNoPlaceForPrompt = false;

static volatile bool gIsAsyncCommand = false;

static bool gReadNextCommand = false;

static bool gIsNewCommand = true;

static void CLI_writeString(void *_buffer, size_t _size);
static void UartReadCallback(UART_Handle _handle, void *_buf, size_t _size);
static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size);
static void recivePacketCommand(char *line);
static void sendPacketCommand(char *line);
static void defaultTestLog( const log_level level, const char* file, const int line, const char* format, ... );
static void txDoneCbAckReceive(EasyLink_RxPacket * rxPacket, EasyLink_Status status);
static void rxDoneCbAckSend(EasyLink_RxPacket * rxPacket, EasyLink_Status status);
CLI_log_handler_func_type*  glogHandler = &defaultTestLog;


static void debugCli(char *line);
static void addNodeCommand(char *line);



void CLI_init()
{

    if (!gModuleInitialized)
    {

        {

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
                    return;
                }
                else
                {
                    UART_read(gUartHandle, gUartRxBuffer, 1);
                }
            }

        }

        gModuleInitialized = true;

//        CLI_writeString(CLI_PROMPT, sizeof(CLI_PROMPT));
        return;
    }

    return;
}

static void defaultTestLog( const log_level level, const char* file, const int line, const char* format, ... )
{
    if (strlen(format) >= 512)
       {
           return ;
       }
       char printBuff[512] = { 0 };
       if (format == NULL)
       {
           return ;
       }
                va_list args;

                switch( level )
                {
                case CP_CLI_INFO:
                    CLI_cliPrintf( "\r\n[INFO   ] %s:%d : ", file, line);
                        break;
                case CP_CLI_DEBUG:
                   return;
                   CLI_cliPrintf( "\r\n[DEBUG  ] %s:%d : ", file, line);
                        break;
                case CP_CLI_ERR:
//                  return;

                  CLI_cliPrintf( "\r\n[ERROR  ] %s:%d : ", file, line);
                        break;
                case CP_CLI_WARN:
//                   return;

                   CLI_cliPrintf( "\r\n[WARNING] %s:%d : ", file, line);
                        break;
                }

                va_start(args, format);
                SystemP_vsnprintf(printBuff, sizeof(printBuff), format, args);
                va_end(args);
                CLI_cliPrintf(printBuff);
}

void CLI_processCliUpdate()
{

    if (!gModuleInitialized)
    {
        return ;
    }
    uint8_t* line = gUartTxBuffer;
            gUartTxBuffer[gUartTxBufferIdx - 1] = 0;


    const char badInputMsg[] = "\r\nINVALID INPUT";

    bool inputBad = true;

    bool is_async_command = false;



    if (memcmp(CLI_DEBUG, line, sizeof(CLI_DEBUG)-1) == 0)
       {

        debugCli(line);

           inputBad = false;

       }

//        if (memcmp(CLI_ADD_NODE, line, sizeof(CLI_ADD_NODE)-1) == 0)
//               {
//
//            addNodeCommand(line);
//
//                   inputBad = false;
//                   CLI_startREAD();
//               }
//
//
//        if (memcmp(CLI_LIST_NODES, line, sizeof(CLI_LIST_NODES)-1) == 0)
//               {
//
////            Node_listNodes();
////            CLI_cliPrintf("\r\nStatus:0x0");
//                   inputBad = false;
//                   CLI_startREAD();
//               }




    if (memcmp(CLI_ADD_COLLECTOR, line, sizeof(CLI_ADD_COLLECTOR)-1) == 0)
           {

        addNodeCommand(line);

               inputBad = false;
               CLI_startREAD();
           }


    if (memcmp(CLI_LIST_COLLECTOR, line, sizeof(CLI_LIST_COLLECTOR)-1) == 0)
           {

       CollectorLink_printCollector();
        CLI_cliPrintf("\r\nStatus:0x0");
               inputBad = false;
               CLI_startREAD();
           }


    if (memcmp(CLI_SEND_PACKET, line, sizeof(CLI_SEND_PACKET)-1) == 0)
           {

        sendPacketCommand(line);

               inputBad = false;
               CLI_cliPrintf("\r\nStatus:0x0");

           }

    if (memcmp(CLI_RECIVE_PACKET, line, sizeof(CLI_RECIVE_PACKET)-1) == 0)
           {

        recivePacketCommand(line);

               inputBad = false;
               CLI_cliPrintf("\r\nStatus:0x0");

           }
//    if (memcmp(CLI_STOP_RECIVE_PACKET, line, sizeof(CLI_STOP_RECIVE_PACKET)-1) == 0)
//              {
//
//        stopRecivePacketCommand(line);
//
//                  inputBad = false;
//
//              }
//
//




    if (inputBad && strlen(line) > 0)
    {
        CLI_cliPrintf(badInputMsg);
        CLI_startREAD();
        return;
    }
    else if (inputBad)
    {
        CLI_startREAD();
        return;
    }

    return ;
}

static void debugCli(char *line)
{
//    RfEasyLink_sendPacket();

        Node_nodeInfo_t node;
        uint8_t mac[8]={0xa,0xa,0xa,0xa,0xa,0xa,0xa,0xa};
        memcpy(node.mac,mac,8);
        CollectorLink_updateCollector(&node);
        CollectorLink_printCollector();
        CollectorLink_setTimeout((Clock_FuncPtr)funcTest,110000/Clock_tickPeriod);
        CollectorLink_startTimer();
            CLI_startREAD();
}


static void addNodeCommand(char *line)
{
    Node_nodeInfo_t node={0};
    uint8_t mac[MAC_SIZE]={0};
    char tempBuff[TMP_BUFF_SZIE]={0};
    memcpy(tempBuff,line,strlen(line));
    const char s[2] = " ";
       char *token;
       /* get the first token */
       token = strtok(tempBuff, s);//add
       token = strtok(NULL, s);//node
       token = strtok(NULL, s);//macAddr
       if (strlen(token)!=(MAC_SIZE+2)) {
           CLI_cliPrintf("\r\nStatus:0x1");
           return;
    }
       char tmpMacStrAddr[MAC_SIZE]={0};
       memcpy(tmpMacStrAddr,&token[2],MAC_SIZE);
       int i=0;
       char* ptr=tmpMacStrAddr;
       char tmp[2]={0};
       //0xaa aa aa aa
       for (i = 0; i <MAC_SIZE; i++) {
           *tmp=*ptr;
           node.mac[i]=(uint8_t) strtoul(tmp, NULL, 16);
           ptr++;
    }

//       Node_addNode(&node);
       CollectorLink_updateCollector(&node);

       CLI_cliPrintf("\r\nStatus:0x0");




}



//
//static void sendPacketCommand(char *line)
//{
//    RfEasyLink_sendPacket();
//}
//
//static void recivePacketCommand(char *line)
//{
//    RfEasyLink_recivePacket();
//    CLI_startREAD();
//
//}
//static void stopRecivePacketCommand(char *line)
//{
//    RfEasyLink_stopRecivePacket();
//}

void CLI_startREAD()
{

//


        CLI_writeString(CLI_PROMPT, strlen(CLI_PROMPT));

    gIsNewCommand = true;
    memset(gUartTxBuffer, 0, CUI_NUM_UART_CHARS - 1);

    gUartTxBufferIdx = 0;
    if (gReadNextCommand == false)
    {
        gReadNextCommand = true;
//        UART_read(gUartHandle, gUartRxBuffer, 1);

    }

    return ;

}

void CLI_cliPrintf(const char *_format, ...)
{
    if (strlen(_format) >= 1024)
    {
        return ;
    }
    char printBuff[1024] = { 0 };
    if (_format == NULL)
    {
        return ;
    }
    va_list args;

    va_start(args, _format);
    SystemP_vsnprintf(printBuff, sizeof(printBuff), _format, args);
    va_end(args);


    CLI_writeString(printBuff, strlen(printBuff));
    return ;
}


static void UartWriteCallback(UART_Handle _handle, void *_buf, size_t _size)
{

    if (_size != gWriteNowBuffSize)
    {
        gWriteNowBuffIdx = gWriteNowBuffIdx + _size;
        gWriteNowBuffSize = gWriteNowBuffSize - _size;
        UART_write(gUartHandle, &gWriteNowBuff[gWriteNowBuffIdx],
                   gWriteNowBuffSize);

        return;
    }
    gIsDoneWriting = true;

    if (gIsDoneFilling)
    {

        memset(gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy(gWriteNowBuff, gWriteWaitingBuff, gWriteWaitingBuffIdx);
        gWriteNowBuffSize = gWriteWaitingBuffIdx;
        memset(gWriteWaitingBuff, 0, gWriteWaitingBuffIdx + 1);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
        gIsDoneFilling = false;
        UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);

        return ;
    }

    if (gIsNoPlaceForPrompt == true)
    {
        gIsNoPlaceForPrompt = false;
        gIsDoneWriting = false;

        memset(gWriteNowBuff, 0, strlen(CLI_PROMPT) + 1);
        memcpy(gWriteNowBuff, CLI_PROMPT, strlen(CLI_PROMPT));
        gWriteNowBuffSize = strlen(CLI_PROMPT);
        gWriteNowBuffIdx = 0;

        UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);
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
        if (gUartTxBufferIdx == CUI_NUM_UART_CHARS-1)
        {
            if ( ((uint8_t*) _buf)[0] != '\r')
            {
                return;
            }

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
            Mac_cliUpdate();


        }

    }
    else
    {
        // Handle error or call to UART_readCancel()
        UART_readCancel(gUartHandle);
    }
}

static void CLI_writeString(void *_buffer, size_t _size)
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
        return ;
    }

    if (_buffer == NULL)
    {
        return ;
    }

    if (gIsDoneWriting == true)
    {
        if (gWriteWaitingBuffIdx + _size < UART_WRITE_BUFF_SIZE)
        {
            memcpy(&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
            gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;
        }

        gWriteNowBuffSize = gWriteWaitingBuffIdx;

        memset(gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
        memcpy(gWriteNowBuff, gWriteWaitingBuff, gWriteWaitingBuffIdx);
        memset(gWriteWaitingBuff, 0, gWriteWaitingBuffIdx);
        gWriteWaitingBuffIdx = 0;
        gWriteNowBuffIdx = 0;
        gIsDoneWriting = false;
//        gWriteNowBuffTotalSize = gWriteNowBuffSize;
        UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);

        return ;
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
            return ;
        }
        bool flag = false;
        while (gIsDoneFilling == true)
        {
            flag  = true;
        }



        memcpy(&gWriteWaitingBuff[gWriteWaitingBuffIdx], _buffer, _size);
        gWriteWaitingBuffIdx = gWriteWaitingBuffIdx + _size;

        if (_size >= strlen(CLI_PROMPT)
                && memcmp(CLI_PROMPT, _buffer, strlen(CLI_PROMPT)) == 0)
        {
            gIsDoneFilling = true;
        }
        char *cliPrompt = CLI_PROMPT;
        if (_size >= strlen(CLI_PROMPT)
                && memcmp(cliPrompt + 2, _buffer + 1, strlen(CLI_PROMPT) - 2)
                        == 0)
        {
            gIsDoneFilling = true;
        }


        if (gIsDoneWriting && gIsDoneFilling)
                {



                    gWriteNowBuffSize = gWriteWaitingBuffIdx;

                    memset(gWriteNowBuff, 0, gWriteWaitingBuffIdx + 1);
                    memcpy(gWriteNowBuff, gWriteWaitingBuff, gWriteWaitingBuffIdx);
                    memset(gWriteWaitingBuff, 0, gWriteWaitingBuffIdx);
                    gWriteWaitingBuffIdx = 0;
                    gWriteNowBuffIdx = 0;
                    gIsDoneWriting = false;
//                    gWriteNowBuffTotalSize = gWriteNowBuffSize;
                    UART_write(gUartHandle, gWriteNowBuff, gWriteNowBuffSize);

                    return ;
                }

        return ;
    }

    return ;
}


static void sendPacketCommand(char *line)
{
    MAC_crsPacket_t pkt = {0};
    pkt.commandId = MAC_COMMAND_DATA;

    uint8_t tmp[8] = {0xcf, 0x26, 0xf4, 0x14, 0x4b, 0x12, 0x00, 0x00};
    memcpy(pkt.dstAddr, tmp, 8);

    uint8_t tmp2[8] = { 0xfc, 0x62, 0x4f, 0x41, 0xb4, 0x21, 0x00 };
    memcpy(pkt.srcAddr, tmp2, 8);

    pkt.seqSent = 10;
    pkt.seqRcv = 50;

    pkt.isNeedAck = 1;

    int i = 0;

    for (i = 0; i < 50; i++)
    {
        pkt.payload[i] = i;
    }
    pkt.len = 50;

//     pkt.dstAddr

    TX_sendPacket(&pkt, tmp,txDoneCbAckListen);
    CLI_startREAD();

}


static void recivePacketCommand(char *line)
{
    uint8_t tmp[8] = {0xcf, 0x26, 0xf4, 0x14, 0x4b, 0x12, 0x00, 0x00};
    RX_enterRx(tmp, rxDoneCbAckSend);
    CLI_startREAD();

}

