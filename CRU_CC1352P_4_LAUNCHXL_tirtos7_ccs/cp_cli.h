/*
 * cp_cli.h
 *
 *  Created on: 29 Mar 2022
 *      Author: epc_4
 */

#ifndef CP_CLI_H_
#define CP_CLI_H_
/******************************************************************************
 Function Prototypes
 *****************************************************************************/
void CP_CLI_init();
void CP_CLI_processCliUpdate();
void CP_CLI_startREAD();
void CP_CLI_cliPrintf( const char *_format, ...);



/******************************************************************************
 Constants and definitions
 *****************************************************************************/
typedef enum { CP_CLI_INFO, CP_CLI_DEBUG, CP_CLI_WARN, CP_CLI_ERR  } cp_log_level;


typedef void CP_CLI_log_handler_func_type( const cp_log_level level, const char* file, const int line, const char* format, ... );

/******************************************************************************
 Global variables
 *****************************************************************************/
extern CP_CLI_log_handler_func_type* gCpLogHandler;

#define CP_LOG( __LEVEL__, __FORMAT__, ... ) \
    if( gCpLogHandler ) \
    gCpLogHandler( __LEVEL__, __FUNCTION__ , __LINE__ , __FORMAT__, ##__VA_ARGS__ )

#endif /* CP_CLI_H_ */
