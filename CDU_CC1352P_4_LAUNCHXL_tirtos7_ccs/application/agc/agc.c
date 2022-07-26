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
#include "application/crs/crs_agc_management.h"
#include "application/crs/crs_alarms.h"
#include "application/crs/crs_tdd.h"
#include "application/util_timer.h"
#include "application/crs/crs_cli.h"
#include <ti/drivers/utils/Random.h>
#include <ti/devices/DeviceFamily.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/hal/Seconds.h>

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

#define AGC_INTERVAL 1000
#define AGC_HOLD 60
#define DC_RF_HIGH_FREQ_HB_RX 21
#define DC_IF_LOW_FREQ_TX 21
#define UC_RF_HIGH_FREQ_HB_TX 21
#define UC_IF_LOW_FREQ_RX 15

#define AGC_GET_SAMPLE_EVT 0x0001

/******************************************************************************
 Local variables
 *****************************************************************************/
static AGC_results_t gAgcResults = {.adcMaxResults={0}, .adcMinResults={0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}, .adcAvgResults={0}};
static AGC_max_results_t gAgcMaxResults  = {.IfMaxDL="N/A", .IfMaxUL="N/A", .RfMaxDL="N/A", .RfMaxUL="N/A"};
static AGC_max_stored_t gAgcMaxStored = {.dbValues={{0}, {0}, {0}, {0}}, .times={0}};
static int gAgcInitialized =0;
static int gAgcReady=0;
static uint32_t gCurrentMaxResults [SAMPLE_TYPES] = {0xffffffff, 0x0, 0xffffffff, 0xffffffff};
static Clock_Struct agcClkStruct;
static Clock_Handle agcClkHandle;
static void* gSem;
static uint16_t Agc_events = 0;
#ifdef CLI_SENSOR
static bool gIsTDDLocked=0;
#endif

static void processAgcTimeoutCallback(UArg a0);
static void processAgcSample();
static void Agc_updateMaxStored(int db, int type);
static bool Agc_getLock();

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


CRS_retVal_t Agc_init(void * sem){
    if(gAgcInitialized){
        return CRS_SUCCESS;
    }
    gSem = sem;
    scifOsalInit();
    scifOsalRegisterCtrlReadyCallback(scCtrlReadyCallback);
    scifOsalRegisterTaskAlertCallback(scTaskAlertCallback);
    scifInit(&scifDriverSetup);

    int i;

    uint32_t randomNumber;
    uint16_t period = 5000;
    uint16_t dl1 = 3714;
    Random_seedAutomatic();
    for(i=0;i<SCIF_SYSTEM_AGC_SAMPLE_SIZE;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow,
        // the ranges are between 0 to DL/UL time minus 800ms.

        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayUL[i] = randomNumber;
        dl1 = 1286;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayDL[i] = randomNumber;
    }
    int16_t mode = Agc_getMode();
    scifTaskData.systemAgc.cfg.tddMode = mode;
    #ifndef CLI_SENSOR
        scifTaskData.systemAgc.cfg.unitType = 0;
    #else
        scifTaskData.systemAgc.cfg.unitType = 1;
    #endif
    // get AGC interval time
    char thrshFile[4096]={0};
    Thresh_read("AgcInterval", thrshFile);
    uint32_t agcInterval = strtol(thrshFile + strlen("AgcInterval="), NULL, 10);
    if((!agcInterval) || (agcInterval<AGC_INTERVAL)){
        agcInterval = AGC_INTERVAL;
    }
    // start task.
    scifStartTasksNbl(BV(SCIF_SYSTEM_AGC_TASK_ID));
    agcClkHandle = UtilTimer_construct(&agcClkStruct, processAgcTimeoutCallback,
                                       agcInterval, 0, true, 0);
    gAgcInitialized = 1;
    return CRS_SUCCESS;
}

void Agc_process(void)
{
    // Agc_process: get the samples, check for alarms, update interval.
    if (Agc_events & AGC_GET_SAMPLE_EVT){

        Agc_sample();
        char envFile[4096]={0};
        //AGC_max_results_t agcResults = Agc_getMaxResults();
        // agcResults.adcValues [] - 0 RfMaxRx, 1 RfMaxTx, 2 IfMaxRx, 3 IfMaxTx
        int mode = Agc_getMode();
        #ifndef CLI_SENSOR
            Thresh_read("DLMaxInputPower", envFile);
            int16_t dlMaxInputPower = strtol(envFile + strlen("DLMaxInputPower="),
            NULL, 10);
            if ((dlMaxInputPower < Agc_convert(gCurrentMaxResults[0], 0, 0)) && (mode==1))
            {
                Alarms_setAlarm(DLMaxInputPower);
            }
            else
            {
                Alarms_clearAlarm(DLMaxInputPower, ALARM_INACTIVE);
            }
            memset(envFile, 0, 4096);
            Thresh_read("ULMaxOutputPower", envFile);
            int16_t ulMaxOutputPower = strtol(envFile + strlen("ULMaxOutputPower="),
            NULL, 10);
            if ((ulMaxOutputPower < Agc_convert(gCurrentMaxResults[1], 1, 0)) && (mode==2) )
            {
                Alarms_setAlarm(ULMaxOutputPower);
            }
            else
            {
                Alarms_clearAlarm(ULMaxOutputPower, ALARM_INACTIVE);
            }
        #else
            Thresh_read("ULMaxInputPower", envFile);
            int16_t ulMaxInputPower = strtol(envFile + strlen("ULMaxInputPower="),
            NULL, 10);
            if ((ulMaxInputPower < Agc_convert(gCurrentMaxResults[1], 0, 0)) && (mode==2) )
            {
                Alarms_setAlarm(ULMaxInputPower);
            }
            else
            {
                Alarms_clearAlarm(ULMaxInputPower, ALARM_INACTIVE);
            }
            memset(envFile, 0, 4096);
            Thresh_read("DLMaxOutputPower", envFile);
            int16_t dlMaxOutputPower = strtol(envFile + strlen("DLMaxOutputPower="),
            NULL, 10);
            if ((dlMaxOutputPower < Agc_convert(gCurrentMaxResults[0], 1, 0)) && (mode==1)  )
            {
                Alarms_setAlarm(DLMaxOutputPower);
            }
            else
            {
                Alarms_clearAlarm(DLMaxOutputPower, ALARM_INACTIVE);
            }
        #endif
        memset(envFile, 0, 4096);
        Thresh_read("AgcInterval", envFile);
        uint32_t agcInterval = strtol(envFile + strlen("AgcInterval="), NULL, 10);
        if((!agcInterval) || (agcInterval<AGC_INTERVAL)){
            agcInterval = AGC_INTERVAL;
        }
        UtilTimer_setTimeout(agcClkHandle, agcInterval);
        UtilTimer_start(&agcClkStruct);
        AGCM_finishedTask();
        /* Clear the event */
        Util_clearEvent(&Agc_events, AGC_GET_SAMPLE_EVT);
    }
}


int Agc_convert(float voltage, int tx_rx, int rf_if){
    // voltage- in microVolts.
    // tx_rx: 0 - RX, 1 - TX
    // rf_if: 0 - RF, 1 - IF
    // return: sensor value in dB.
    int result;
    int offset;
    if (rf_if == 0)
    {
        if (tx_rx == 0)
        {
            // RF RX - RX_DET_An
            // tests the power that enters the RF/IF card (RSSI)
            // y = -0.023x + 0.2791 , R^2 = 0.9998
            offset = CRS_cbGainStates.dc_rf_high_freq_hb_rx - DC_RF_HIGH_FREQ_HB_RX;
            voltage = voltage - 279100;
            voltage = voltage / -23000;
            result = (int)(voltage) + offset;
        }
        else
        {
            // RF TX - TX_DET_An
            // power that comes out of FEM card
            // y = ln(x) * 10.094 - 121.61
            offset = CRS_cbGainStates.uc_rf_high_freq_hb_tx - UC_RF_HIGH_FREQ_HB_TX;
            voltage = log(voltage) * 10.094 - 121.61;
            result = (int)(voltage) + offset;
        }

    }
    else
    {
        if (tx_rx == 0)
        {
            // IF RX - IF_DET_An_RX (DownLink)
            // tests the power coming out of the CAT5PA into the cable
            // y = -0.0187x + 0.5642 , R^2 = 0.9977
            offset = CRS_cbGainStates.uc_if_low_freq_rx - UC_IF_LOW_FREQ_RX;
            voltage = voltage - 564200;
            voltage = voltage / -18700;
            result = (int)(voltage) + offset;
        }
        else
        {
            // IF TX - IF_DET_An_TX (UpLink)
            // tests the power coming from the cable
            // y = -0.0219x + 0.5669 , R^2 = 0.9995
            offset = CRS_cbGainStates.dc_if_low_freq_tx - DC_IF_LOW_FREQ_TX;
            voltage = voltage - 566900;
            voltage = voltage/ -21900;
            result = (int)(voltage) + offset;
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
    //  clear the interrupt flag
    scifClearAlertIntSource();
    if(gAgcReady){
        scifAckAlertEvents();
    }
    gAgcReady = 0;

    scifTaskData.systemAgc.state.alertEnabled = 1;
    scifTaskData.systemAgc.cfg.tddMode = mode;

    int i;
    for(i=0;i<SCIF_SYSTEM_AGC_AVERAGE_SIZE;i++){
        gAgcResults.adcMinResults[i] = 0xffffffff;
        gAgcResults.adcMaxResults[i] = 0;
        gAgcResults.adcAvgResults[i] = 0;
    }
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        if(i!=1){
            gCurrentMaxResults[i] = 0xffffffff;
        }
        else{
            gCurrentMaxResults[i] = 0x0;
        }
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
    if (mode > 2){
        mode = 2;
    }
    //return scifTaskData.systemAgc.cfg.tddMode;
    return mode;
}

CRS_retVal_t Agc_sample_debug(){
    // Check for TDD open and lock
    int mode = scifTaskData.systemAgc.cfg.tddMode;
    if(mode == 0){
        bool lock = Agc_getLock();
        if(!lock){
            return CRS_FAILURE;
        }
    }
    if(!gAgcReady){
        return CRS_FAILURE;
    }
    // Clear the ALERT interrupt source
    scifClearAlertIntSource();

    int i;
    int j;
    // values to correct adc conversion offset
    int32_t adcOffset = AUXADCGetAdjustmentOffset(AUXADC_REF_FIXED);
    int32_t adcGainError = AUXADCGetAdjustmentGain(AUXADC_REF_FIXED);
    int32_t adcValue, adcCorrectedValue, adcValueMicroVolt;


    // maxTX , minTX, maxRX, minRX,
    uint16_t adcSums [SAMPLE_TYPES] = {0};

    AGC_results_t newAgcResults = {0};

    // for each channel, calculate max average
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsMaxRFDL[i];
        adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsMaxRFUL[i];
        adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsMaxIFDL[i];
        adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsMaxIFUL[i];
        for(j=0;j<SAMPLE_TYPES;j++){
            // number of results in top 20% and bottom 20% for each channel
            adcValue = adcSums[j]/ SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;

            adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
            adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
            newAgcResults.adcMaxResults[i+(j*SAMPLE_TYPES)] = adcValueMicroVolt;

        }
    }
    // for each channel, calculate min average
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsMinRFDL[i];
        adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsMinRFUL[i];
        adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsMinIFDL[i];
        adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsMinIFUL[i];
        for(j=0;j<SAMPLE_TYPES;j++){
            // modesChannel = number of results in top 20% and bottom 20% for each channel
            adcValue = adcSums[j]/ SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;

            adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
            adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
            newAgcResults.adcMinResults[i+(j*SAMPLE_TYPES)] = adcValueMicroVolt;
        }
    }
    // for each channel calculate average
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsAverage[i];
        adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsAverage[i+SAMPLE_TYPES];
        adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsAverage[i+(SAMPLE_TYPES*2)];
        adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsAverage[i+(SAMPLE_TYPES*3)];
        for(j=0;j<SAMPLE_TYPES;j++){
            // divide the sum of the results by 90% of total samples (because we discard the top and bottom 5% of samples)
            adcValue = adcSums[j]/ ( (SCIF_SYSTEM_AGC_BUFFER_SIZE*SCIF_SYSTEM_AGC_SAMPLE_SIZE) -
                    ((SCIF_SYSTEM_AGC_MODES_CHANNEL_SIZE - SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE)*2) );
            adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
            adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
            newAgcResults.adcAvgResults[i+(j*SAMPLE_TYPES)] = adcValueMicroVolt;

        }
    }

    gAgcResults = newAgcResults;
    uint32_t randomNumber;
    TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // uint16_t period = tdArgs.period;
    // uint16_t dl1 = tdArgs.dl1;
    uint16_t period = 5000;
    uint16_t dl1 = 3714;
    for(i=0;i<SCIF_SYSTEM_AGC_SAMPLE_SIZE;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow, the ranges are between 0 to TX/RX time minus 800ms.
        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayUL[i] = randomNumber;
        dl1 = 1286;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayDL[i] = randomNumber;
    }

    gAgcReady = 0;

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
    int i = 0;
    if(mode == 0){
        bool lock = Agc_getLock();
        if(!lock){
            AGC_max_results_t gAgcNewResults  ={.IfMaxDL="N/A", .IfMaxUL="N/A", .RfMaxDL="N/A", .RfMaxUL="N/A"};
            gAgcMaxResults = gAgcNewResults;
            for(i=0; i<SAMPLE_TYPES; i++){
                if(i!=1){
                    gCurrentMaxResults[i] = 0xffffffff;
                }
                else{
                    gCurrentMaxResults[i] = 0x0;
                }
            }

            //CLI_cliPrintf("\r\nSC is not locked");
            return CRS_FAILURE;
        }
    }
    if(!gAgcReady){
        AGC_max_results_t gAgcNewResults  ={.IfMaxDL="N/A", .IfMaxUL="N/A", .RfMaxDL="N/A", .RfMaxUL="N/A"};
        gAgcMaxResults = gAgcNewResults;
        for(i =0; i<SAMPLE_TYPES; i++){
            if(i!=UL_RF){
                gCurrentMaxResults[i] = 0xffffffff;
            }
            else{
                gCurrentMaxResults[i] = 0x0;
            }
        }
        return CRS_FAILURE;
    }
    // Clear the ALERT interrupt source
    scifClearAlertIntSource();

    // values to correct ADC conversion offset
    int32_t adcOffset = AUXADCGetAdjustmentOffset(AUXADC_REF_FIXED);
    int32_t adcGainError = AUXADCGetAdjustmentGain(AUXADC_REF_FIXED);
    int32_t adcValue, adcCorrectedValue, adcValueMicroVolt;

    // DL_RF , UL_RF, DL_IF, UL_IF,
    uint32_t adcSums [SAMPLE_TYPES] = {0xffffffff, 0x0, 0xffffffff, 0xffffffff};

    // get the highest channel
    // for some values lower voltage means higher dB
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        if(adcSums[DL_RF] > scifTaskData.systemAgc.output.channelsMinRFDL[i]){
            adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsMinRFDL[i];
        }
        if(adcSums[UL_RF] < scifTaskData.systemAgc.output.channelsMaxRFUL[i]){
            adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsMaxRFUL[i];
        }
        if(adcSums[DL_IF] > scifTaskData.systemAgc.output.channelsMinIFDL[i]){
            adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsMinIFDL[i];
        }
        if(adcSums[UL_IF] > scifTaskData.systemAgc.output.channelsMinIFUL[i]){
            adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsMinIFUL[i];
        }
    }

    if(scifTaskData.systemAgc.cfg.channelsSwitch != 0){
        adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsMinRFDL[scifTaskData.systemAgc.cfg.channelsSwitch-1];
        adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsMaxRFUL[scifTaskData.systemAgc.cfg.channelsSwitch-1];
        adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsMinIFDL[scifTaskData.systemAgc.cfg.channelsSwitch-1];
        adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsMinIFUL[scifTaskData.systemAgc.cfg.channelsSwitch-1];
    }
//    else{
//        // for now take only from channel 1 for testing
//        adcSums[DL_RF] = scifTaskData.systemAgc.output.channelsMinRFDL[0];
//        adcSums[UL_RF] = scifTaskData.systemAgc.output.channelsMaxRFUL[0];
//        adcSums[DL_IF] = scifTaskData.systemAgc.output.channelsMinIFDL[0];
//        adcSums[UL_IF] = scifTaskData.systemAgc.output.channelsMinIFUL[0];
//    }


    if(mode==0 || mode==1){
        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[DL_RF]/  SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        gCurrentMaxResults[DL_RF] = adcValueMicroVolt;
        #ifndef CLI_SENSOR
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 0, 0), 0);
        #else
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 1, 0), 0);
        #endif
        sprintf(gAgcMaxResults.RfMaxDL,"%i" ,gAgcMaxStored.dbValues[DL_RF][0]);

        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[DL_IF]/  SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        gCurrentMaxResults[DL_IF] = adcValueMicroVolt;
        #ifndef CLI_SENSOR
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 0, 1), 2);
        #else
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 1, 1), 2);
        #endif
        sprintf(gAgcMaxResults.IfMaxDL,"%i" ,gAgcMaxStored.dbValues[DL_IF][0]);
    }
    else{
        strcpy(gAgcMaxResults.RfMaxDL, "N/A");
        strcpy(gAgcMaxResults.IfMaxDL, "N/A");
        gCurrentMaxResults[0] = 0xffffffff;
        gCurrentMaxResults[2] = 0xffffffff;
    }

    if(mode==0 || mode==2){
        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[UL_RF]/ SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        gCurrentMaxResults[UL_RF] = adcValueMicroVolt;
        #ifndef CLI_SENSOR
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 1, 0), 1);
        #else
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 0, 1), 1);
        #endif
        sprintf(gAgcMaxResults.RfMaxUL,"%i" ,gAgcMaxStored.dbValues[UL_RF][0]);

        // modesChannel = number of results in top 20% and bottom 20% for each channel
        adcValue = adcSums[UL_IF]/ SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE;
        adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
        adcValueMicroVolt = AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
        gCurrentMaxResults[UL_IF] = adcValueMicroVolt;
        #ifndef CLI_SENSOR
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 1, 1), 3);
        #else
            Agc_updateMaxStored(Agc_convert(adcValueMicroVolt, 0, 1), 3);
        #endif
        sprintf(gAgcMaxResults.IfMaxUL,"%i" ,gAgcMaxStored.dbValues[UL_IF][0]);
    }
    else{
        strcpy(gAgcMaxResults.RfMaxUL, "N/A");
        strcpy(gAgcMaxResults.IfMaxUL, "N/A");
        gCurrentMaxResults[1] = 0x0;
        gCurrentMaxResults[3] = 0xffffffff;
    }

    uint32_t randomNumber;
    TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    // TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    uint16_t period = tdArgs.period;
    uint16_t dl1 = tdArgs.dl1;
    //uint16_t period = 5000;
    //uint16_t dl1 = 3714;
    for(i=0;i<SCIF_SYSTEM_AGC_SAMPLE_SIZE;i++){
        // generate random delays for both TX and RX samples to start measuring from. In order to prevent overflow, the ranges are between 0 to TX/RX time minus 800ms.
        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayUL[i] = randomNumber;
        dl1 = 1286;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayDL[i] = randomNumber;
    }

    gAgcReady = 0;

    // Acknowledge the ALERT event. Note that there are no event flags for this task since the Sensor
    // Controller uses fwGenQuickAlertInterrupt(), but this function must be called nonetheless.
    scifAckAlertEvents();
    // re-enable ALERT generation (on buffer half full)
    scifTaskData.systemAgc.state.alertEnabled = 1;
    //Semaphore_post(collectorSem);

    return CRS_SUCCESS;
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
    AGCM_runTask(processAgcSample);

    //gAgcTimeout = true;
}

static void processAgcSample()
{
    Util_setEvent(&Agc_events, AGC_GET_SAMPLE_EVT);

       /* Wake up the application thread when it waits for clock event */
       Semaphore_post(gSem);

}

//static uint32_t cmpfunc1(const void * p1, const void * p2){
//    return (*(uint32_t*)p1 - *(uint32_t*)p2) ;
//}
//
//static uint32_t cmpfunc2(const void * p1, const void * p2){
//    return (*(uint32_t*)p2 - *(uint32_t*)p1);
//}

static void Agc_updateMaxStored(int db, int type){
    // type: 0 - rfDL, 1 - rfUL, 2- ifDL, 3 - ifUL
    // for all stored values of the type check it their time expired, if so then reset them so they will be overwritten
    int i = 0;
    int j = 0;
    for(i=0;i<STORED_NUM;i++){
        if( Seconds_get() - gAgcMaxStored.times[type][i]  > AGC_HOLD){
            for(j=i;j<STORED_NUM-1;j++){
                gAgcMaxStored.dbValues[type][j] = gAgcMaxStored.dbValues[type][j+1];
                gAgcMaxStored.times[type][j] = gAgcMaxStored.times[type][j+1];
            }
            gAgcMaxStored.dbValues[type][STORED_NUM-1] = 0;
            gAgcMaxStored.times[type][STORED_NUM-1] = 0;
        }
    }
//    // sort the arrays before trying to insert the new value
//    if( type != 1 ){
//        qsort(gAgcMaxStored.adcValues[type], STORED_NUM, sizeof(uint32_t), cmpfunc2);
//    }else{
//        qsort(gAgcMaxStored.adcValues[type], STORED_NUM, sizeof(uint32_t), cmpfunc1);
//    }

    for(i=0;i<STORED_NUM;i++){
        if((gAgcMaxStored.dbValues[type][i]<=db) || (gAgcMaxStored.times[type][i] == 0)){
            gAgcMaxStored.dbValues[type][i] = db;
            gAgcMaxStored.times[type][i] = Seconds_get();
            break;
        }
    }


}

static bool Agc_getLock(){
#ifndef CLI_SENSOR
    CRS_retVal_t retVal = Tdd_isLocked();
    if( retVal == CRS_TDD_NOT_LOCKED){
        return false;
    }
    return true;
#else
    return gIsTDDLocked;
#endif
}


#ifdef CLI_SENSOR
CRS_retVal_t Agc_setLock(bool lock){
    scifTaskData.systemAgc.input.tddLock = lock;
    gIsTDDLocked = lock;
    return CRS_SUCCESS;
}
#endif
