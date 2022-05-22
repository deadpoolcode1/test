#ifndef APPLICATION_AGC_AGC_H_
#define APPLICATION_AGC_AGC_H_

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
/*! Event ID - Calculate AGC Event */
#define AGC_EVT   0x0001

typedef struct
{
    // 0-4 RfmaxRx, 5-8 RfmaxTx, 9-12 IfRx, 13-16 IfTx
    uint32_t adcResults[16];
} AGC_results_t;

typedef struct
{
    char RfMaxRx[10];
    char RfMaxTx[10];
    char IfMaxRx[10];
    char IfMaxTx[10];
} AGC_max_results_t;

typedef struct
{
    bool ADC_AnaSW_0_DIO26;
    bool ADC_AnaSW_1_DIO27;
} AGC_ctrlPins_t;
/******************************************************************************
 Function Prototypes
 *****************************************************************************/
CRS_retVal_t Agc_getValues(int *tx_value, int *rx_value);
CRS_retVal_t Agc_getControlPins(int mode, int channel, AGC_ctrlPins_t *pins);
CRS_retVal_t Agc_setChannel(uint16_t channel);
CRS_retVal_t Agc_init();
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

#endif /* APPLICATION_AGC_AGC_H_ */
