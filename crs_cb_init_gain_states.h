/*
 * crs_cb_init_gain_states.h
 *
 *  Created on: 29 ????? 2022
 *      Author: cellium
 */

#ifndef APPLICATION_CRS_CRS_CB_INIT_GAIN_STATES_H_
#define APPLICATION_CRS_CRS_CB_INIT_GAIN_STATES_H_

#include <ti/drivers/NVS.h>
#include "crs.h"


CRS_retVal_t CIGS_init();
CRS_retVal_t CIGS_read(char *vars, char *returnedVars);
CRS_retVal_t CIGS_write(char *vars);
CRS_retVal_t CIGS_delete(char *vars);
CRS_retVal_t CIGS_format();
CRS_retVal_t CIGS_restore();



#endif /* APPLICATION_CRS_CRS_CB_INIT_GAIN_STATES_H_ */
