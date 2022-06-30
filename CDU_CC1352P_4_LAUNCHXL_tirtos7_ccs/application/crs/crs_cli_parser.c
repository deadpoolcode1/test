/*
 * crs_cli_parser.c
 *
 *  Created on: 27 Jun 2022
 *      Author: epc_4
 */

/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_cli_parser.h"
#include "crs.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/

#define CLI_CRS_FPGA_OPEN "fpga open"
#define CLI_CRS_FPGA_CLOSE "fpga close"

#define CLI_CRS_FPGA_WRITELINES "fpga writelines"
#define CLI_CRS_FPGA_READLINES "fpga readlines"


#define COMMAND_LEN 30
//
//#define AVI_SHIT(name,pFn,cmpFn) \
//    static  CRS_retVal_t pFn(uint8_t * line); \
//    static bool cmpFn(uint8_t *command,uint8_t* line) {\
//        return memcmp(command, line, sizeof(name) - 1)\
//    }


#define CRS_COMMAND(mCommandName, mHandlerFnP, mIsLocal) \
        {.name = mCommandName, .pFn=mHandlerFnP, .isLocal=mIsLocal}

typedef CRS_retVal_t (*Parser__cbFn_t)(uint8_t* line);

typedef struct _crs_parser
{
    uint8_t* name;
    Parser__cbFn_t pFn;
    bool isLocal;
} CrsParser_command_t;


/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/

static CRS_retVal_t fpgaOpenParsing(uint8_t *line);
static CRS_retVal_t fpgaCloseParsing(uint8_t *line);


/******************************************************************************
 Local variables
 *****************************************************************************/

static CrsParser_command_t gAllCommands[] = {
                                             CRS_COMMAND(CLI_CRS_FPGA_OPEN, fpgaOpenParsing, false),
};



/******************************************************************************
 * Public CLI APIs
 *****************************************************************************/

static void parseCommand(uint8_t *line)
{

}

static CRS_retVal_t fpgaOpenParsing(uint8_t *line)
{

}

static CRS_retVal_t fpgaCloseParsing(uint8_t *line)
{

}

static CRS_retVal_t fpgaWriteLinesParsing(uint8_t *line)
{

}
