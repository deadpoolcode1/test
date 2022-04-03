/*
 * cp_cli.h
 *
 *  Created on: 29 Mar 2022
 *      Author: epc_4
 */

#ifndef CP_CLI_H_
#define CP_CLI_H_

void CLI_init();
void CLI_processCliUpdate();
void CLI_startREAD();
void CLI_cliPrintf( const char *_format, ...);




typedef enum { CP_CLI_INFO, CP_CLI_DEBUG, CP_CLI_WARN, CP_CLI_ERR  } log_level;


typedef void CLI_log_handler_func_type( const log_level level, const char* file, const int line, const char* format, ... );


extern CLI_log_handler_func_type* glogHandler;

#define CP_LOG( __LEVEL__, __FORMAT__, ... ) \
    if( glogHandler ) \
    glogHandler( __LEVEL__, __FUNCTION__ , __LINE__ , __FORMAT__, ##__VA_ARGS__ )

#endif /* CP_CLI_H_ */
