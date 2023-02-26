/*
 * crs_convert_snapshot.c
 *
 *  Created on: 2 Jan 2022
 *      Author: epc_4
 */
/******************************************************************************
 Includes
 *****************************************************************************/
#include "crs_convert_snapshot.h"
/******************************************************************************
 Local variables
 *****************************************************************************/
static uint32_t gRfChain = 1;
static int gLUT_num = 1;
static bool gIsRfChainChanged = false;
static bool gIsFirstLine = true;

/******************************************************************************
 Local Function Prototypes
 *****************************************************************************/
static CRS_retVal_t changeRfChain(uint32_t addrVal);
static CRS_retVal_t checkRfChain(uint32_t addrVal);

/******************************************************************************
 Public Functions
 *****************************************************************************/
CRS_retVal_t Convert_readLineRf(char *line,
                                char respLine[9][CRS_NVS_LINE_BYTES],
                                uint32_t LUT_line, CRS_chipMode_t chipMode)
{
//    CLI_cliPrintf("\r\nin convertreadliner%s\r\n",line);
    char readLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(readLine, line, CRS_NVS_LINE_BYTES);
    memset(respLine[0], 0, CRS_NVS_LINE_BYTES);
    if (strlen(line) <= 2 || memcmp(line, "//", 2) == 0)
    {
        return (CRS_NEXT_LINE);
    }
    if ((strstr(readLine, "16b'")) || (strstr(readLine, "32b'")))
    {
        memcpy(respLine[0], readLine, CRS_NVS_LINE_BYTES);
        return (CRS_SUCCESS);
    }

    const char s[2] = " ";
    char *token;
    /* get the first token */
    char tokenMode[2];
    char tokenAddr[9] = { 0 };
    char tokenValue[10] = { 0 };
    char baseAddr[9] = { 0 };
    memset(baseAddr, '0', 8);

    char tmpReadLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(tmpReadLine, readLine, CRS_NVS_LINE_BYTES);
    token = strtok((tmpReadLine), s); //w or r
    memcpy(tokenMode, token, 2);
    token = strtok(NULL, s); //addr
    memcpy(tokenAddr, &token[2], 8);
    memcpy(baseAddr, &token[2], 5);

    uint32_t addrVal; //= CLI_convertStrUint(&tokenAddr[0]);
    sscanf(&tokenAddr[0], "%x", &addrVal);
    uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
    addrVal = ((addrVal - baseAddrVal) / 4);
    if (gIsFirstLine == false)
    {
        checkRfChain(addrVal);
    }
    else
    {
        changeRfChain(addrVal);
    }
    char addrValHex[10] = { 0 };
    int2hex(addrVal, addrValHex);
    if (*(tokenMode) == 'w')
    {
        int rspLineIdx = 0;
        if (gIsRfChainChanged == true)
        {
//            CLI_cliPrintf("\r\ngIsRfChainChanged == true");

            gIsRfChainChanged = false;
            char LUT[20] = { 0 };
            if (chipMode == MODE_NATIVE)
            {
                memcpy(LUT, "wr 0x50 0x43", strlen("wr 0x50 0x43"));
            }
            else if (chipMode == MODE_SPI_SLAVE)
            {
                memcpy(LUT, "wr 0x60 0x43", strlen("wr 0x60 0x43"));

            }
            uint32_t i = 0;
            i |= (0xf & gLUT_num);
            i |= (0xf80 & (LUT_line << 8));
            char i_str[20] = { 0 };
            int2hex(i, i_str);
            int len_i_str = strlen(i_str);
            len_i_str = 4 - len_i_str;
            int k = 0;
            for (k = 0; k < len_i_str; ++k)
            {
                strcat(LUT, "0");
            }
            //            CLI_cliPrintf("\r\nLUT1: %s", LUT);

            strcat(LUT, i_str);
            //            CLI_cliPrintf("\r\nLUT3: %s", LUT);

            memcpy(respLine[rspLineIdx], LUT, strlen(LUT)); //LUT number and line number
            //apply
            rspLineIdx++;

            char lines[2][25] = { 0 };
            memset(lines[0], 0, 24);
            memset(lines[1], 0, 24);

            if (chipMode == MODE_NATIVE)
            {
                memcpy(lines[0], "wr 0x50 0x000001",
                       strlen("wr 0x50 0x000001"));
                memcpy(lines[1], "wr 0x50 0x000000",
                       strlen("wr 0x50 0x000000"));

            }
            else if (chipMode == MODE_SPI_SLAVE)
            {
                memcpy(lines[0], "wr 0x60 0x000001",
                       strlen("wr 0x60 0x000001"));
                memcpy(lines[1], "wr 0x60 0x000000",
                       strlen("wr 0x60 0x000000"));

            }

            memcpy(respLine[rspLineIdx], lines[0], strlen(lines[0]));
            rspLineIdx++;

            memcpy(respLine[rspLineIdx], lines[1], strlen(lines[1]));
            rspLineIdx++;
        }

        if (gIsFirstLine == false)
        {
            changeRfChain(addrVal);
        }
        if (((addrVal < 0x8 || addrVal > 0x27)
                && (addrVal < 0x3F || addrVal > 0x42)))
        {
            token = strtok(NULL, s); // value

            memcpy(tokenValue, &token[2], 4);
            sprintf(respLine[rspLineIdx], "wr 0x50 0x0%x%s", addrVal,
                    tokenValue);
            rspLineIdx++;
//            if (gIsFirstLine == true)
//            {
//                gIsFirstLine = false;
//            }
//            CLI_cliPrintf("\r\nCRS_NEXT_LINE");

            return (CRS_NEXT_LINE);
        }

        if (addrVal == 0x25)
        {
//            if (gIsFirstLine == true)
//            {
//                gIsFirstLine = false;
//            }
//            CLI_cliPrintf("\r\nCRS_NEXT_LINE");

            return (CRS_NEXT_LINE);
        }
        if ((addrVal >= 0x8 && addrVal <= 0x27)) //if its a global register-dont convert its addr to an rf addr
        {

            switch (addrVal % 8)
            {
            case 0:
                addrVal = 0x44;
                break;
            case 1:
                addrVal = 0x45;
                break;
            case 2:
                addrVal = 0x46;
                break;
            case 3:
                addrVal = 0x47;
                break;
            case 4:
                addrVal = 0x48;
                break;
            case 5:
                addrVal = 0x49;
                break;
            case 6:
                addrVal = 0x4a;
                break;
            case 7:
                addrVal = 0x4b;
                break;
            }
        }
        int2hex(addrVal, addrValHex);
        token = strtok(NULL, s); // value

        memcpy(tokenValue, &token[2], 4);
        char wr_converted[17] = { 0 };
        if (chipMode == MODE_NATIVE)
        {
            memcpy(wr_converted, "wr 0x50 0x", strlen("wr 0x50 0x"));
        }
        else if (chipMode == MODE_SPI_SLAVE)
        {
            memcpy(wr_converted, "wr 0x60 0x", strlen("wr 0x60 0x"));

        }
        //wr 0x50 0xaddrValHex_tokenValue
        strcat(wr_converted, addrValHex);
        int len_i_str = strlen(tokenValue);
        len_i_str = 4 - len_i_str;
        int k = 0;
        for (k = 0; k < len_i_str; ++k)
        {
            strcat(wr_converted, "0");
        }
        strcat(wr_converted, tokenValue);
        memcpy(respLine[rspLineIdx], wr_converted, 17);
        rspLineIdx++;

        if (chipMode == MODE_SPI_SLAVE)
        {

            memcpy(respLine[rspLineIdx], "wr 0x61 0x0", strlen("wr 0x61 0x0"));
            rspLineIdx++;
//LUT number and line number
        }

    }
    else if (*(tokenMode) == 'r')
    {
        char rd_converted[2][17] = { 0 };
        memset(rd_converted[0], 0, 16);
        memset(rd_converted[1], 0, 16);
        if (chipMode == MODE_NATIVE)
        {
            memcpy(rd_converted[0], "wr 0x51 0x", strlen("wr 0x51 0x"));
            memcpy(rd_converted[1], "rd 0x51", strlen("rd 0x51"));

        }
        else if (chipMode == MODE_SPI_SLAVE)
        {
            memcpy(rd_converted[0], "wr 0x62 0x", strlen("wr 0x62 0x"));
            memcpy(rd_converted[1], "rd 0x62", strlen("rd 0x62"));
        }
        strcat(rd_converted[0], addrValHex);
        strcat(rd_converted[0], "0000");
        memcpy(respLine[0], rd_converted[0], 17);
        memcpy(respLine[1], rd_converted[1], 17);
        //        CLI_cliPrintf("%s \n%s \n", rd_converted[0], rd_converted[1]);

    }
    else if (memcmp(tokenMode, "ew", 2) == 0)
    {
//        Nvs_readLine(filename, lineNumber, readLine);
        readLine[0] = 'w';
        readLine[1] = 'r';
        memcpy(respLine[0], readLine, 17);
    }
    else if (memcmp(tokenMode, "er", 2) == 0)
    {
//        Nvs_readLine(filename, lineNumber, readLine);
        readLine[0] = 'r';
        readLine[1] = 'd';
        memcpy(respLine[0], readLine, 17);
    }

    return (CRS_SUCCESS);

}

CRS_retVal_t Convert_readLineDig(char *line,
                                 char respLine[9][CRS_NVS_LINE_BYTES],
                                 CRS_chipMode_t chipMode)
{

    char readLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(readLine, line, CRS_NVS_LINE_BYTES);
    memset(respLine[0], 0, CRS_NVS_LINE_BYTES);
    if (strlen(line) <= 2 || memcmp(line, "//", 2) == 0)
    {
        return (CRS_NEXT_LINE);
    }
    if (memcmp(readLine, "FAILURE", 7) == 0)
    {
        return (CRS_FAILURE);
    }
    const char s[2] = " ";
    char *token;
    /* get the first token */
    char tokenMode[2];
    char tokenAddr[9] = { 0 };
    char tokenValue[5] = { 0 };
    char baseAddr[9] = { 0 };
    memset(baseAddr, '0', 8);
    token = strtok((readLine), s); //w or r
    memcpy(tokenMode, token, 2);
    token = strtok(NULL, s); //addr
    memcpy(tokenAddr, &token[2], strlen(&token[2]));
    memcpy(baseAddr, &token[2], 5);

    uint32_t addrVal = CLI_convertStrUint(&tokenAddr[0]);
    uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
    addrVal = ((addrVal - baseAddrVal) / 4);

    char addrValHex[3] = { 0 };
    int2hex(addrVal, addrValHex);
    if (*(tokenMode) == 'w')
    {
        if ((addrVal < 0x8 || addrVal > 0x4b))
        {
            token = strtok(NULL, s); // value

            memcpy(tokenValue, &token[2], 4);
            sprintf(respLine[0], "wr 0x50 0x0%x%s", addrVal, tokenValue);

            return (CRS_NEXT_LINE);
        }
        token = strtok(NULL, s); // value
        memcpy(tokenValue, &token[2], 4);
        char wr_converted[17] = { 0 };
        if (chipMode == MODE_NATIVE)
        {
            memcpy(wr_converted, "wr 0x50 0x", strlen("wr 0x50 0x"));
        }
        else if (chipMode == MODE_SPI_SLAVE)
        {
            memcpy(wr_converted, "wr 0x60 0x", strlen("wr 0x60 0x"));
            memcpy(respLine[1], "wr 0x61 0x0", strlen("wr 0x61 0x0"));
        }
        //wr 0x50 0xaddrValHex_tokenValue
        strcat(wr_converted, addrValHex);
        strcat(wr_converted, tokenValue);
        memcpy(respLine[0], wr_converted, 17);
    }
    else if (*(tokenMode) == 'r')
    {
        char rd_converted[2][17] = { 0 };
        memset(rd_converted[0], 0, 16);
        memset(rd_converted[1], 0, 16);

        if (chipMode == MODE_NATIVE)
        {
            memcpy(rd_converted[0], "wr 0x51 0x", strlen("wr 0x51 0x"));
            memcpy(rd_converted[1], "rd 0x51", strlen("rd 0x51"));
        }
        else if (chipMode == MODE_SPI_SLAVE)
        {
            memcpy(rd_converted[0], "wr 0x62 0x", strlen("wr 0x62 0x"));
            memcpy(rd_converted[1], "rd 0x62", strlen("rd 0x62"));
        }
        strcat(rd_converted[0], addrValHex);
        strcat(rd_converted[0], "0000");
        memcpy(respLine[0], rd_converted[0], 17);
        memcpy(respLine[1], rd_converted[1], 17);
        //        CLI_cliPrintf("%s \n%s \n", rd_converted[0], rd_converted[1]);

    }
    else if (memcmp(tokenMode, "ew", 2) == 0)
    {
//           Nvs_readLine(filename, lineNumber, readLine);
        memcpy(readLine, line, CRS_NVS_LINE_BYTES);

        readLine[0] = 'w';
        readLine[1] = 'r';
        memcpy(respLine[0], readLine, 17);
    }
    else if (memcmp(tokenMode, "er", 2) == 0)
    {
//           Nvs_readLine(filename, lineNumber, readLine);
        memcpy(readLine, line, CRS_NVS_LINE_BYTES);

        readLine[0] = 'r';
        readLine[1] = 'd';
        memcpy(respLine[0], readLine, 17);
    }
    return (CRS_SUCCESS);

}

CRS_retVal_t Convert_readLineFpga(char *line,
                                  char respLine[9][CRS_NVS_LINE_BYTES])
{
    char readLine[CRS_NVS_LINE_BYTES + 1] = { 0 };
    memcpy(readLine, line, CRS_NVS_LINE_BYTES);
    memset(respLine[0], 0, CRS_NVS_LINE_BYTES);

    if (memcmp(readLine, "ew", 2) == 0)
    {
        //           Nvs_readLine(filename, lineNumber, readLine);
        readLine[0] = 'w';
        readLine[1] = 'r';
        memcpy(respLine[0], readLine, 20);
    }
    else if (memcmp(readLine, "er", 2) == 0)
    {
        //           Nvs_readLine(filename, lineNumber, readLine);
        readLine[0] = 'r';
        readLine[1] = 'd';
        memcpy(respLine[0], readLine, 20);
    }
    else
    {
        memcpy(respLine[0], line, 20);
    }
    return (CRS_SUCCESS);

}

CRS_retVal_t Convert_convertStar(char *starLine, char *rdResp, char *parsedLine)
{
    char tmp[CRS_NVS_LINE_BYTES] = { 0 };
    memcpy(tmp, starLine, CRS_NVS_LINE_BYTES);
    char result[CRS_NVS_LINE_BYTES] = { 0 };

    const char s[3] = " ";
    char *token;
    token = strtok((tmp), s); //w
    char mode[3] = { 0 };
    memcpy(mode, token, 2);
    if (*mode == 'e')
    {
        result[0] = 'e';
    }
    strcat(result, "w");
    strcat(result, " ");
    token = strtok(NULL, s); //addr
    strcat(result, token);
    strcat(result, " 0x");
    token = strtok(NULL, s); //val
    char tokenValue[CRS_NVS_LINE_BYTES] = { 0 };
    memcpy(tokenValue, &token[4], CRS_NVS_LINE_BYTES);
    char valStr[CRS_NVS_LINE_BYTES] = { 0 };
    int i = 0;

    if (memcmp(token, "16b", 3) == 0)
    {
        uint16_t val = CLI_convertStrUint(&rdResp[6]);
        for (i = 0; i < 16; i++)
        {

            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val |= (1 << (15 - i));
            }
            else if (tokenValue[i] == '0')
            {
                val &= ~(1 << (15 - i));
            }
        }

        int2hex(val, valStr);
    }
    else if (memcmp(token, "32b", 3) == 0)
    {
        uint32_t val = CLI_convertStrUint(&rdResp[6]);

        for (i = 0; i < 32; i++)
        {

            if (tokenValue[i] == '*')
            {
                continue;
            }
            else if (tokenValue[i] == '1')
            {
                val |= (1 << (31 - i));
            }
            else if (tokenValue[i] == '0')
            {
                val &= ~(1 << (31 - i));
            }
        }

        //        uint32_t tokenVal = strtol(&tokenValue[0], NULL, 2);
        //
        ////                CLI_cliPrintf("16b val is %08x\r\n", val);
        ////                CLI_cliPrintf("16b tokenVal is %08x\r\n", tokenVal);
        //        val |= tokenVal;
        ////                CLI_cliPrintf("new val is %08x\r\n", val);
        int2hex(val, valStr);

    }
    strcat(result, valStr);
    memcpy(parsedLine, result, strlen(result));
    return CRS_SUCCESS;
}

CRS_retVal_t Convert_rdParser(char *addr, char respLine[9][CRS_NVS_LINE_BYTES])
{
    char *s = " ";
    char mode[3] = { 0 };
    char *token = strtok(addr, s);            //w
    memcpy(mode, token, 2);
    token = strtok(NULL, s);            //addr
    uint32_t addrVal = CLI_convertStrUint(&token[2]);
    if (memcmp(mode, "ew", 2) != 0)
    {
        char baseAddr[9] = { 0 };
        memset(baseAddr, '0', 8);
        memcpy(baseAddr, &token[2], 5);
        uint32_t baseAddrVal = CLI_convertStrUint(&baseAddr[0]);
        addrVal = ((addrVal - baseAddrVal) / 4);
    }
    char addrValHex[12] = { 0 };
    int2hex(addrVal, addrValHex);
    char rd_converted[2][CRS_NVS_LINE_BYTES] = { "wr 0x51 0x", "rd 0x51" };
    strcat(rd_converted[0], addrValHex);
    strcat(rd_converted[0], "0000");
    memcpy(respLine[0], rd_converted[0], CRS_NVS_LINE_BYTES);
    memcpy(respLine[1], rd_converted[1], CRS_NVS_LINE_BYTES);
    return CRS_SUCCESS;
}

CRS_retVal_t Convert_applyLine(char respLine[9][CRS_NVS_LINE_BYTES],
                               uint32_t LUT_line, CRS_chipMode_t chipMode)
{
//    CLI_cliPrintf("\r\nConvert_applyLine");

    gIsFirstLine = true;
    int rspLineIdx = 0;
    char LUT[20] = { 0 };
    if (chipMode == MODE_NATIVE)
    {
        memcpy(LUT, "wr 0x50 0x43", strlen("wr 0x50 0x43"));
    }
    else if (chipMode == MODE_SPI_SLAVE)
    {
        memcpy(LUT, "wr 0x60 0x43", strlen("wr 0x60 0x43"));

    }
    uint32_t i = 0;
    i |= (0xf & gLUT_num);
    i |= (0xf80 & (LUT_line << 8));
    char i_str[20] = { 0 };
    int2hex(i, i_str);
    int len_i_str = strlen(i_str);
    len_i_str = 4 - len_i_str;
    int k = 0;
    for (k = 0; k < len_i_str; ++k)
    {
        strcat(LUT, "0");
    }
    //            CLI_cliPrintf("\r\nLUT1: %s", LUT);

    strcat(LUT, i_str);
    //            CLI_cliPrintf("\r\nLUT3: %s", LUT);

    memcpy(respLine[rspLineIdx], LUT, strlen(LUT)); //LUT number and line number
    //apply
    rspLineIdx++;

    char lines[2][25] = { 0 };
    memset(lines[0], 0, 24);
    memset(lines[1], 0, 24);

    if (chipMode == MODE_NATIVE)
    {
        memcpy(lines[0], "wr 0x50 0x000001", strlen("wr 0x50 0x000001"));
        memcpy(lines[1], "wr 0x50 0x000000", strlen("wr 0x50 0x000000"));

    }
    else if (chipMode == MODE_SPI_SLAVE)
    {
        memcpy(lines[0], "wr 0x60 0x000001", strlen("wr 0x60 0x000001"));
        memcpy(lines[1], "wr 0x60 0x000000", strlen("wr 0x60 0x000000"));

    }
    memcpy(respLine[rspLineIdx], lines[0], strlen(lines[0]));
    rspLineIdx++;

    memcpy(respLine[rspLineIdx], lines[1], strlen(lines[1]));
    return CRS_SUCCESS;
}

//slave:
//wr 0x62 0xCH CH A A 0000
//rd 0x62
CRS_retVal_t Convert_rd(uint32_t addr, CRS_chipMode_t mode, uint32_t rfAddr,
                        char convertedResp[2][CRS_NVS_LINE_BYTES])
{

//    char addrStr[15] = { 0 };

    if (mode == MODE_NATIVE)
    {
        sprintf(convertedResp[0], "wr 0x51 0x%x0000", addr);
        sprintf(convertedResp[1], "rd 0x51");

    }
    else if (mode == MODE_SPI_SLAVE)
    {
        sprintf(convertedResp[0], "wr 0x62 0x%x0000", addr);
        sprintf(convertedResp[1], "rd 0x62");

    }
    else
    {
        CLI_cliPrintf("\r\nIN Convert_rd INVALID MODE");
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;

}

//slave:
//wr 0x60 0xCH CH A A DDDD
//rd 0x61
CRS_retVal_t Convert_wr(uint32_t addr, uint32_t value, CRS_chipMode_t mode,
                        uint32_t rfAddr,
                        char convertedResp[2][CRS_NVS_LINE_BYTES])
{
    //rfAddr for byte 6
//char valStr[]
    if (mode == MODE_NATIVE)
    {
        sprintf(convertedResp[0], "wr 0x50 0x%x%x", addr, value);
    }
    else if (mode == MODE_SPI_SLAVE)
    {
        sprintf(convertedResp[0], "wr 0x60 0x%x%x", addr, value);
        sprintf(convertedResp[1], "wwr 0x61 0x0");

    }
    else
    {
        CLI_cliPrintf("\r\nIN Convert_rd INVALID MODE");
        return CRS_FAILURE;
    }
    return CRS_SUCCESS;

}

/******************************************************************************
 Local Functions
 *****************************************************************************/
static CRS_retVal_t checkRfChain(uint32_t addrVal)
{
//    CLI_cliPrintf("\r\in checkRfChain");

    if (addrVal >= 0x8 && addrVal <= 0xf)
    {
        if (gRfChain != 1)
        {
            gIsRfChainChanged = true;
        }
    }
    else if (addrVal >= 0x10 && addrVal <= 0x17)
    {
        if (gRfChain != 2)
        {

            gIsRfChainChanged = true;
        }
    }
    else if (addrVal >= 0x18 && addrVal <= 0x1f)
    {
        if (gRfChain != 3)
        {

            gIsRfChainChanged = true;
        }
    }
    else if (addrVal >= 0x26 && addrVal <= 0x27)
    {
        if (gRfChain != 4)
        {
            gIsRfChainChanged = true;
        }
    }
    return CRS_SUCCESS;
}

static CRS_retVal_t changeRfChain(uint32_t addrVal)
{
    if (addrVal >= 0x8 && addrVal <= 0xf)
    {
        if (gRfChain != 1)
        {
            gRfChain = 1;
            gLUT_num = 1;
        }
        if (gIsFirstLine == true)
        {
            gIsFirstLine = false;
        }
    }
    else if (addrVal >= 0x10 && addrVal <= 0x17)
    {
        if (gRfChain != 2)
        {
            gRfChain = 2;
            gLUT_num = 2;
        }
        if (gIsFirstLine == true)
        {
            gIsFirstLine = false;
        }
    }
    else if (addrVal >= 0x18 && addrVal <= 0x1f)
    {
        if (gRfChain != 3)
        {
            gRfChain = 3;
            gLUT_num = 4;
        }
        if (gIsFirstLine == true)
        {
            gIsFirstLine = false;
        }
    }
    else if (addrVal >= 0x26 && addrVal <= 0x27)
    {
        if (gRfChain != 4)
        {
            gRfChain = 4;
            gLUT_num = 8;
        }
        if (gIsFirstLine == true)
        {
            gIsFirstLine = false;
        }
    }
    return CRS_SUCCESS;
}
