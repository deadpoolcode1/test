#ifndef APPLICATION_AGC_AGC_H_
#define APPLICATION_AGC_AGC_H_





/*! Event ID - Calculate AGC Event */
#define AGC_EVT   0x0001

typedef struct {
    // 0-4 RfmaxRx, 5-8 RfmaxTx, 9-12 IfRx, 13-16 IfTx
    uint32_t adcResults [16];
} AGC_results_t;

typedef struct {
    char RfMaxRx [10];
    char RfMaxTx [10];
    char IfMaxRx [10];
    char IfMaxTx [10];
} AGC_max_results_t;

typedef struct {
    bool ADC_AnaSW_0_DIO26;
    bool ADC_AnaSW_1_DIO27;
} AGC_ctrlPins_t;

int Agc_isInitialized();
int Agc_isReady();
CRS_retVal_t Agc_init();
CRS_retVal_t Agc_sample();
CRS_retVal_t Agc_sample_debug();
AGC_results_t Agc_getResults();
AGC_max_results_t Agc_getMaxResults();
CRS_retVal_t Agc_setMode(int mode);
uint16_t Agc_getMode();
CRS_retVal_t Agc_setChannel(uint16_t channel);
uint16_t Agc_getChannel();
int Agc_convert(uint32_t voltage, int tx_rx, int rf_if);
//CRS_retVal_t Agc_getRxValue(int* value);
//CRS_retVal_t Agc_getTxValue(int* value);
CRS_retVal_t Agc_getValues(int* tx_value, int* rx_value);
CRS_retVal_t Agc_getControlPins(int mode, int channel, AGC_ctrlPins_t* pins);

#endif /* APPLICATION_AGC_AGC_H_ */
