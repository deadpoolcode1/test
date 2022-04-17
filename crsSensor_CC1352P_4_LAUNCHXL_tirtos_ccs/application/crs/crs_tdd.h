/*
 * crs_tdd.h
 *
 *  Created on: 24 Feb 2022
 *      Author: epc_4
 */

#ifndef APPLICATION_CRS_CRS_TDD_H_
#define APPLICATION_CRS_CRS_TDD_H_
#include <stdbool.h>
#include <stdint.h>
#include "crs.h"
#include "string.h"

typedef struct {
    uint32_t arg0;
    uint32_t arg1;
    char*    arg3;
} TDD_cbArgs_t;

typedef void (*TDD_cbFn_t)(const TDD_cbArgs_t _cbArgs);

CRS_retVal_t Tdd_initSem(void *sem);
CRS_retVal_t Tdd_init(TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_close();
CRS_retVal_t Tdd_isOpen();
CRS_retVal_t Tdd_isLocked();
CRS_retVal_t Tdd_printStatus(TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setSyncMode(bool mode, TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setSCS(uint8_t scs, TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setFrameFormat(uint8_t frame, TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setAllocationMode(uint8_t alloc, TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setHolTime(uint8_t min, uint8_t sec, TDD_cbFn_t _cbFn);

CRS_retVal_t Tdd_setTtg(int8_t * ttg_vals, TDD_cbFn_t _cbFn);
CRS_retVal_t Tdd_setRtg(int8_t * rtg_vals, TDD_cbFn_t _cbFn);

typedef struct {
    uint16_t period;
    uint16_t dl1;
} TDD_tdArgs_t;

TDD_tdArgs_t Tdd_getTdArgs();

void Tdd_process(void);

#endif /* APPLICATION_CRS_CRS_TDD_H_ */
