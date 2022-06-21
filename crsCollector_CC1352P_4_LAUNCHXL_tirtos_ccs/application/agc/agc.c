/******************************************************************************
 Includes
 *****************************************************************************/
#include "mac/mac_util.h"
#include "application/crs/crs_thresholds.h"
#include "scif/scif.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "application/crs/crs.h"
#include "agc.h"
#include "application/crs/crs_tdd.h"
#include "application/util_timer.h"
#include "application/crs/crs_cli.h"
#include <ti/drivers/utils/Random.h>
#include <ti/devices/DeviceFamily.h>
#include <math.h>
#include <float.h>
#include DeviceFamily_constructPath(driverlib/aux_adc.h)

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
// Display error message if the SCIF driver has been generated with incorrect operating system setting
#if !(defined(SCIF_OSAL_TIRTOS_H) || defined(SCIF_OSAL_TIDPL_H))
    #error "SCIF driver has incorrect operating system configuration for this example. Please change to 'TI-RTOS' or 'TI Driver Porting Layer' in the Sensor Controller Studio project panel and re-generate the driver."
#endif

// Display error message if the SCIF driver has been generated with incorrect target chip package
#if !(defined(SCIF_TARGET_CHIP_PACKAGE_QFN48_7X7_RGZ) || defined(SCIF_TARGET_CHIP_PACKAGE_7X7_MOT))
    #error "SCIF driver has incorrect target chip package configuration for this example. Please change to 'QFN48 7x7 RGZ' in the Sensor Controller Studio project panel and re-generate the driver."
#endif

#define BV(n)               (1 << (n))

#define AGC_TIMEOUT 30000

/******************************************************************************
 Local variables
 *****************************************************************************/
static AGC_results_t gAgcResults = {.adcMaxResults={0}, .adcMinResults={0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}};
static AGC_max_results_t gAgcMaxResults  = {.IfMaxRx="N/A", .IfMaxTx="N/A", .RfMaxRx="N/A", .RfMaxTx="N/A", .adcValues={0}};
static int gAgcInitialized =0;
static int gAgcReady=0;
static int gAgcTimeout=0;
static Clock_Struct agcClkStruct;
static Clock_Handle agcClkHandle;
static void processAgcTimeoutCallback(UArg a0);

void scTaskAlertCallback(void) {
    //Log_info0("callback");
    // Wake up the OS task
    //Semaphore_post(Semaphore_handle(&semScTaskAlert));
    //Util_setEvent(&AGC_events, AGC_EVT);
    gAgcReady = 1;

} // taskAlertCallback


void scCtrlReadyCallback(void) {

} // ctrlReadyCallback


int Agc_isInitialized(){
    return gAgcInitialized;
}

int Agc_isReady(){
    return gAgcReady;
}

AGC_results_t Agc_getResults(){
    return gAgcResults;
 }

AGC_max_results_t Agc_getMaxResults(){
    return gAgcMaxResults;
 }


CRS_retVal_t Agc_init(){
    if(gAgcInitialized){
        return CRS_SUCCESS;
    }
    scifOsalInit();
    scifOsalRegisterCtrlReadyCallback(scCtrlReadyCallback);
    scifOsalRegisterTaskAlertCallback(scTaskAlertCallback);
    scifInit(&scifDriverSetup);

    int i;

    uint32_t randomNumber;
    uint16_t period = 5000;
    uint16_t dl1 = 3857;
    Random_seedAutomatic();
    for(i=0;i<scifTaskData.systemAgc.cfg.measuresCount;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow, the ranges are between 0 to TX/RX time minus 800ms.

        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayTX[i] = randomNumber;
        dl1 = 1214;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayRX[i] = randomNumber;
    }
    int16_t mode = Agc_getMode();
    scifTaskData.systemAgc.cfg.tddMode = mode;
    scifTaskData.systemAgc.cfg.channelsNum = 4;
    #ifndef CLI_SENSOR
        scifTaskData.systemAgc.cfg.unitType = 0;
    #else
        scifTaskData.systemAgc.cfg.unitType = 1;
    #endif
    // start task.
    scifStartTasksNbl(BV(SCIF_SYSTEM_AGC_TASK_ID));
    agcClkHandle = UtilTimer_construct(&agcClkStruct,
                                       processAgcTimeoutCallback,
                                       AGC_TIMEOUT, 0,
                                        true,
                                        0);

    gAgcInitialized = 1;
    return CRS_SUCCESS;
}


int Agc_convert(float voltage, int tx_rx, int rf_if)
{
    // tx_rx: 0 - RX, 1 - TX
    // rf_if: 0 - RF, 1 - IF
    int result;
    if (rf_if == 0)
    {

        if (tx_rx == 0)
        {
            voltage = voltage - 500000;
            voltage = voltage / -23400;
            result = (int)(voltage);
        }
        else
        {
            voltage = voltage/1000000;
            voltage = log(voltage) * 10.285 + 15.952;
            result = (int)(voltage);
        }

    }
    else
    {
        if (tx_rx == 0)
        {

            voltage = voltage - 230772;
            voltage = voltage / -6933.7;
            result = (int)(voltage);
        }
        else
        {
            voltage = voltage - 442934;
            voltage = voltage/ -5737;
            result = (int)(voltage);
        }
    }
    return result;
}

CRS_retVal_t Agc_setChannel(uint16_t channel){
    scifTaskData.systemAgc.cfg.channelsSwitch = channel;
    if(scifTaskData.systemAgc.cfg.channelsSwitch == channel){
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

uint16_t Agc_getChannel(){
    uint16_t channel = scifTaskData.systemAgc.cfg.channelsSwitch;
    return channel;
}

CRS_retVal_t Agc_setMode(int mode){
    // change tdd_mode in thrsh file.
    char envFile[1024] = { 0 };
    sprintf(envFile, "SensorMode=%x\n", mode);
    Thresh_write(envFile);
    //  clear the interrupt flag, reset results struct and rerun task.
    scifClearAlertIntSource();
    if(gAgcReady){
        scifAckAlertEvents();
    }
    gAgcReady = 0;
    scifStopTasksNbl(BV(SCIF_SYSTEM_AGC_TASK_ID));
    scifResetTaskStructs(BV(SCIF_SYSTEM_AGC_TASK_ID), BV(SCIF_STRUCT_OUTPUT));
    //scifTaskData.systemAgc.state.alertEnabled = 1;
    scifTaskData.systemAgc.cfg.tddMode = mode;
    scifTaskData.systemAgc.cfg.samplesCount = scifTaskData.systemAgc.cfg.samplesNum;
//    int i;
//    for(i=0;i<48;i++){
//        scifTaskData.systemAgc.output.minSamplesIFRX[i] = 0xffff;
//        scifTaskData.systemAgc.output.minSamplesIFTX[i] = 0xffff;
//        scifTaskData.systemAgc.output.minSamplesRFRX[i] = 0xffff;
//        scifTaskData.systemAgc.output.minSamplesRFTX[i] = 0xffff;
//    }
    //scifExecuteTasksOnceNbl(BV(SCIF_SYSTEM_AGC_TASK_ID));

    //scifStartTasksNbl(BV(SCIF_SYSTEM_AGC_TASK_ID));
    int i;
    for(i=0;i<16;i++){
        gAgcResults.adcMinResults[i] = 0xffffffff;
        gAgcResults.adcMaxResults[i] = 0;
    }
    for(i=0;i<4;i++){
        gAgcMaxResults.adcValues[i] = 0;
    }
    //gAgcMaxResults  = {.IfMaxRx="N/A", .IfMaxTx="N/A", .RfMaxRx="N/A", .RfMaxTx="N/A", .adcValues={0}};
    if(scifTaskData.systemAgc.cfg.tddMode == mode){
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

uint16_t Agc_getMode(){
    char envFile[4096] = { 0 };
    Thresh_read("SensorMode", envFile);
    uint16_t mode = strtol(envFile + strlen("SensorMode="), NULL, 16);
    //return scifTaskData.systemAgc.cfg.tddMode;
    return mode;
}

CRS_retVal_t Agc_sample_debug(){
    // Check for TDD open and lock
    int mode = scifTaskData.systemAgc.cfg.tddMode;
    if(mode == 2){
        CRS_retVal_t retVal = Tdd_isLocked();
        if( retVal == CRS_TDD_NOT_LOCKED){
            return retVal;
        }
    }
    // Clear the ALERT interrupt source
    scifClearAlertIntSource();

    int i;
    int j;
    // values to correct adc conversion offset
    int32_t adcOffset = AUXADCGetAdjustmentOffset(AUXADC_REF_FIXED);
    int32_t adcGainError = AUXADCGetAdjustmentGain(AUXADC_REF_FIXED);
    int32_t adcValue, adcCorrectedValue, adcValueMicroVolt;
    uint16_t channelsNum = scifTaskData.systemAgc.cfg.channelsNum;


    // maxTX , minTX, maxRX, minRX,
    uint16_t adcSums [6] = {0};

//    for(i=0;i<channelsNum;i++){
//        // reset the results array for new results
//        gAgcResults.adcResults[i] = 0;
//    }
    AGC_results_t newAgcResults = {0};
    // for each channel, calculate max average
    for(i=0;i<channelsNum;i++){
        adcSums[0] = scifTaskData.systemAgc.output.channelsMaxRFRX[i];
        adcSums[1] = scifTaskData.systemAgc.output.channelsMaxRFTX[i];
        adcSums[2] = scifTaskData.systemAgc.output.channelsMaxIFRX[i];
        adcSums[3] = scifTaskData.systemAgc.output.channelsMaxIFTX[i];
        for(j=0;j<4;j++){
            // modesChannel = number of results in top 20% and bottom 20% for each channel
            adcValue = adcSums[j]/ scifTaskData.systemAgc.cfg.modesChannel;

            adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
            adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
            if(gAgcTimeout || gAgcResults.adcMaxResults[i+(j*4)] < adcValueMicroVolt){
                newAgcResults.adcMaxResults[i+(j*4)] = adcValueMicroVolt;
            }
            else{
                newAgcResults.adcMaxResults[i+(j*4)] = gAgcResults.adcMaxResults[i+(j*4)];
            }
        }
    }
    // for each channel, calculate min average
    for(i=0;i<channelsNum;i++){
        adcSums[0] = scifTaskData.systemAgc.output.channelsMinRFRX[i];
        adcSums[1] = scifTaskData.systemAgc.output.channelsMinRFTX[i];
        adcSums[2] = scifTaskData.systemAgc.output.channelsMinIFRX[i];
        adcSums[3] = scifTaskData.systemAgc.output.channelsMinIFTX[i];
        for(j=0;j<4;j++){
            // modesChannel = number of results in top 20% and bottom 20% for each channel
            adcValue = adcSums[j]/ scifTaskData.systemAgc.cfg.modesChannel;

            adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
            adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
            if(gAgcTimeout || gAgcResults.adcMinResults[i+(j*4)] > adcValueMicroVolt){
                newAgcResults.adcMinResults[i+(j*4)] = adcValueMicroVolt;
            }
            else{
                newAgcResults.adcMinResults[i+(j*4)] = gAgcResults.adcMinResults[i+(j*4)];
            }
        }
    }
    gAgcResults = newAgcResults;
    uint32_t randomNumber;
    TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // uint16_t period = tdArgs.period;
    // uint16_t dl1 = tdArgs.dl1;
    uint16_t period = 5000;
    uint16_t dl1 = 3857;
    for(i=0;i<scifTaskData.systemAgc.cfg.samplesCount;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow, the ranges are between 0 to TX/RX time minus 800ms.
        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayTX[i] = randomNumber;
        dl1 = 1214;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayRX[i] = randomNumber;
    }

    gAgcReady = 0;
    if(gAgcTimeout){
        gAgcTimeout = 0;
        //CLI_cliPrintf("\r\nTime out");
        UtilTimer_setTimeout(agcClkHandle, AGC_TIMEOUT);
        UtilTimer_start(&agcClkStruct);

    }
    // Acknowledge the ALERT event. Note that there are no event flags for this task since the Sensor
    // Controller uses fwGenQuickAlertInterrupt(), but this function must be called nonetheless.
    scifAckAlertEvents();
    // re-enable ALERT generation (on buffer half full)
    scifTaskData.systemAgc.state.alertEnabled = 1;
    //Semaphore_post(collectorSem);

    return CRS_SUCCESS;
}


CRS_retVal_t Agc_sample(){
    // Check for TDD open and lock
    int mode = scifTaskData.systemAgc.cfg.tddMode;
    if(mode == 2){
        CRS_retVal_t retVal = Tdd_isLocked();
        if( retVal == CRS_TDD_NOT_LOCKED){
            AGC_max_results_t gAgcNewResults  ={.IfMaxRx="N/A", .IfMaxTx="N/A", .RfMaxRx="N/A", .RfMaxTx="N/A",
                                                .adcValues={0}};
            gAgcMaxResults = gAgcNewResults;
            //CLI_cliPrintf("\r\nSC is not locked");
            return retVal;
        }
    }
    if(!gAgcReady){
        AGC_max_results_t gAgcNewResults  ={.IfMaxRx="N/A", .IfMaxTx="N/A", .RfMaxRx="N/A", .RfMaxTx="N/A",
                                            .adcValues={0}};
        gAgcMaxResults = gAgcNewResults;
        //CLI_cliPrintf("\r\nSC is not ready. sample is: %u", scifTaskData.systemAgc.cfg.samplesCount);
        return CRS_FAILURE;
    }
    // Clear the ALERT interrupt source
    scifClearAlertIntSource();

    int i;
    // values to correct adc conversion offset
    int32_t adcOffset = AUXADCGetAdjustmentOffset(AUXADC_REF_FIXED);
    int32_t adcGainError = AUXADCGetAdjustmentGain(AUXADC_REF_FIXED);
    int32_t adcValue, adcCorrectedValue, adcValueMicroVolt;
    uint16_t channelsNum = scifTaskData.systemAgc.cfg.channelsNum;

    // maxTX , minTX, maxRX, minRX,
    uint16_t adcSums [4] = {0};

    // get the highest channel
    for(i=0;i<channelsNum;i++){
        if(adcSums[0] < scifTaskData.systemAgc.output.channelsMaxRFRX[i]){
            adcSums[0] = scifTaskData.systemAgc.output.channelsMaxRFRX[i];
        }
        if(adcSums[1] < scifTaskData.systemAgc.output.channelsMaxRFTX[i]){
            adcSums[1] = scifTaskData.systemAgc.output.channelsMaxRFTX[i];
        }
        if(adcSums[2] < scifTaskData.systemAgc.output.channelsMaxIFRX[i]){
            adcSums[2] = scifTaskData.systemAgc.output.channelsMaxIFRX[i];
        }
        if(adcSums[3] < scifTaskData.systemAgc.output.channelsMaxIFTX[i]){
            adcSums[3] = scifTaskData.systemAgc.output.channelsMaxIFTX[i];
        }
    }
    if(mode==0 || mode==2){
        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[0]/ scifTaskData.systemAgc.cfg.modesChannel;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        //newAgcResults.RfMaxRx = adcValueMicroVolt;
        if(gAgcTimeout || gAgcMaxResults.adcValues[0] < adcValueMicroVolt){
            gAgcMaxResults.adcValues[0] = adcValueMicroVolt;
            sprintf(gAgcMaxResults.RfMaxRx,"%i" ,Agc_convert(adcValueMicroVolt, 0, 0));
        }

        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[2]/ scifTaskData.systemAgc.cfg.modesChannel;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        //newAgcResults.IfMaxRx = adcValueMicroVolt;
        if(gAgcTimeout || gAgcMaxResults.adcValues[2] < adcValueMicroVolt){
            gAgcMaxResults.adcValues[2] = adcValueMicroVolt;
            sprintf(gAgcMaxResults.IfMaxRx,"%i" ,Agc_convert(adcValueMicroVolt, 0, 1));
        }
    }
    else{
        strcpy(gAgcMaxResults.RfMaxRx, "N/A");
        strcpy(gAgcMaxResults.IfMaxRx, "N/A");
        gAgcMaxResults.adcValues[0] = 0;
        gAgcMaxResults.adcValues[2] = 0;
    }

    if(mode==1 || mode==2){
        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[1]/ scifTaskData.systemAgc.cfg.modesChannel;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        //newAgcResults.RfMaxTx = adcValueMicroVolt;
        if(gAgcTimeout || gAgcMaxResults.adcValues[1] < adcValueMicroVolt){
            gAgcMaxResults.adcValues[1] = adcValueMicroVolt;
            sprintf(gAgcMaxResults.RfMaxTx,"%i" ,Agc_convert(adcValueMicroVolt, 1, 0));
        }

        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[3]/ scifTaskData.systemAgc.cfg.modesChannel;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        //newAgcResults.IfMaxTx = adcValueMicroVolt;
        if(gAgcTimeout || gAgcMaxResults.adcValues[3] < adcValueMicroVolt){
            gAgcMaxResults.adcValues[3] = adcValueMicroVolt;
            sprintf(gAgcMaxResults.IfMaxTx,"%i" ,Agc_convert(adcValueMicroVolt, 1, 1));
        }
    }
    else{
        strcpy(gAgcMaxResults.RfMaxTx, "N/A");
        strcpy(gAgcMaxResults.IfMaxTx, "N/A");
        gAgcMaxResults.adcValues[1] = 0;
        gAgcMaxResults.adcValues[3] = 0;
    }

    uint32_t randomNumber;
    TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    uint16_t period = tdArgs.period;
    uint16_t dl1 = tdArgs.dl1;
    //uint16_t period = 5000;
    //uint16_t dl1 = 3857;
    for(i=0;i<scifTaskData.systemAgc.cfg.samplesCount;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow, the ranges are between 0 to TX/RX time minus 800ms.
        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayTX[i] = randomNumber;
        dl1 = 1214;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayRX[i] = randomNumber;
    }

    gAgcReady = 0;
    if(gAgcTimeout){
        //CLI_cliPrintf("\r\nTime out");
        gAgcTimeout = 0;
        UtilTimer_setTimeout(agcClkHandle, AGC_TIMEOUT);
        UtilTimer_start(&agcClkStruct);

    }
    // Acknowledge the ALERT event. Note that there are no event flags for this task since the Sensor
    // Controller uses fwGenQuickAlertInterrupt(), but this function must be called nonetheless.
    scifAckAlertEvents();
    // re-enable ALERT generation (on buffer half full)
    scifTaskData.systemAgc.state.alertEnabled = 1;
    //Semaphore_post(collectorSem);

    return CRS_SUCCESS;
}

CRS_retVal_t Agc_getValues(int* tx_value, int* rx_value){
    // tx_value: in collector this is "ULDetMaxPwr", in sensor this is "DLDetMaxOutPwr" (range 10 to 23)
    // rx_value: in collector this is "DLDetMaxInPwr", in sensor this is "ULDetMaxInPwr" (range -60 to -30)
    if(gAgcReady){
        CRS_retVal_t retStatus =  Agc_sample();
        if (retStatus == CRS_SUCCESS){
            //*tx_value = Agc_convert(gAgcResults.adcResults[0], 1);
            //*rx_value = Agc_convert(gAgcResults.adcResults[2], 0);
        }
        return retStatus;
    }
    else{
        return CRS_FAILURE;
    }
}


CRS_retVal_t Agc_getControlPins(int mode, int channel, AGC_ctrlPins_t* pins){
    /*
     * mode: RX - 0, TX - 1.
     * channel(TP number): 1, 2, 3 or 4.
     * pins: ADC_AnaSW_0_DIO26/ADC_AnaSW_1_DIO27 - 0 or 1
    */
        if((mode == 1 && channel == 1)||(mode == 0 && channel == 4)){
            (*pins).ADC_AnaSW_0_DIO26 = 0;
            (*pins).ADC_AnaSW_1_DIO27 = 0;
            return CRS_SUCCESS;
        }
        if ((mode == 1 && channel == 2)||(mode == 0 && channel == 3)){
            (*pins).ADC_AnaSW_0_DIO26 = 1;
            (*pins).ADC_AnaSW_1_DIO27 = 0;
            return CRS_SUCCESS;
        }
        if ((mode == 1 && channel == 3)||(mode == 0 && channel == 2)){
            (*pins).ADC_AnaSW_0_DIO26 = 0;
            (*pins).ADC_AnaSW_1_DIO27 = 1;
            return CRS_SUCCESS;
        }
        if ((mode == 1 && channel == 4)||(mode == 0 && channel == 1)){
            (*pins).ADC_AnaSW_0_DIO26 = 1;
            (*pins).ADC_AnaSW_1_DIO27 = 1;
            return CRS_SUCCESS;
        }

    return CRS_FAILURE;
}

static void processAgcTimeoutCallback(UArg a0)
{
    gAgcTimeout = true;
}

#ifdef CLI_SENSOR
CRS_retVal_t Agc_setLock(bool lock){

    scifTaskData.systemAgc.input.tddLock = lock;
    return CRS_SUCCESS;
}
#endif
