/** \mainpage Driver Overview
  *
  * \section section_drv_info Driver Information
  * This Sensor Controller Interface driver has been generated by the Texas Instruments Sensor Controller
  * Studio tool:
  * - <b>Project name</b>:     System AGC
  * - <b>Project file</b>:     C:/Users/cellium/workspace_v12/crs_tirtos_test/CDU_CC1352P_4_LAUNCHXL_tirtos7_ccs/scif/system_agc.scp
  * - <b>Code prefix</b>:      -
  * - <b>Operating system</b>: TI-RTOS
  * - <b>Tool version</b>:     2.9.0.208
  * - <b>Tool patches</b>:     None
  * - <b>Target chip</b>:      CC1352P1F3, package QFN48 7x7 RGZ, revision E (2.1) or F (3.0)
  * - <b>Created</b>:          2022-09-12 14:24:48.886
  * - <b>Computer</b>:         NIZAN-CELLIUM-L
  * - <b>User</b>:             cellium
  *
  * No user-provided resource definitions were used to generate this driver.
  *
  * No user-provided procedure definitions were used to generate this driver.
  *
  * Do not edit the generated source code files other than temporarily for debug purposes. Any
  * modifications will be overwritten by the Sensor Controller Studio when generating new output.
  *
  * \section section_drv_modules Driver Modules
  * The driver is divided into three modules:
  * - \ref module_scif_generic_interface, providing the API for:
  *     - Initializing and uninitializing the driver
  *     - Task control (for starting, stopping and executing Sensor Controller tasks)
  *     - Task data exchange (for producing input data to and consume output data from Sensor Controller
  *       tasks)
  * - \ref module_scif_driver_setup, containing:
  *     - The AUX RAM image (Sensor Controller code and data)
  *     - I/O mapping information
  *     - Task data structure information
  *     - Driver setup data, to be used in the driver initialization
  *     - Project-specific functionality
  * - \ref module_scif_osal, for flexible OS support:
  *     - Interfaces with the selected operating system
  *
  * It is possible to use output from multiple Sensor Controller Studio projects in one application. Only
  * one driver setup may be active at a time, but it is possible to switch between these setups. When
  * using this option, there is one instance of the \ref module_scif_generic_interface and
  * \ref module_scif_osal modules, and multiple instances of the \ref module_scif_driver_setup module.
  * This requires that:
  * - The outputs must be generated using the same version of Sensor Controller Studio
  * - The outputs must use the same operating system
  * - The outputs must use different source code prefixes (inserted into all globals of the
  *   \ref module_scif_driver_setup)
  *
  *
  * \section section_project_info Project Description
  * Samples 4 channels for TX/RX voltage, calculates min and max values of these measures for each
  * channel and raises AlertInterrupt for main application when done.
  * Sampling occurs only when TDD is locked and connected to the configured GPIO pins.
  *
  *
  * \subsection section_io_mapping I/O Mapping
  * Task I/O functions are mapped to the following pins:
  * - System AGC:
  *     - <b>A: Analog sensor output 0</b>: DIO24
  *     - <b>A: Analog sensor output 1</b>: DIO23
  *     - <b>A: Analog sensor output 2</b>: DIO25
  *     - <b>I: TDD lock pin</b>: DIO15
  *     - <b>I: TDD synchronization pin</b>: DIO14
  *     - <b>O: Channel control PINS 0</b>: DIO26
  *     - <b>O: Channel control PINS 1</b>: DIO27
  *     - <b>O: Controls the red LED</b>: DIO6
  *     - <b>O: Controls the green LED</b>: DIO7
  *
  *
  * \section section_task_info Task Description(s)
  * This driver supports the following task(s):
  *
  *
  * \subsection section_task_desc_system_agc System AGC
  * Samples 4 channels for TX/RX voltage, calculates min and max values of these measures for each
  * channel and raises AlertInterrupt for main application when done.
  * Sampling occurs only when TDD is locked and connected to the configured GPIO pins.
  *
  */




/** \addtogroup module_scif_driver_setup Driver Setup
  *
  * \section section_driver_setup_overview Overview
  *
  * This driver setup instance has been generated for:
  * - <b>Project name</b>:     System AGC
  * - <b>Code prefix</b>:      -
  *
  * The driver setup module contains the generated output from the Sensor Controller Studio project:
  * - Location of task control and scheduling data structures in AUX RAM
  * - The AUX RAM image, and the size the image
  * - Task data structure information (location, size and buffer count)
  * - I/O pin mapping translation table
  * - Task resource initialization and uninitialization functions
  * - Hooks for run-time logging
  *
  * @{
  */
#ifndef SCIF_H
#define SCIF_H

#include <stdint.h>
#include <stdbool.h>
#include "scif_framework.h"
#include "scif_osal_tirtos.h"


/// Target chip name
#define SCIF_TARGET_CHIP_NAME_CC1352P1F3
/// Target chip package
#define SCIF_TARGET_CHIP_PACKAGE_QFN48_7X7_RGZ

/// Number of tasks implemented by this driver
#define SCIF_TASK_COUNT 1

/// System AGC: Task ID
#define SCIF_SYSTEM_AGC_TASK_ID 0


/// System AGC: The size of the array of the sums of the samples results (for each channel sum of samples from DLRF, ULRF, DLIF, ULIF).
#define SCIF_SYSTEM_AGC_AVERAGE_SIZE 16
/// System AGC: Number of measures on each channel from one sample cycle (one DL/UL period)
#define SCIF_SYSTEM_AGC_BUFFER_SIZE 10
/// System AGC: Number of channels
#define SCIF_SYSTEM_AGC_CHANNELS_NUMBER 4
/// System AGC: 15% of results from all samples across cycles for a given channel
#define SCIF_SYSTEM_AGC_MIN_MAX_CHANNELS_SIZE 3
/// System AGC: 20% of results from all samples across cycles for a given channel
#define SCIF_SYSTEM_AGC_MODES_CHANNEL_SIZE 4
/// System AGC: 20% of sample across all cycles and channels
#define SCIF_SYSTEM_AGC_MODES_SIZE 16
/// System AGC: Number of samples from all channels in each sample cycle
#define SCIF_SYSTEM_AGC_MULTI_BUFFER_SIZE 40
/// System AGC: Total number of measures from all channels across all samples cycles
#define SCIF_SYSTEM_AGC_MULTI_RESULT_SIZE 80
/// System AGC: Number of measures for a channel across all sample cycles
#define SCIF_SYSTEM_AGC_RESULTS 20
/// System AGC: Number of sample cycles
#define SCIF_SYSTEM_AGC_SAMPLE_SIZE 2
/// System AGC I/O mapping: Analog sensor output
#define SCIF_SYSTEM_AGC_DIO_ASENSOR_OUTPUT { 24, 23, 25 }
/// System AGC I/O mapping: TDD lock pin
#define SCIF_SYSTEM_AGC_DIO_I_TDD_LOCK 15
/// System AGC I/O mapping: TDD synchronization pin
#define SCIF_SYSTEM_AGC_DIO_I_TDD_SYNCH 14
/// System AGC I/O mapping: Channel control PINS
#define SCIF_SYSTEM_AGC_DIO_OCHANNEL_CONTROL { 26, 27 }
/// System AGC I/O mapping: Controls the red LED
#define SCIF_SYSTEM_AGC_DIO_O_RED_LED 6
/// System AGC I/O mapping: Controls the green LED
#define SCIF_SYSTEM_AGC_DIO_O_GREEN_LED 7


// All shared data structures in AUX RAM need to be packed
#pragma pack(push, 2)


/// System AGC: Task configuration structure
typedef struct {
    uint16_t channelsSwitch;           ///< Channel select - 0: measure all channels, 1 - 4: measure only this channel
    uint16_t pAuxioASensorOutput[3];   ///< I/O mapping: Analog sensor output
    uint16_t pAuxioOChannelControl[2]; ///< I/O mapping: Channel control PINS
    uint16_t tddMode;                  ///< TDD mode - 0: ManualDL, 1: ManualUL, 2: Auto
    uint16_t unitType;                 ///< Type of unit - 0 : CDU, 1: CRU
} SCIF_SYSTEM_AGC_CFG_T;


/// System AGC: Task input data structure
typedef struct {
    uint16_t randomDelayDL[2]; ///< Random delay period after interrupt wakeup and before starting to perform measures(DL event). Contains multiple numbers for each sample cylce.
    uint16_t randomDelayUL[2]; ///< Random delay period after interrupt wakeup and before starting to perform measures(UL event). Contains multiple numbers for each sample cylce.
    uint16_t tddLock;          ///< TDD lock status
} SCIF_SYSTEM_AGC_INPUT_T;


/// System AGC: Task output data structure
typedef struct {
    uint16_t channelsAverage[16];        ///< Contains sums of 90% of results of measures for each channel and type of measure: [sumsChannel1-4DLRF, sumsChannel1-4ULRF, sumsChannel1-4DLIF,  sumsChannel1-4ULIF].
    uint16_t channelsAverageTemp[16];    ///< Stores the average sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMaxIFDL[4];         ///< Contains sums of the 15% max results from downlink IF of channels 1-4.
    uint16_t channelsMaxIFDLTemp[4];     ///< Stores the max sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMaxIFUL[4];         ///< Contains sums of the 15% max results from uplink IF of channels 1-4.
    uint16_t channelsMaxIFULTemp[4];     ///< Stores the max sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMaxRFDL[4];         ///< Contains sums of the 15% max results from downlink RF of channels 1-4.
    uint16_t channelsMaxRFDLTemp[4];     ///< Stores the max sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMaxRFUL[4];         ///< Contains sums of the 15% max results from uplink RF of channels 1-4.
    uint16_t channelsMaxRFULTemp[4];     ///< Stores the max sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMinIFDL[4];         ///< Contains sums of the 15% min results from downlink IF of channels 1-4.
    uint16_t channelsMinIFDLTemp[4];     ///< Stores the min sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMinIFUL[4];         ///< Contains sums of the 15% min results from uplink IF of channels 1-4.
    uint16_t channelsMinIFULTemp[4];     ///< Stores the min sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMinRFDL[4];         ///< Contains sums of the 15% min results from downlink RF of channels 1-4.
    uint16_t channelsMinRFDLTemp[4];     ///< Stores the min sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t channelsMinRFUL[4];         ///< Contains sums of the 15% min results from uplink RF of channels 1-4.
    uint16_t channelsMinRFULTemp[4];     ///< Stores the min sums between calculations, is not accessed by the CPU and copied into the non-temp array after calculation is done.
    uint16_t maxSamplesIFDL[16];         ///< Contains the 20% top reasults for each channel from all smaple cycles for IF downlink: [channel1MaxSamples,  channel2MaxSamples, channel3MaxSamples, channel4MaxSamples].
    uint16_t maxSamplesIFUL[16];         ///< Contains the 20% top reasults for each channel from all smaple cycles for IF uplink: [channel1MaxSamples,  channel2MaxSamples, channel3MaxSamples, channel4MaxSamples].
    uint16_t maxSamplesRFDL[16];         ///< Contains the 20% top reasults for each channel from all smaple cycles for RF downlink: [channel1MaxSamples,  channel2MaxSamples, channel3MaxSamples, channel4MaxSamples].
    uint16_t maxSamplesRFUL[16];         ///< Contains the 20% top reasults for each channel from all smaple cycles for RF uplink: [channel1MaxSamples,  channel2MaxSamples, channel3MaxSamples, channel4MaxSamples].
    uint16_t minSamplesIFDL[16];         ///< Contains the 20% bottom reasults for each channel from all smaple cycles for IF downlink [channel1MinSamples,  channel2MinSamples, channel3MinSamples, channel4MinSamples].
    uint16_t minSamplesIFUL[16];         ///< Contains the 20% bottom reasults for each channel from all smaple cycles for IF uplink [channel1MinSamples,  channel2MinSamples, channel3MinSamples, channel4MinSamples].
    uint16_t minSamplesRFDL[16];         ///< Contains the 20% bottom reasults for each channel from all smaple cycles for RF downlink [channel1MinSamples,  channel2MinSamples, channel3MinSamples, channel4MinSamples].
    uint16_t minSamplesRFUL[16];         ///< Contains the 20% bottom reasults for each channel from all smaple cycles for RF uplink [channel1MinSamples,  channel2MinSamples, channel3MinSamples, channel4MinSamples].
    uint16_t pSamplesMultiChannelIF[40]; ///< Stores IF samples of each sample cycle from each channel
    uint16_t pSamplesMultiChannelRF[40]; ///< Stores RF samples of each sample cycle from each channel
} SCIF_SYSTEM_AGC_OUTPUT_T;


/// System AGC: Task state structure
typedef struct {
    uint16_t alertEnabled;   ///< Is ALERT interrupt generation enabled?
    uint16_t channelsSwitch; ///< Channel select - 0: measure all channels, 1 - 4: measure only this channel (copied from cfg in Event A)
    uint16_t exit;           ///< Set to exit the ADC data streamer
    uint16_t invalid;        ///< signal that results of current sample cycle should be discarded and not copied to the results arrays.
    uint16_t ledCounter;     ///< Determines which LED will turn on in Auto mode
    uint16_t ledOn;          ///< bool variable to decide weather we light led or ont
    uint16_t samplesCount;   ///< The sample cycles counter
    uint16_t tddMode;        ///< TDD mode - 0: Auto, 1: ManualDL, 2: ManualUL  (copied from cfg in Event A)
    uint16_t unitType;       ///< Type of unit - 0 : CDU, 1: CRU  (copied from cfg in Event A)
} SCIF_SYSTEM_AGC_STATE_T;


/// Sensor Controller task data (configuration, input buffer(s), output buffer(s) and internal state)
typedef struct {
    struct {
        SCIF_SYSTEM_AGC_CFG_T cfg;
        SCIF_SYSTEM_AGC_INPUT_T input;
        SCIF_SYSTEM_AGC_OUTPUT_T output;
        SCIF_SYSTEM_AGC_STATE_T state;
    } systemAgc;
} SCIF_TASK_DATA_T;

/// Sensor Controller task generic control (located in AUX RAM)
#define scifTaskData    (*((volatile SCIF_TASK_DATA_T*) 0x400E0172))


// Initialized internal driver data, to be used in the call to \ref scifInit()
extern const SCIF_DATA_T scifDriverSetup;


// Restore previous struct packing setting
#pragma pack(pop)


// AUX I/O re-initialization functions
void scifReinitTaskIo(uint32_t bvTaskIds);


// No task-specific API available


#endif
//@}


// Generated by NIZAN-CELLIUM-L at 2022-09-12 14:24:48.886
