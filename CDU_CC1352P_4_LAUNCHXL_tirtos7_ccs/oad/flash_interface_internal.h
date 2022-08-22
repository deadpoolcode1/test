/*
 * flash_interface_internal.h
 *
 *  Created on: 22 ????? 2022
 *      Author: cellium
 */

#ifndef OAD_FLASH_INTERFACE_INTERNAL_H_
#define OAD_FLASH_INTERFACE_INTERNAL_H_
#include <stdbool.h>
#include <string.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/flash.h)
#include DeviceFamily_constructPath(driverlib/vims.h)
#include "common/cc26xx/flash_interface/flash_interface.h"

#include "flash_interface_internal.h"
uint8_t readInternalbimFlash(uint_least32_t addr, uint8_t *pBuf, size_t len);
uint8_t readInternalbimFlashPg(uint8_t page, uint32_t offset, uint8_t *pBuf, uint16_t len);
uint8_t writeInternalbimFlash(uint_least32_t addr, uint8_t *pBuf, size_t len);

uint8_t writeInternalbimFlashPg(uint8_t page, uint32_t offset, uint8_t *pBuf, uint16_t len);
uint8_t eraseInternalbimFlashPg(uint8_t page);

#endif /* OAD_FLASH_INTERFACE_INTERNAL_H_ */
