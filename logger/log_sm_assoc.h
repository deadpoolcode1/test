/*
 * log_sm_assoc.h
 *
 *  Created on: Feb 20, 2023
 *      Author: yardenr
 */

#ifndef APPLICATION_CRS_LOGGER_LOG_SM_ASSOC_H_
#define APPLICATION_CRS_LOGGER_LOG_SM_ASSOC_H_

#include "logger/crs_logger.h"

#define LOG_SMASSOC_NEW_NODE(ADDR) LOG_5(CRS_DEBUG, LogMsg_NEWNODE,ADDR, "newNode")
#define LOG_SMASSOC_ERASE_NODE(ADDR) LOG_5(CRS_DEBUG, LogMsg_ERASENODE,ADDR, "delNode")
#define LOG_SMASSOC_RECVD_BEACON(ADDR) LOG_5(CRS_DEBUG, LogMsg_RECEIVEDBEACON, ADDR, "rcvdBeacon")
#define LOG_SMASSOC_ASSOC_TIMEOUT LOG_1(CRS_WARN, LogMsg_ASSOCREQTIMEOUT, "assocTimeout")
#define LOG_SMASSOC_ASSOC_SENT_REQ_MSG(ADDR) LOG_5(CRS_DEBUG, LogMsg_SENTASSOCREQMSG,ADDR, "sntAscReqMsg")
#define LOG_SMASSOC_RECVD_ASSOC_RSP_MSG(ADDR) LOG_5(CRS_DEBUG, LogMsg_RECEIVEDASSOCRSPMSG,ADDR, "rcvdAscRspMsg")


#endif /* APPLICATION_CRS_LOGGER_LOG_SM_ASSOC_H_ */
