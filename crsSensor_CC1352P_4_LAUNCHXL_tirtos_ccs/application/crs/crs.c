/*
 * crs.c
 *
 *  Created on: 23 Jan 2022
 *      Author: avi
 */
#include "crs_nvs.h"

#include "crs.h"
#include "crs_cli.h"
#include "crs_thresholds.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ti/drivers/Temperature.h>




void CRS_init()
{

}





void* CRS_malloc(uint16_t size)
{
    return malloc(size);

}

/*!
 Csf implementation for memory de-allocation

 Public function defined in csf.h
 */
void CRS_free(void *ptr)
{
//    CRS_LOG(CRS_DEBUG, "FREEE");
    if (ptr != NULL)
    {
        free(ptr);

    }
}

