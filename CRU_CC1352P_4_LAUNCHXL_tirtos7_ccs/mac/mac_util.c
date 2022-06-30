/*
 * mac_util.c
 *
 *  Created on: 3 Apr 2022
 *      Author: epc_4
 */

#include "mac_util.h"




/*!
 * @brief      Build a uint16_t out of 2 uint8_t variables
 *
 * @param      loByte - low byte
 * @param      hiByte - high byte
 *
 * @return     combined uint16_t
 */
uint16_t Util_buildUint16(uint8_t loByte, uint8_t hiByte)
{
    uint16_t wd = ((uint16_t)hiByte << 8) | loByte;
    return wd;

}

/*!
 * @brief      Build a uint32_t out of 4 uint8_t variables
 *
 * @param      byte0 - byte - 0
 * @param      byte1 - byte - 1
 * @param      byte2 - byte - 2
 * @param      byte3 - byte - 3
 *
 * @return     combined uint32_t
 */
uint32_t Util_buildUint32(uint8_t byte0, uint8_t byte1, uint8_t byte2,
                                 uint8_t byte3)
{
//    uint32_t wd = ((uint32_t) byte1 << 8) | byte0;
//    return wd;
}

/*!
 * @brief      Break and buffer a uint16 value - LSB first
 *
 * @param      pBuf - ptr to next available buffer location
 * @param      val  - 16-bit value to break/buffer
 *
 * @return     pBuf - ptr to next available buffer location
 */
uint8_t *Util_bufferUint16(uint8_t *pBuf, uint16_t val)
{
    pBuf[0] = (uint8_t)(val & 0x00ff);
    pBuf[1] = (uint8_t)(val >> 8);
    pBuf = pBuf + 2;
    return pBuf;

}

/*!
 * @brief      Break and buffer a uint32 value - LSB first
 *
 * @param      pBuf - ptr to next available buffer location
 * @param      val  - 32-bit value to break/buffer
 *
 * @return     pBuf - ptr to next available buffer location
 */
uint8_t *Util_bufferUint32(uint8_t *pBuf, uint32_t val);

/*!
 * @brief       Utility function to copy the extended address
 *
 * @param       pSrcAddr - pointer to source from which to be copied
 * @param       pDstAddr - pointer to destination to copy to
 */
void Util_copyExtAddr(void *pSrcAddr, void *pDstAddr);

void Util_clearEvent(uint16_t *pEvent, uint16_t event)
{
    *pEvent =  *(pEvent) & (~event);
}
void Util_setEvent(uint16_t *pEvent, uint16_t event)
{
    *pEvent = (*pEvent) | event;
}
