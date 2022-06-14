/*
 * api_mac.h
 *
 *  Created on: 3 ????? 2022
 *      Author: cellium
 */

#ifndef MAC_API_MAC_H_
#define MAC_API_MAC_H_

/******************************************************************************
 Includes
 *****************************************************************************/
#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 Constants and definitions
 *****************************************************************************/
/*! IEEE Address Length */
#define APIMAC_SADDR_EXT_LEN 8

#define MAC_MLME_ASSOCIATE_IND      1     /* Associate indication */
#define MAC_MLME_DISASSOCIATE_IND   3     /* Disassociate indication */
#define MAC_MLME_DISCOVERY_IND      4     /* Disassociate indication */
#define MAC_MCPS_DATA_CNF           12    /* Data confirm */
#define MAC_MCPS_DATA_IND           13    /* Data indication */
/*!
 Address types - used to set addrMode field of the ApiMac_sAddr_t structure.
 */
typedef enum
{
    /*! Address not present */
    ApiMac_addrType_none = 0,
    /*! Short Address (16 bits) */
    ApiMac_addrType_short = 2,
    /*! Extended Address (64 bits) */
    ApiMac_addrType_extended = 3
} ApiMac_addrType_t;

/*! General MAC Status values */
typedef enum
{
    /*! Operation successful */
    ApiMac_status_success = 0,
    /*! MAC Co-Processor only - Subsystem Error */
    ApiMac_status_subSystemError = 0x25,
    /*! MAC Co-Processor only - Command ID error */
    ApiMac_status_commandIDError = 0x26,
    /*! MAC Co-Processor only - Length error */
    ApiMac_status_lengthError = 0x27,
    /*! MAC Co-Processor only - Unsupported Extended Type */
    ApiMac_status_unsupportedType = 0x28,
    /*! The AUTOPEND pending all is turned on */
    ApiMac_status_autoAckPendingAllOn = 0xFE,
    /*! The AUTOPEND pending all is turned off */
    ApiMac_status_autoAckPendingAllOff = 0xFF,
    /*! The beacon was lost following a synchronization request */
    ApiMac_status_beaconLoss = 0xE0,
    /*!
     The operation or data request failed because of activity on the channel
     */
    ApiMac_status_channelAccessFailure = 0xE1,
    /*!
     The frame counter puportedly applied by the originator of the received
     frame is invalid
     */
    ApiMac_status_counterError = 0xDB,
    /*! The MAC was not able to enter low power mode */
    ApiMac_status_denied = 0xE2,
    /*! Unused */
    ApiMac_status_disabledTrxFailure = 0xE3,
    /*!
     The received frame or frame resulting from an operation
     or data request is too long to be processed by the MAC
     */
    ApiMac_status_frameTooLong = 0xE5,
    /*!
     The key purportedly applied by the originator of the
     received frame is not allowed
     */
    ApiMac_status_improperKeyType = 0xDC,
    /*!
     The security level purportedly applied by the originator of
     the received frame does not meet the minimum security level
     */
    ApiMac_status_improperSecurityLevel = 0xDD,
    /*!
     The data request failed because neither the source address nor
     destination address parameters were present
     */
    ApiMac_status_invalidAddress = 0xF5,
    /*! Unused */
    ApiMac_status_invalidGts = 0xE6,
    /*! The purge request contained an invalid handle */
    ApiMac_status_invalidHandle = 0xE7,
    /*! Unused */
    ApiMac_status_invalidIndex = 0xF9,
    /*! The API function parameter is out of range */
    ApiMac_status_invalidParameter = 0xE8,
    /*!
     The scan terminated because the PAN descriptor storage limit
     was reached
     */
    ApiMac_status_limitReached = 0xFA,
    /*!
     The operation or data request failed because no
     acknowledgement was received
     */
    ApiMac_status_noAck = 0xE9,
    /*!
     The scan request failed because no beacons were received or the
     orphan scan failed because no coordinator realignment was received
     */
    ApiMac_status_noBeacon = 0xEA,
    /*!
     The associate request failed because no associate response was received
     or the poll request did not return any data
     */
    ApiMac_status_noData = 0xEB,
    /*! The short address parameter of the start request was invalid */
    ApiMac_status_noShortAddress = 0xEC,
    /*! Unused */
    ApiMac_status_onTimeTooLong = 0xF6,
    /*! Unused */
    ApiMac_status_outOfCap = 0xED,
    /*!
     A PAN identifier conflict has been detected and
     communicated to the PAN coordinator
     */
    ApiMac_status_panIdConflict = 0xEE,
    /*! Unused */
    ApiMac_status_pastTime = 0xF7,
    /*! A set request was issued with a read-only identifier */
    ApiMac_status_readOnly = 0xFB,
    /*! A coordinator realignment command has been received */
    ApiMac_status_realignment = 0xEF,
    /*! The scan request failed because a scan is already in progress */
    ApiMac_status_scanInProgress = 0xFC,
    /*! Cryptographic processing of the received secure frame failed */
    ApiMac_status_securityError = 0xE4,
    /*! The beacon start time overlapped the coordinator transmission time */
    ApiMac_status_superframeOverlap = 0xFD,
    /*!
     The start request failed because the device is not tracking
     the beacon of its coordinator
     */
    ApiMac_status_trackingOff = 0xF8,
    /*!
     The associate response, disassociate request, or indirect
     data transmission failed because the peer device did not respond
     before the transaction expired or was purged
     */
    ApiMac_status_transactionExpired = 0xF0,
    /*! The request failed because MAC data buffers are full */
    ApiMac_status_transactionOverflow = 0xF1,
    /*! Unused */
    ApiMac_status_txActive = 0xF2,
    /*!
     The operation or data request failed because the
     security key is not available
     */
    ApiMac_status_unavailableKey = 0xF3,
    /*! The set or get request failed because the attribute is not supported */
    ApiMac_status_unsupportedAttribute = 0xF4,
    /*!
     The received frame was secured with legacy security which is
     not supported
     */
    ApiMac_status_unsupportedLegacy = 0xDE,
    /*! The security of the received frame is not supported */
    ApiMac_status_unsupportedSecurity = 0xDF,
    /*! The operation is not supported in the current configuration */
    ApiMac_status_unsupported = 0x18,
    /*! The operation could not be performed in the current state */
    ApiMac_status_badState = 0x19,
    /*!
     The operation could not be completed because no
     memory resources were available
     */
    ApiMac_status_noResources = 0x1A,
    /*! For internal use only */
    ApiMac_status_ackPending = 0x1B,
    /*! For internal use only */
    ApiMac_status_noTime = 0x1C,
    /*! For internal use only */
    ApiMac_status_txAborted = 0x1D,

} ApiMac_status_t;

/******************************************************************************
 Structures - Building blocks for the API for the MAC Module
 *****************************************************************************/
/*! Extended address */
typedef uint8_t ApiMac_sAddrExt_t[APIMAC_SADDR_EXT_LEN];

/*! MAC address type field structure */
typedef struct
{
    /*!
     The address can be either a long address or a short address depending
     the addrMode field.
     */
    union
    {
        /*! 16 bit address */
        uint16_t shortAddr;
        /*! Extended address */
        ApiMac_sAddrExt_t extAddr;
    } addr;

    /*! Address type/mode */
    ApiMac_addrType_t addrMode;
} ApiMac_sAddr_t;

/*! Data buffer structure */
typedef struct _apimac_sdata
{
    /*! pointer to the data buffer */
    uint8_t *p;
    /*! length of the data buffer */
    uint16_t len;
} ApiMac_sData_t;

/*! MCPS data request type */
typedef struct _apimac_mcpsdatareq
{
    /*! The address of the destination device */
    ApiMac_sAddr_t dstAddr;
    /*! The source address mode */
    ApiMac_addrType_t srcAddrMode;
    /*! Application-defined handle value associated with this data request */
    uint8_t msduHandle;
    /*! Data buffer */
    ApiMac_sData_t msdu;
} ApiMac_mcpsDataReq_t;

/*! MAC_MLME_START_CNF type */
typedef struct _apimac_mlmestartcnf
{
    /*! status of the  start request */
    ApiMac_status_t status;
} ApiMac_mlmeStartCnf_t;

/*! MCPS data confirm type */
typedef struct _apimac_mcpsdatacnf
{
    /*! Contains the status of the data request operation */
    ApiMac_status_t status;
    /*! Application-defined handle value associated with the data request */
    uint8_t msduHandle;
    /*! The time, in backoffs, at which the frame was transmitted */
    uint32_t timestamp;
    /*!
     The time, in internal MAC timer units, at which the frame was
     transmitted
     */
    uint16_t timestamp2;
    /*! The number of retries required to transmit the data frame */
    uint8_t retries;
    /*! The link quality of the received ack frame */
    uint8_t mpduLinkQuality;
    /*! The raw correlation value of the received ack frame */
    uint8_t correlation;
    /*! The RF power of the received ack frame in units dBm */
    int8_t rssi;
    /*! Frame counter value used (if any) for the transmitted frame */
    uint32_t frameCntr;
} ApiMac_mcpsDataCnf_t;

/*! MCPS data indication type */
typedef struct _apimac_mcpsdataind
{
    /*! The address of the sending device */
    ApiMac_sAddr_t srcAddr;
    /*! The address of the destination device */
    ApiMac_sAddr_t dstAddr;
    /*! The time, in backoffs, at which the data were received */
    uint32_t timestamp;
    /*!
     The time, in internal MAC timer units, at which the data were received
     */
    uint16_t timestamp2;
    /*! The PAN ID of the sending device */
    uint16_t srcPanId;
    /*! The PAN ID of the destination device */
    uint16_t dstPanId;
    /*! The link quality of the received data frame */
    uint8_t mpduLinkQuality;
    /*! The raw correlation value of the received data frame */
    uint8_t correlation;
    /*! The received RF power in units dBm */
    int8_t rssi;
    /*! The data sequence number of the received frame */
    uint8_t dsn;
    /*! length of the payload IE buffer (pPayloadIE) */
    uint16_t payloadIeLen;
    /*! Pointer to the start of payload IEs */
    uint8_t *pPayloadIE;

    /*! Frame counter value of the received data frame (if used) */
    uint32_t frameCntr;

    /*! Data Buffer */
    ApiMac_sData_t msdu;
} ApiMac_mcpsDataInd_t;

/*! MAC_MLME_ASSOCIATE_IND type */
typedef struct _apimac_mlmeassociateind
{
    /*! The address of the device requesting association */
    ApiMac_sAddrExt_t deviceAddress;
    uint16_t shortAddr;
} ApiMac_mlmeAssociateInd_t;

/*! MAC_MLME_DISASSOCIATE_IND type */
typedef struct _apimac_mlmedisassociateind
{
    /*! The address of the device sending the disassociate command */
    ApiMac_sAddrExt_t deviceAddress;
    uint16_t shortAddr;
} ApiMac_mlmeDisassociateInd_t;

/*! MAC_MLME_ASSOCIATE_IND type */
typedef struct _apimac_mlmediscoveryind
{
    /*! The address of the device requesting association */
    ApiMac_sAddrExt_t deviceAddress;
    uint16_t shortAddr;
    bool isLocked;
    int8_t rssi;
    struct
    {
        int8_t rssiMax;
        int8_t rssiMin;
        int8_t rssiAvg;
        int8_t rssiLast;
    } rssiRemote;
} ApiMac_mlmeDiscoveryInd_t;

/* Extended address */
typedef uint8_t sAddrExt_t[8];

/* Combined short/extended device address */
typedef struct
{
    union
    {
        uint16_t shortAddr; /* Short address */
        sAddrExt_t extAddr; /* Extended address */
    } addr;
    uint8_t addrMode; /* Address mode */
} sAddr_t;

typedef struct
{
    uint8_t *p;
    uint16_t len;
} sData_t;

/* Data request parameters type */
typedef struct
{
    sAddr_t dstAddr; /* The address of the destination device */
    uint16_t dstPanId; /* The PAN ID of the destination device */
    uint8_t srcAddrMode; /* The source address mode */
    uint8_t msduHandle; /* Application-defined handle value associated with this data request */
    uint16_t txOptions; /* TX options bit mask */
    uint8_t channel; /* Transmit the data frame on this channel */
    uint8_t power; /* Transmit the data frame at this power level */
    uint8_t *pIEList; /* pointer to the payload IE list, excluding termination IEs */
    uint16_t payloadIELen; /* length of the payload IE�s */
    uint8_t fhProtoDispatch; /* Not Used, RESERVED for future. Shall be set to zero */
    uint32_t includeFhIEs; /* Bitmap indicates which FH IE's need to be included */
} macDataReq_t;

/* MAC event header type */
typedef struct
{
    uint8_t event; /* MAC event */
    uint8_t status; /* MAC status */
} macEventHdr_t;

/* MCPS data request type */
typedef struct
{
    macEventHdr_t hdr; /* Internal use only */
    sData_t msdu; /* Data pointer and length */
    macDataReq_t mac; /* Data request parameters */
} macMcpsDataReq_t;

/* MAC_MLME_START_CNF type */
typedef struct
{
    macEventHdr_t hdr; /* Event header contains the status of the start request */
} macMlmeStartCnf_t;

/* MCPS data confirm type */
typedef struct
{
    macEventHdr_t hdr; /* Contains the status of the data request operation */
    uint8_t msduHandle; /* Application-defined handle value associated with the data request */
    macMcpsDataReq_t *pDataReq; /* Pointer to the data request buffer for this data confirm */
    uint32_t timestamp; /* The time, in backoffs, at which the frame was transmitted */
    uint16_t timestamp2; /* The time, in internal MAC timer units, at which the
     frame was transmitted */
    uint8_t retries; /* The number of retries required to transmit the data frame */
    uint8_t mpduLinkQuality; /* The link quality of the received ack frame */
    uint8_t correlation; /* The raw correlation value of the received ack frame */
    int8_t rssi; /* The RF power of the received ack frame in units dBm */
    uint32_t frameCntr; /* Frame counter value used (if any) for the transmitted frame */
} macMcpsDataCnf_t;

/* Data indication parameters type */
typedef struct
{
    sAddr_t srcAddr; /* The address of the sending device */
    sAddr_t dstAddr; /* The address of the destination device */
    uint32_t timestamp; /* The time, in backoffs, at which the data were received */
    uint16_t timestamp2; /* The time, in internal MAC timer units, at which the
     data were received */
    uint16_t srcPanId; /* The PAN ID of the sending device */
    uint16_t dstPanId; /* The PAN ID of the destination device */
    uint8_t mpduLinkQuality; /* The link quality of the received data frame */
    uint8_t correlation; /* The raw correlation value of the received data frame */
    int8_t rssi; /* The received RF power in units dBm */
    uint8_t dsn; /* The data sequence number of the received frame */
    uint8_t *pPayloadIE; /* Pointer to the start of payload IE's */
    uint16_t payloadIeLen; /* length of payload ie if any */
    uint8_t fhProtoDispatch; /* Not Used, RESERVED for future. */
    uint32_t frameCntr; /* Frame counter value of the received data frame (if used) */
} macDataInd_t;

/* MCPS data indication type */
typedef struct
{
    macEventHdr_t hdr; /* Internal use only */
    sData_t msdu; /* Data pointer and length */
    macDataInd_t mac; /* Data indication parameters */
} macMcpsDataInd_t;

/* MAC_MLME_ASSOCIATE_IND type */
typedef struct
{
    macEventHdr_t hdr; /* The event header */
    sAddrExt_t deviceAddress; /* The address of the device requesting association */
    uint16_t shortAddr;
} macMlmeAssociateInd_t;

/* MAC_MLME_DISASSOCIATE_IND type */
typedef struct
{
    macEventHdr_t hdr; /* The event header */
    sAddrExt_t deviceAddress; /* The address of the device sending the disassociate command */
    uint16_t shortAddr;
} macMlmeDisassociateInd_t;

/* MAC_MLME_DISCOVERY_IND type */
typedef struct
{
    macEventHdr_t hdr; /* The event header */
    sAddrExt_t deviceAddress; /* The address of the device sending the disassociate command */
    uint16_t shortAddr;
    bool isLocked;
    int8_t rssi; /* The received RF power in units dBm */
    struct
    {
        int8_t rssiMax;
        int8_t rssiMin;
        int8_t rssiAvg;
        int8_t rssiLast;
    } rssiRemote;
} macMlmeDiscoveryInd_t;

/* Union of callback structures */
typedef union
{
    macEventHdr_t hdr;
    macMlmeAssociateInd_t associateInd; /* MAC_MLME_ASSOCIATE_IND */
    macMlmeDisassociateInd_t disassociateInd; /* MAC_MLME_DISASSOCIATE_IND */
    macMlmeStartCnf_t startCnf; /* MAC_MLME_START_CNF */
    macMcpsDataCnf_t dataCnf; /* MAC_MCPS_DATA_CNF */
    macMcpsDataInd_t dataInd; /* MAC_MCPS_DATA_IND */
    macMlmeDiscoveryInd_t discoveryInd;
} macCbackEvent_t;

/*!
 Disassociate Indication Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_disassociateIndFp_t)(
        ApiMac_mlmeDisassociateInd_t *pDisassociateInd);

/*!
 Associate Indication Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_associateIndFp_t)(ApiMac_mlmeAssociateInd_t *pAssocInd);

/*!
 Associate Indication Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_discoveryIndFp_t)(ApiMac_mlmeDiscoveryInd_t *pDiscInd);

/*!
 Start Confirmation Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_startCnfFp_t)(ApiMac_mlmeStartCnf_t *pStartCnf);

/*!
 Data Confirmation Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_dataCnfFp_t)(ApiMac_mcpsDataCnf_t *pDataCnf);

/*!
 Data Indication Callback function pointer prototype
 for the [callback table](@ref ApiMac_callbacks_t)
 */
typedef void (*ApiMac_dataIndFp_t)(ApiMac_mcpsDataInd_t *pDataInd);

/*!
 Structure containing all the MAC callbacks (indications).
 To receive the confirmation or indication fill in the
 associated callback with a pointer to the function that
 will handle that callback.  To ignore a callback
 set that function pointer to NULL.
 */
typedef struct _apimac_callbacks
{
    /*! Associate Indicated callback */
    ApiMac_associateIndFp_t pAssocIndCb;
    /*! Disassociate Indication callback */
    ApiMac_disassociateIndFp_t pDisassociateIndCb;

    ApiMac_discoveryIndFp_t pDiscIndCb;

    /*! Start Confirmation callback */
    ApiMac_startCnfFp_t pStartCnfCb;
    /*! Data Confirmation callback */
    ApiMac_dataCnfFp_t pDataCnfCb;
    /*! Data Indication callback */
    ApiMac_dataIndFp_t pDataIndCb;
} ApiMac_callbacks_t;

/*! Device Descriptor */
typedef struct _apimac_devicedescriptor
{
    /*! The 16-bit PAN identifier of the device */
    uint16_t panID;
    /*! The 16-bit short address of the device */
    uint16_t shortAddress;
    /*!
     The 64-bit IEEE extended address of the device. This element is also
     used inunsecuring operations on incoming frames.
     */
    ApiMac_sAddrExt_t extAddress;
} ApiMac_deviceDescriptor_t;

/******************************************************************************
 Function Prototypes
 *****************************************************************************/

/*!
 * @brief       Initialize this module.
 *
 * @param       macTaskId - The MAC Task ID to send messages to.
 * @param       enableFH - true to enable frequency hopping, false to not.
 *
 * @return      pointer to a wakeup variable (semaphore in some systems)
 */
extern void* ApiMac_init();

/*!
 * @brief       Register for MAC callbacks.
 *
 * @param       pCallbacks - pointer to callback structure
 */
extern void ApiMac_registerCallbacks(ApiMac_callbacks_t *pCallbacks);

/*!
 * @brief       Process incoming messages from the MAC stack.
 */
extern void ApiMac_processIncoming(void);

/*!
 * @brief       This function sends application data to the MAC for
 *              transmission in a MAC data frame.
 *              <BR>
 *              The MAC can only buffer a certain number of data request
 *              frames.  When the MAC is congested and cannot accept the data
 *              request it will initiate a callback ([ApiMac_dataCnfFp_t]
 *              (@ref ApiMac_dataCnfFp_t)) with
 *              an overflow status ([ApiMac_status_transactionOverflow]
 *              (@ref ApiMac_status_t)) .  Eventually the MAC will become
 *              uncongested and initiate the callback ([ApiMac_dataCnfFp_t]
 *              (@ref ApiMac_dataCnfFp_t)) for
 *              a buffered request.  At this point the application can attempt
 *              another data request.  Using this scheme, the application can
 *              send data whenever it wants but it must queue data to be resent
 *              if it receives an overflow status.
 *
 * @param       pData - pointer to parameter structure
 *
 * @return      The status of the request, as follows:<BR>
 *              [ApiMac_status_success](@ref ApiMac_status_success)
 *               - Operation successful<BR>
 *              [ApiMac_status_noResources]
 *              (@ref ApiMac_status_noResources) - Resources not available
 */
extern ApiMac_status_t ApiMac_mcpsDataReq(ApiMac_mcpsDataReq_t *pData);

#endif /* MAC_API_MAC_H_ */
