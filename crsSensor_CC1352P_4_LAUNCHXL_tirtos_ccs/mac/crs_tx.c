/*
 * crs_tx.c
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/

#include "crs_tx.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"
#include "mac/mac_util.h"



/******************************************************************************
 Constants and definitions
 *****************************************************************************/


/******************************************************************************
 Local variables
 *****************************************************************************/

void* sem;

static EasyLink_Status gStatus;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
void txDoneCb(EasyLink_Status status);



void TX_init(void * semaphore)
{
    sem = semaphore;
}


//typedef struct Frame
//{
//    uint16_t len;
//    uint16_t seqSent;
//    uint16_t seqRcv;
//    uint8_t srcAddr[8];
//    uint8_t dstAddr[8];
//    MAC_commandId_t commandId;
//    uint8_t isNeedAck;
//} MAC_crsPacket_t;

void TX_sendPacket(MAC_crsPacket_t* pkt)
{
    EasyLink_TxPacket txPacket = { { 0 }, 0, 0, { 0 } };

    uint8_t *pBuf = txPacket.payload;
    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);
    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);

    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);
    pBuf = Util_bufferUint16(pBuf, pkt->seqSent);

    memcpy(pBuf, pkt->srcAddr, 8);
    pBuf = pBuf + 8;

    memcpy(pBuf, pkt->dstAddr, 8);
    pBuf = pBuf + 8;

    *pBuf = (uint8_t) pkt->isNeedAck;
    pBuf++;
    *pBuf = (uint8_t) pkt->commandId;

//    /* Create packet with incrementing sequence number and random payload */
//    txPacket.payload[0] = (uint8_t) (seqNumber >> 8);
//    txPacket.payload[1] = (uint8_t) (seqNumber++);
//    uint8_t i;
//    for (i = 2; i < RFEASYLINKTXPAYLOAD_LENGTH; i++)
//    {
//        txPacket.payload[i] = rand();
//    }

    txPacket.len = pkt->len;

    /*
     * Address filtering is enabled by default on the Rx device with the
     * an address of 0xAA. This device must set the dstAddr accordingly.
     */
    txPacket.dstAddr[0] = CRS_PAN_ID;

    /* Add a Tx delay for > 500ms, so that the abort kicks in and brakes the burst */
//    if (EasyLink_getAbsTime(&absTime) != EasyLink_Status_Success)
//    {
//        // Problem getting absolute time
//    }
//    if (txBurstSize++ >= RFEASYLINKTX_BURST_SIZE)
//    {
//        /* Set Tx absolute time to current time + 1s */
//        txPacket.absTime = absTime + EasyLink_ms_To_RadioTime(1000);
//        txBurstSize = 0;
//    }
//    /* Else set the next packet in burst to Tx in 100ms */
//    else
//    {
//        /* Set Tx absolute time to current time + 100ms */
//        txPacket.absTime = absTime + EasyLink_ms_To_RadioTime(100);
//    }
//
//    EasyLink_transmitAsync(&txPacket, txDoneCb);
//    /* Wait 300ms for Tx to complete */
//    if (Semaphore_pend(txDoneSem, (300000 / Clock_tickPeriod)) == FALSE)
//    {
//        /* TX timed out, abort */
//        if (EasyLink_abort() == EasyLink_Status_Success)
//        {
//            /*
//             * Abort will cause the txDoneCb to be called and the txDoneSem
//             * to be released, so we must consume the txDoneSem
//             */
//            Semaphore_pend(txDoneSem, BIOS_WAIT_FOREVER);
//        }
//    }
}

void txDoneCb(EasyLink_Status status)
{
    if (status == EasyLink_Status_Success)
    {
//        /* Toggle GLED to indicate TX */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));
    }
    else if(status == EasyLink_Status_Aborted)
    {
//        /* Toggle RLED to indicate command aborted */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
    }
    else
    {
//        /* Toggle GLED and RLED to indicate error */
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_GLED,!PIN_getOutputValue(CONFIG_PIN_GLED));
//        PIN_setOutputValue(pinHandle, CONFIG_PIN_RLED,!PIN_getOutputValue(CONFIG_PIN_RLED));
    }

    gStatus = status;
    Util_setEvent(&macEvents, MAC_TASK_TX_DONE_EVT);
    Semaphore_post(sem);
}
