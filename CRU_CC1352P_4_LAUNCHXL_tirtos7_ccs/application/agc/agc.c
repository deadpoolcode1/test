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
#include "application/crs/crs_env.h"
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
#include <ti/drivers/GPIO.h>

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

#define AGC_INTERVAL 1000  //time in milliseconds  for the main CPU to check the samples reported from the scif
#define AGC_HOLD 1
#define DC_RF_HIGH_FREQ_HB_RX 21
#define DC_IF_LOW_FREQ_TX 21
#define UC_RF_HIGH_FREQ_HB_TX 21
#define UC_IF_LOW_FREQ_RX 15
#define MAX_AVG_HOLD_ARRAY_SZ 32
#define AGC_GET_SAMPLE_EVT 0x0001

/******************************************************************************
 Local variables
 *****************************************************************************/
static AGC_results_t gAgcResults = {.adcMaxResults={0}, .adcMinResults={0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                                                                        0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff}, .adcAvgResults={0}};
static AGC_max_results_t gAgcMaxResults  = {.IfMaxDL="N/A", .IfMaxUL="N/A", .RfMaxDL="N/A", .RfMaxUL="N/A"};
static AGC_max_stored_t gAgcMaxStored = {.dbValues={0}, .times={0}};
static int gAgcInitialized =0;
static int gAgcReady=0;
static uint32_t gCurrentMaxResults [SAMPLE_TYPES] = {0xffffffff, 0x0, 0xffffffff, 0xffffffff};
static Clock_Params gClkParamsTimeMinMaxAvgInit;
static Clock_Handle gClkHandleTimeMinMaxAvgInit;
static Clock_Struct gClkStructTimeMinMaxAvgInit;


static Clock_Struct agcClkStruct;
static Clock_Handle agcClkHandle;
static void* gSem;
static uint32_t gTimeMinMaxAvgseconds=AGC_HOLD;
static uint16_t Agc_events = 0;
static AGC_sensorMode_t gAgcMode = AGC_AUTO;
static AGC_channels_t gAgcChannel = AGC_ALL_CHANNELS;
static adc_output_t outputValues[4][4]={0};
static adc_output_t outputValuesHold[4][4]={0};
static int32_t avgArrayHold[4][4][MAX_AVG_HOLD_ARRAY_SZ]={-1};
static uint32_t cntrAlerts=0;
#ifdef CLI_SENSOR
static uint16_t gCounterInc=0;
static uint16_t gCounterDec=0;
static bool gIsTDDLocked;
static bool gCruTDDLocked=0;
#endif

static void processAgcTimeoutCallback(UArg a0);
static void processAgcSample();
static void avgArray(int32_t* array, int32_t* result);
static void timeMinMaxAvgInitClockCb(xdc_UArg arg);
//From Sensor Controller: ALERT wakes up the System CPU if currently in standby mode, and generates a callback (interrupt)
void scTaskAlertCallback(void) {

    //Log_info0("callback");
    // Wake up the OS task
    //Semaphore_post(Semaphore_handle(&semScTaskAlert));
    //Util_setEvent(&AGC_events, AGC_EVT);
    if(scifTaskData.systemAgc.state.invalid != 1){
        gAgcReady = 1;
    }



} // taskAlertCallback

//To Sensor Controller: CTRL initiates a control operation that starts or stops Sensor Controller tasks
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
    // configure and start SC task
    if(gAgcInitialized){
        return CRS_SUCCESS;
    }
    gSem = sem;
    scifOsalInit(); //Construct Sensor Controller framework semaphore.
    scifOsalRegisterCtrlReadyCallback(scCtrlReadyCallback); //Interrupt handlers are registered. To Sensor Controller: CTRL initiates a control operation that starts or stops Sensor Controller tasks
    scifOsalRegisterTaskAlertCallback(scTaskAlertCallback); //Interrupt handlers are registered. From Sensor Controller: ALERT wakes up the System CPU if currently in standby mode, and generates a callback (interrupt)
    scifInit(&scifDriverSetup); //Load Sensor Controller program image to the Sensor Controller RAM and configure the complete driver setup which includes IO Mapping, Domain Clocks, control and data variables, HWI's and more. For more details refer to the source code.
    //the Sensor Controller wakes up every 125 ms, because  (0x00010000 / 8) = 8192. divide it by 65536(0x10000) you'll get 0.125 in seconds.
    //scifStartRtcTicksNow(0x00010000 / 8);
    int i;

    uint32_t randomNumber;
    TDD_tdArgs_t tdArgs = Tdd_getTdArgs();
    uint16_t period = tdArgs.period;
    uint16_t dl1 = tdArgs.dl1;
    // uint16_t period = 5000;
    // uint16_t dl1 = 3714;
    Random_seedAutomatic();
    for(i=0;i<SCIF_SYSTEM_AGC_SAMPLE_SIZE;i++){
        // generate random delays for both DL and UL to wait on in SC task.
        // In order to prevent overflow, the ranges are between 0 to DL/UL time minus 800ms.

        randomNumber = (Random_getNumber() % (period - dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayUL[i] = randomNumber;
        dl1 = 1286;
        randomNumber = (Random_getNumber() % (dl1 - 800));
        scifTaskData.systemAgc.input.randomDelayDL[i] = randomNumber;
    }
    // update cfg struct of sensor controller
    AGC_sensorMode_t mode = Agc_getMode();
    scifTaskData.systemAgc.cfg.tddMode = mode;

    // get AGC interval time from thresholds file
    char thrshFile[4096]={0};
    Thresh_read("AgcInterval", thrshFile);
    uint32_t agcInterval = strtol(thrshFile + strlen("AgcInterval="), NULL, 10);
    if((!agcInterval) || (agcInterval<AGC_INTERVAL)){
        agcInterval = AGC_INTERVAL;
    }
    // start task.
    scifStartTasksNbl(BV(SCIF_SYSTEM_AGC_TASK_ID)); //The Sensor Controller task is started and the initialization code will run once.
    agcClkHandle = UtilTimer_construct(&agcClkStruct, processAgcTimeoutCallback,
                                       agcInterval, 0, true, 0);

        Clock_Params_init(&gClkParamsTimeMinMaxAvgInit);
        gClkParamsTimeMinMaxAvgInit.period =  (15 * 100000);
        gClkParamsTimeMinMaxAvgInit.startFlag = FALSE;

        Clock_construct(&gClkStructTimeMinMaxAvgInit, NULL, 11000 / Clock_tickPeriod, &gClkParamsTimeMinMaxAvgInit);

        gClkHandleTimeMinMaxAvgInit = Clock_handle(&gClkStructTimeMinMaxAvgInit);

        Clock_setFunc(gClkHandleTimeMinMaxAvgInit, timeMinMaxAvgInitClockCb, 0);
        Clock_setTimeout(gClkHandleTimeMinMaxAvgInit, (AGC_HOLD * 100000));//
        Clock_start(gClkHandleTimeMinMaxAvgInit);

        gTimeMinMaxAvgseconds=AGC_HOLD;
    gAgcInitialized = 1;
    return CRS_SUCCESS;
}
static void timeMinMaxAvgInitClockCb(xdc_UArg arg){

int i,j;
for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
        outputValuesHold[i][j].min=UINT16_MAX;
        outputValuesHold[i][j].max=0;
        outputValuesHold[i][j].avg=0;
    }
}

}


void Agc_process(void)
{
    // Agc_process: get the samples, check for alarms, update interval.
    if (Agc_events & AGC_GET_SAMPLE_EVT){

//        printADCOutput();

       Agc_sample();
        char envFile[4096]={0};
        // update cfg struct for next sample
        scifTaskData.systemAgc.cfg.tddMode = Agc_getMode();
        scifTaskData.systemAgc.cfg.channelsSwitch = gAgcChannel;

        // this is the mode the sample we got was on
        AGC_sensorMode_t mode = (AGC_sensorMode_t)scifTaskData.systemAgc.state.tddMode;
        // check alarms
        if((mode!=AGC_AUTO) ||(mode==AGC_AUTO && Agc_getLock())){
            #ifndef CLI_SENSOR
                Thresh_read("DLMaxInputPower", envFile);
                int16_t dlMaxInputPower = strtol(envFile + strlen("DLMaxInputPower="),
                NULL, 10);
                if ((dlMaxInputPower < Agc_convert(gCurrentMaxResults[0], DL_RF, TYPE_CDU)) && (mode==AGC_DL||mode==AGC_AUTO) )
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
                if ((ulMaxOutputPower < Agc_convert(gCurrentMaxResults[1], UL_RF, TYPE_CDU)) && (mode==AGC_UL||mode==AGC_AUTO) )
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
                if ((ulMaxInputPower < Agc_convert(gCurrentMaxResults[1], UL_RF, TYPE_CRU)) && (mode==AGC_UL||mode==AGC_AUTO) )
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
                if ((dlMaxOutputPower < Agc_convert(gCurrentMaxResults[0], DL_RF, TYPE_CRU)) && (mode==AGC_DL||mode==AGC_AUTO) )
                {
                    Alarms_setAlarm(DLMaxOutputPower);
                }
                else
                {
                    Alarms_clearAlarm(DLMaxOutputPower, ALARM_INACTIVE);
                }
            #endif
        }else{
            #ifndef CLI_SENSOR
            Alarms_clearAlarm(DLMaxInputPower, ALARM_INACTIVE);
            Alarms_clearAlarm(ULMaxOutputPower, ALARM_INACTIVE);
            #else
            Alarms_clearAlarm(ULMaxInputPower, ALARM_INACTIVE);
            Alarms_clearAlarm(DLMaxOutputPower, ALARM_INACTIVE);
            #endif
        }
        // update interval
        memset(envFile, 0, 4096);
        Thresh_read("AgcInterval", envFile);
        uint32_t agcInterval = strtol(envFile + strlen("AgcInterval="), NULL, 10);
        if((!agcInterval) || (agcInterval<AGC_INTERVAL)){
            agcInterval = AGC_INTERVAL;
        }
        UtilTimer_setTimeout(agcClkHandle, agcInterval);
        UtilTimer_start(&agcClkStruct);
        AGCM_finishedTask();
#ifdef CLI_SENSOR
        if (gIsTDDLocked) {//if CDU sent a msg that it's locked, we will verify that the CRU is locked from the scif
            //if cruLock - gIsTDDLocked=lock
              if (scifTaskData.systemAgc.output.cruLock==0){
                  gCruTDDLocked=false;
              }else{
                  gCruTDDLocked=true;
              }
          }else{
              gCruTDDLocked=false;
          }
#endif

        /* Clear the event */
        Util_clearEvent(&Agc_events, AGC_GET_SAMPLE_EVT);

    }
}


int Agc_convert(float voltage, AGC_sampleType_t type, AGC_unitType_t unitType){
    // voltage- in microVolts.
    // type: 0 - RX_RF, 1 - TX_RF, 2 - RX_IF, 3 - TX_IF
    // unitType: 0 - CDU, 1- CRU
    // return: sensor value in dBm.
    int result = 0;
    int offset = 0;
    if(unitType){
        uint8_t temp = type;
        temp ^= 1;
        type = (AGC_sampleType_t)temp;
    }

    if (type == DL_RF)
    {
        // RF RX - RX_DET_An
        // tests the power that enters the RF/IF card (RSSI)
        // y = -0.023x + 0.2791 , R^2 = 0.9998
        offset = CRS_cbGainStates.dc_rf_high_freq_hb_rx - DC_RF_HIGH_FREQ_HB_RX;
        voltage = voltage - 279100;
        voltage = voltage / -23000;
        result = (int)(voltage) + offset;
    }
    else if(type == UL_RF)
    {
        // RF TX - TX_DET_An
        // power that comes out of FEM card
        // y = ln(x) * 10.094 - 121.61
        offset = CRS_cbGainStates.uc_rf_high_freq_hb_tx - UC_RF_HIGH_FREQ_HB_TX;
        voltage = log(voltage) * 10.094 - 121.61;
        result = (int)(voltage) + offset;
    }
    else if (type == DL_IF)
    {
        // IF RX - IF_DET_An_RX (DownLink)
        // tests the power coming out of the CAT5PA into the cable
        // y = -0.0187x + 0.5642 , R^2 = 0.9977
        offset = CRS_cbGainStates.uc_if_low_freq_rx - UC_IF_LOW_FREQ_RX;
        voltage = voltage - 564200;
        voltage = voltage / -18700;
        result = (int)(voltage) + offset;
    }
    else if(type == UL_IF)
    {
        // IF TX - IF_DET_An_TX (UpLink)
        // tests the power coming from the cable
        // y = -0.0219x + 0.5669 , R^2 = 0.9995
        offset = CRS_cbGainStates.dc_if_low_freq_tx - DC_IF_LOW_FREQ_TX;
        voltage = voltage - 566900;
        voltage = voltage/ -21900;
        result = (int)(voltage) + offset;
    }
    return result;
}




CRS_retVal_t Agc_setGap(uint8_t isStart,uint8_t isRising, uint16_t us){
if (isStart) {
    if (isRising==1) {
#ifdef CLI_SENSOR
        scifTaskData.systemAgc.input.gapStartTx=us;

#else
        scifTaskData.systemAgc.input.gapStartRx=us;
#endif
    }else{
#ifdef CLI_SENSOR
        scifTaskData.systemAgc.input.gapStartRx=us;

#else
        scifTaskData.systemAgc.input.gapStartTx=us;
#endif

    }
}else{
    if (isRising==1) {
#ifdef CLI_SENSOR
        scifTaskData.systemAgc.input.gapStopTx=us;

#else
         scifTaskData.systemAgc.input.gapStopRx=us;
#endif
     }else{
#ifdef CLI_SENSOR
        scifTaskData.systemAgc.input.gapStopRx=us;

#else
         scifTaskData.systemAgc.input.gapStopTx=us;
#endif
     }


}
return CRS_SUCCESS;
}

CRS_retVal_t Agc_getGap(uint8_t isStart,uint8_t isRising, uint16_t* result){
    if (isStart) {
        if (isRising==1) {
    #ifdef CLI_SENSOR
            * result=scifTaskData.systemAgc.input.gapStartTx;

    #else
            * result=scifTaskData.systemAgc.input.gapStartRx;
    #endif
        }else{
    #ifdef CLI_SENSOR
            * result=scifTaskData.systemAgc.input.gapStartRx;

    #else
            * result=scifTaskData.systemAgc.input.gapStartTx;
    #endif

        }
    }else{
        if (isRising==1) {
    #ifdef CLI_SENSOR
            * result=  scifTaskData.systemAgc.input.gapStopTx;

    #else
            * result= scifTaskData.systemAgc.input.gapStopRx;
    #endif
         }else{
    #ifdef CLI_SENSOR
             * result=scifTaskData.systemAgc.input.gapStopRx;

    #else
             * result= scifTaskData.systemAgc.input.gapStopTx;
    #endif
         }

    }
return CRS_SUCCESS;
}


CRS_retVal_t Agc_setTimeMinMax(uint32_t seconds){
    Clock_stop(gClkHandleTimeMinMaxAvgInit);
    Clock_setTimeout(gClkHandleTimeMinMaxAvgInit, seconds * 100000);
    Clock_start(gClkHandleTimeMinMaxAvgInit);
    gTimeMinMaxAvgseconds=seconds;
    return CRS_SUCCESS;
}

CRS_retVal_t Agc_getTimeMinMax(uint32_t* seconds){
    *seconds=gTimeMinMaxAvgseconds;
    return CRS_SUCCESS;
}
CRS_retVal_t Agc_setChannel(AGC_channels_t channel){
    scifTaskData.systemAgc.cfg.channelsSwitch = channel;
    gAgcChannel = channel;
    if(scifTaskData.systemAgc.cfg.channelsSwitch == channel){
        return CRS_SUCCESS;
    }
    return CRS_FAILURE;
}

CRS_retVal_t Agc_setDio(uint32_t dioIdx,uint8_t value){
    switch (dioIdx) {
            case 21:
                GPIO_init();
                GPIO_write(dioIdx, value);
                break;
        case 26:
            scifTaskData.systemAgc.cfg.Dio26= value;
            break;
        case 27:
            scifTaskData.systemAgc.cfg.Dio27= value;
            break;
        default:
            return CRS_FAILURE;
    }
return CRS_SUCCESS;
}


CRS_retVal_t Agc_readDio(uint32_t dioIdx){
    GPIO_init();
        CLI_cliPrintf("\r\nread dio %d : %d",dioIdx,GPIO_read(dioIdx));
            return CRS_SUCCESS;
}

AGC_channels_t Agc_getChannel(){
    return gAgcChannel;
}

CRS_retVal_t Agc_ledEnv(){
    char envFile[1024] = { 0 };
    CRS_retVal_t ret= Env_read("ledMode", envFile);
    if (ret == CRS_FAILURE) {
        Agc_ledMode(0x1);
        return CRS_SUCCESS;
    }
    uint16_t ledModeInt = strtol(envFile + strlen("ledMode="),NULL,16);
    scifTaskData.systemAgc.state.ledOn = ledModeInt;
    return CRS_SUCCESS;
}


CRS_retVal_t Agc_ledMode(uint16_t ledModeInt){
    scifTaskData.systemAgc.state.ledOn = ledModeInt;
    char ledMode[500]={0};
    sprintf(ledMode,"ledMode=%x",ledModeInt);
    Env_write(ledMode);
    return CRS_SUCCESS;
}
CRS_retVal_t Agc_ledOn(){
    scifTaskData.systemAgc.state.ledOn = 1;

    return CRS_SUCCESS;
}

CRS_retVal_t Agc_ledOff(){
    scifTaskData.systemAgc.state.ledOn = 0;
    return CRS_SUCCESS;
}


CRS_retVal_t Agc_setMode(AGC_sensorMode_t mode){
    // change tdd mode in thrsh file.
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
    scifTaskData.systemAgc.state.tddMode = mode;
    gAgcMode = mode;
    int i;
    for(i=0;i<SCIF_SYSTEM_AGC_AVERAGE_SIZE;i++){
        gAgcResults.adcMinResults[i] = 0xffffffff;
        gAgcResults.adcMaxResults[i] = 0;
        gAgcResults.adcAvgResults[i] = 0;
    }
    for(i=0;i<SCIF_SYSTEM_AGC_CHANNELS_NUMBER;i++){
        if(i!=UL_RF){
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

AGC_sensorMode_t Agc_getMode(){
//    CLI_cliPrintf("\r\nledCounter: %d",scifTaskData.systemAgc.state.ledCounter);
    char envFile[4096] = { 0 };
    Thresh_read("SensorMode", envFile);
    uint16_t tempMode = strtol(envFile + strlen("SensorMode="), NULL, 16);
    if (tempMode > AGC_UL){
        tempMode = AGC_UL;
    }
    AGC_sensorMode_t mode = (AGC_sensorMode_t)tempMode;
    gAgcMode = mode;
    //return scifTaskData.systemAgc.cfg.tddMode;
    return mode;
}



CRS_retVal_t Agc_sample(){

    if( (!gAgcReady) ){

        return CRS_FAILURE;
    }


    // Clear the ALERT interrupt source
        scifClearAlertIntSource();
        cntrAlerts++;
//        if ((!gAgcReady)) {
//return CRS_FAILURE;
//        }




        // values to correct ADC conversion offset
               int32_t adcOffset = AUXADCGetAdjustmentOffset(AUXADC_REF_FIXED);
               int32_t adcGainError = AUXADCGetAdjustmentGain(AUXADC_REF_FIXED);
               int32_t adcValue, adcCorrectedValue;

         uint16_t channelIdx=scifTaskData.systemAgc.output.channelsIdx;
        //pAccValueBuffer[0]-pAccValueBuffer[1]: Tx channel 0
        //pAccValueBuffer[2]-pAccValueBuffer[3]: Tx channel 1
        //pAccValueBuffer[4]-pAccValueBuffer[5]: Tx channel 2
        //pAccValueBuffer[6]-pAccValueBuffer[7]: Tx channel 3



       //pAccValueBuffer[8]-pAccValueBuffer[9]: Rx channel 0
       //pAccValueBuffer[10]-pAccValueBuffer[11]: Rx channel 1
       //pAccValueBuffer[12]-pAccValueBuffer[13]: Rx channel 2
       //pAccValueBuffer[14]-pAccValueBuffer[15]: Rx channel 3

        //pAccValueBuffer[16]-pAccValueBuffer[17]: IF Det DL channel 0
        //pAccValueBuffer[18]-pAccValueBuffer[19]: IF Det DL channel 1
        //pAccValueBuffer[20]-pAccValueBuffer[21]: IF Det DL channel 2
        //pAccValueBuffer[22]-pAccValueBuffer[23]: IF Det DL channel 3


        //pAccValueBuffer[24]-pAccValueBuffer[25]: IF Det UL channel 0
        //pAccValueBuffer[26]-pAccValueBuffer[27]: IF Det UL channel 1
        //pAccValueBuffer[28]-pAccValueBuffer[29]: IF Det UL channel 2
        //pAccValueBuffer[30]-pAccValueBuffer[31]: IF Det UL channel 3


        int32_t reportedVal=0;
        volatile uint32_t* pAccValues = (uint32_t*) scifTaskData.systemAgc.output.pAccValueBuffer;


       pAccValues+=channelIdx;
       int j=0;
       for ( j = 0;  j < 4; j++) {
           adcValue=*pAccValues;
           adcValue/=(SCIF_SYSTEM_AGC_SAMPLE_SZ);
           adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
           reportedVal= AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);

          avgArrayHold[j][channelIdx][cntrAlerts%gTimeMinMaxAvgseconds]=reportedVal;
          avgArray(avgArrayHold[j][channelIdx],&outputValues[j][channelIdx].avg);

           pAccValues+=4;
        }






    adcValue=scifTaskData.systemAgc.output.TxDetMax[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
    reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
    if ((reportedVal>=outputValuesHold[0][channelIdx].max) || (outputValuesHold[0][channelIdx].max==0)) {
        outputValues[0][channelIdx].max =reportedVal;
        outputValuesHold[0][channelIdx].max =reportedVal;
    }
    adcValue=scifTaskData.systemAgc.output.TxDetMin[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
    reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
     if ((reportedVal<=outputValuesHold[0][channelIdx].min) || (outputValuesHold[0][channelIdx].min==UINT16_MAX)) {
         outputValues[0][channelIdx].min =reportedVal;
         outputValuesHold[0][channelIdx].min =reportedVal;
    }


    adcValue=scifTaskData.systemAgc.output.RxDetMax[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
     reportedVal= AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
     if ((reportedVal>=outputValuesHold[1][channelIdx].max) || (outputValuesHold[1][channelIdx].max==0)) {
         outputValues[1][channelIdx].max =reportedVal;
         outputValuesHold[1][channelIdx].max =reportedVal;
         }

     adcValue=scifTaskData.systemAgc.output.RxDetMin[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
    reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
    if ((reportedVal<=outputValuesHold[1][channelIdx].min) || (outputValuesHold[1][channelIdx].min==UINT16_MAX ) ) {
        outputValues[1][channelIdx].min=reportedVal;
        outputValuesHold[1][channelIdx].min=reportedVal;

       }



    adcValue=scifTaskData.systemAgc.output.IFDetDLMax[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
    reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
    if ((reportedVal>=outputValuesHold[2][channelIdx].max) || (outputValuesHold[2][channelIdx].max==0)) {
        outputValues[2][channelIdx].max =reportedVal;
        outputValuesHold[2][channelIdx].max =reportedVal;
    }


    adcValue=scifTaskData.systemAgc.output.IFDetDLMin[channelIdx];
    adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
    reportedVal= AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
    if ((reportedVal<=outputValuesHold[2][channelIdx].min) || (outputValuesHold[2][channelIdx].min == UINT16_MAX)) {
        outputValues[2][channelIdx].min =reportedVal;
        outputValuesHold[2][channelIdx].min =reportedVal;

       }





     adcValue=scifTaskData.systemAgc.output.IFDetULMax[channelIdx];
     adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
     reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
    if ((reportedVal>= outputValuesHold[3][channelIdx].max) || (outputValuesHold[3][channelIdx].max==0)) {
        outputValues[3][channelIdx].max =reportedVal;
        outputValuesHold[3][channelIdx].max =reportedVal;
    }


     adcValue=scifTaskData.systemAgc.output.IFDetULMin[channelIdx];
     adcCorrectedValue = AUXADCAdjustValueForGainAndOffset((int32_t) adcValue, adcGainError, adcOffset);
     reportedVal=AUXADCValueToMicrovolts(AUXADC_FIXED_REF_VOLTAGE_NORMAL,adcCorrectedValue);
     if ((reportedVal<=outputValuesHold[3][channelIdx].min) || (outputValuesHold[3][channelIdx].min==UINT16_MAX)) {
         outputValues[3][channelIdx].min =reportedVal;
         outputValuesHold[3][channelIdx].min =reportedVal;

    }


     gAgcReady = 0;
     scifTaskData.systemAgc.input.canTouchTester=1;

     // Acknowledge the ALERT event. Note that there are no event flags for this task since the Sensor
        // Controller uses fwGenQuickAlertInterrupt(), but this function must be called nonetheless.
        scifAckAlertEvents();
        // re-enable ALERT generation (on buffer half full)
        scifTaskData.systemAgc.state.alertEnabled = 1;
        return CRS_SUCCESS;
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

static void avgArray(int32_t* array, int32_t* result){
   int i=0;
   int32_t sumCounter=0;

   int32_t sum=0;
 while(1){
    if(i==gTimeMinMaxAvgseconds){
        break;
    }
    if (array[i]!=-1) {
        sum+=array[i];
        sumCounter++;
    }
    i++;
 }
 *result= sum/sumCounter;
}

void printADCOutput(){




int channelIdx=0;
int detIdx=0; // 0: ADC0_DIO23_Tx_Det, 1 : ADC1_DIO24_Rx_Det, 2 : ADC2_DIO25_IF_Det_DL, 3: ADC2_DIO25_IF_Det_UL
for (detIdx = 0; detIdx < 4; detIdx++) {


    if ((gAgcMode==AGC_DL && detIdx==0) || (gAgcMode==AGC_DL && detIdx==3)) {
continue;
           }

    if ((gAgcMode==AGC_UL && detIdx==1) || (gAgcMode==AGC_UL && detIdx==2)) {
continue;
           }
    for (channelIdx = 0; channelIdx < 4; channelIdx++) {
#ifdef CLI_SENSOR
        if (Tdd_isLocked()==CRS_SUCCESS) {
#else
        if (Tdd_isLocked()!=CRS_SUCCESS) {
#endif
//      CLI_cliPrintf("\r\n*****");
      switch (detIdx) {
        case 0:
                  CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Average: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].avg);
                  CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Max: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].max);
                  CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Min: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].min);
            break;
        case 1:
                  CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Average: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].avg);
                  CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Max: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].max);
                  CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Min: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].min);
                  break;
        case 2:
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Average: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].avg);
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Max: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].max);
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Min: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].min);
                  break;
        case 3:
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Average: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].avg);
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Max: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].max);
                  CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Min: %u uV",(channelIdx+1),outputValues[detIdx][channelIdx].min);
                  break;
    }



    }else{
   switch (detIdx) {
        case 0:
        CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Average: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Max: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det channel %d Min: N\\C",(channelIdx+1));
        break;
        case 1:

        CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Average: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Max: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det channel %d Min: N\\C",(channelIdx+1));
        break;
        case 2:
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Average: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Max: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL channel %d Min: N\\C",(channelIdx+1));
        break;
        case 3:
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Average: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Max: N\\C",(channelIdx+1));
        CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL channel %d Min: N\\C",(channelIdx+1));
        break;
   }
         }
    }

}







//    CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det  Average: %u uV", (outputValues[0].avg));
//    CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det  Max: %u uV", outputValues[0].max);
//    CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det  Min: %u uV", outputValues[0].min);
////    CLI_cliPrintf("\r\nADC0_DIO23_Tx_Det  Total Samples: %u ", outputValues[0].totalSamples);
//    CLI_cliPrintf("\r\n*****");
//    CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det  Average: %u uV",  (outputValues[1].avg));
//    CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det  Max: %u uV", outputValues[1].max);
//    CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det  Min: %u uV", outputValues[1].min);
////    CLI_cliPrintf("\r\nADC1_DIO24_Rx_Det  Total Samples: %u ", outputValues[1].totalSamples);
//    CLI_cliPrintf("\r\n*****");
//
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL  Average: %u uV", (outputValues[2].avg));
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL  Max: %u uV", outputValues[2].max);
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_DL Min: %u uV", outputValues[2].min);
//    //    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det  Total Samples: %u ", outputValues[2].totalSamples);
//
//    CLI_cliPrintf("\r\n*****");
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL  Average: %u uV", (outputValues[3].avg));
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL  Max: %u uV", outputValues[3].max);
//    CLI_cliPrintf("\r\nADC2_DIO25_IF_Det_UL Min: %u uV", outputValues[3].min);
}

bool Agc_getLock(){
#ifndef CLI_SENSOR
    CRS_retVal_t retVal = Tdd_isLocked();
    if( retVal == CRS_TDD_NOT_LOCKED){
        return false;
    }
    return true;
#else

//    return gIsTDDLocked;
    return gCruTDDLocked;
#endif
}



#ifdef CLI_SENSOR
CRS_retVal_t Agc_setLock(bool lock){
    scifTaskData.systemAgc.input.tddLock = lock;
    gIsTDDLocked = lock;
    return CRS_SUCCESS;
}
#endif
