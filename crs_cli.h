 /*
 * crs_cli.h
 *
 *  Created on: 20 Nov 2021
 *      Author: avi
 */

#ifndef APPLICATION_CRS_CRS_CLI_H_
#define APPLICATION_CRS_CRS_CLI_H_
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "crs.h"
//#include <time.h>
#include "mac/api_mac.h"
extern volatile bool gIsUartCommInParts;
extern volatile bool gIsUartCommCommand;
extern volatile bool gIsUartCommCommandRemoteCL;
extern volatile bool gIsUartCommCliReq;
#ifdef CLI_CEU_BP
CRS_retVal_t CLI_stopOadStatClk();
#endif
CRS_retVal_t CLI_init(bool restartMsg);
CRS_retVal_t CLI_close();
CRS_retVal_t CLI_processCliUpdate(char* line, uint16_t shortAddr );
CRS_retVal_t CLI_processCliSendMsgUpdate(void);
CRS_retVal_t CLI_startREAD();
CRS_retVal_t CLI_cliPrintf( const char *_format, ...);
uint32_t CLI_convertStrUint(char *st);
char* int2hex(uint32_t num, char *outbuf);
void CLI_printHeapStatus();

typedef enum { CRS_INFO, CRS_DEBUG, CRS_WARN, CRS_ERR  } log_level;


typedef CRS_retVal_t CLI_log_handler_func_type( const log_level level, const char* file, const int line, const char* format, ... );


extern CLI_log_handler_func_type* glogHandler;

#define CRS_LOG( __LEVEL__, __FORMAT__, ... ) \
    if( glogHandler ) \
    glogHandler( __LEVEL__, __FUNCTION__ , __LINE__ , __FORMAT__, ##__VA_ARGS__ )

#endif /* APPLICATION_CRS_CRS_CLI_H_ */
