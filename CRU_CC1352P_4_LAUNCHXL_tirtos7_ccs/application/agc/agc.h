#ifndef APPLICATION_AGC_AGC_H_
#define APPLICATION_AGC_AGC_H_

#include "scif/scif.h"
/******************************************************************************
 Constants and definitions
 *****************************************************************************/
/*! Event ID - Calculate AGC Event */
#define AGC_EVT   0x0001
#define STORED_NUM 5
#define SAMPLE_TYPES 4

typedef enum AGC_sampleType{
    DL_RF,
    UL_RF,
    DL_IF,
    UL_IF
} AGC_sampleType_t;

typedef enum AGC_unitType{
    TYPE_CDU,
    TYPE_CRU
} AGC_unitType_t;

typedef enum AGC_sensorMode{
    AGC_AUTO,
    AGC_DL,
    AGC_UL
} AGC_sensorMode_t;

typedef enum AGC_channels{
    AGC_ALL_CHANNELS,
    AGC_CHANNEL_1,
    AGC_CHANNEL_2,
    AGC_CHANNEL_3,
    AGC_CHANNEL_4
} AGC_channels_t;

typedef struct
{
    // 0-4 RfRx, 5-8 RfxTx, 9-12 IfRx, 13-16 IfTx
    uint32_t adcMaxResults[SCIF_SYSTEM_AGC_AVERAGE_SIZE];
    uint32_t adcMinResults[SCIF_SYSTEM_AGC_AVERAGE_SIZE];
    uint32_t adcAvgResults[SCIF_SYSTEM_AGC_AVERAGE_SIZE];
} AGC_results_t;

typedef struct
{
    char RfMaxDL[10];
    char RfMaxUL[10];
    char IfMaxDL[10];
    char IfMaxUL[10];
} AGC_max_results_t;

typedef struct
{
    // rfDL, rfUL, ifDL, ifUL
    //uint32_t adcValues[4][STORED_NUM];
    int dbValues[SAMPLE_TYPES][STORED_NUM];
    int times[SAMPLE_TYPES][STORED_NUM];
} AGC_max_stored_t;

typedef struct
{
    bool ADC_AnaSW_0_DIO26;
    bool ADC_AnaSW_1_DIO27;
} AGC_ctrlPins_t;


typedef struct adc_output {
int32_t avg;
uint32_t max;
uint32_t min;
uint32_t totalSamples;
}adc_output_t;


/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Agc_getControlPins(AGC_sensorMode_t mode, AGC_channels_t channel, AGC_ctrlPins_t *pins);
CRS_retVal_t Agc_setChannel(AGC_channels_t channel);
CRS_retVal_t Agc_setDio(uint32_t dioIdx,uint8_t value);
CRS_retVal_t Agc_init(void * sem);
void Agc_process(void);
bool Agc_getLock();
CRS_retVal_t Agc_sample();
CRS_retVal_t Agc_sample_debug();
void scTaskAlertCallback(void);
void scCtrlReadyCallback(void);
int Agc_isInitialized();
int Agc_isReady();
int Agc_convert(float voltage, AGC_sampleType_t type, AGC_unitType_t unitType);
AGC_results_t Agc_getResults();
AGC_max_results_t Agc_getMaxResults();
CRS_retVal_t Agc_setMode(AGC_sensorMode_t mode);
AGC_sensorMode_t Agc_getMode();
AGC_channels_t Agc_getChannel();
CRS_retVal_t Agc_ledEnv();
CRS_retVal_t Agc_ledMode(uint16_t ledModeInt);
CRS_retVal_t Agc_ledOn();
CRS_retVal_t Agc_ledOff();
void printADCOutput();
void Agc_avgCalc(uint16_t* outputArray,int32_t* avg,uint8_t channelNum);
CRS_retVal_t Agc_setGap(uint8_t isStart,uint8_t detType, uint16_t ms);
CRS_retVal_t Agc_setTimeMinMax(uint16_t seconds);
#ifdef CLI_SENSOR
CRS_retVal_t Agc_setLock(bool lock);
#endif

#endif /* APPLICATION_AGC_AGC_H_ */
