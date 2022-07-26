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
    uint32_t dbValues[SAMPLE_TYPES][STORED_NUM];
    int times[SAMPLE_TYPES][STORED_NUM];
} AGC_max_stored_t;

typedef struct
{
    bool ADC_AnaSW_0_DIO26;
    bool ADC_AnaSW_1_DIO27;
} AGC_ctrlPins_t;

/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Agc_getControlPins(int mode, int channel, AGC_ctrlPins_t *pins);
CRS_retVal_t Agc_setChannel(uint16_t channel);
CRS_retVal_t Agc_init(void * sem);
void Agc_process(void);

CRS_retVal_t Agc_sample();
CRS_retVal_t Agc_sample_debug();
void scTaskAlertCallback(void);
void scCtrlReadyCallback(void);
int Agc_isInitialized();
int Agc_isReady();
int Agc_convert(float voltage, int tx_rx, int rf_if);
AGC_results_t Agc_getResults();
AGC_max_results_t Agc_getMaxResults();
CRS_retVal_t Agc_setMode(int mode);
uint16_t Agc_getMode();
uint16_t Agc_getChannel();

#ifdef CLI_SENSOR
CRS_retVal_t Agc_setLock(bool lock);
#endif

#endif /* APPLICATION_AGC_AGC_H_ */
