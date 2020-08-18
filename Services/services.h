/*
 * Copyright (C) 2015  University of Alberta
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 /**
  * @file services.h
  * @author upSat, Andrew Rooney, Thomas Ganley
  * @date 2020-07-14
  */


#ifndef SERVICES_H
#define SERVICES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <csp/csp.h>
#include <unistd.h>
#include "subsystems_ids.h"
#include "system.h"

#define MAX_APP_ID 32     // number of CSP nodes (5-bits)
#define MAX_SERVICES 64   // number of CSP ports (6-bits)
#define MAX_SUBTYPES 256  // an 8-bit integer

#define MAX_PKT_LEN 210 // TODO: What is our max packet length?

typedef enum {
  OBC_APP_ID = _OBC_APP_ID_,
  EPS_APP_ID = _EPS_APP_ID_,
  ADCS_APP_ID = _ADCS_APP_ID_,
  COMMS_APP_ID = _COMMS_APP_ID_,
  GND_APP_ID = _GND_APP_ID_,
  DEMO_APP_ID = _DEMO_APP_ID_,
  LAST_APP_ID = _LAST_APP_ID_
} TC_TM_app_id;

typedef enum {
  SATR_PKT_ILLEGAL_APPID = 0,
  SATR_OK = 1,
  SATR_ERROR = 2,
  /* Scheduling Service Error State Codes */
  SATR_SCHS_FULL = 16,
  SATR_SCHS_INTRL_LGC_ERR = 27,
  /*LAST*/
  SATR_LAST = 56
} SAT_returnState;

/* Service types
 * Note: ports 0-7 are reserved by CSP
 */
#define TC_VERIFICATION_SERVICE 8
#define TC_HOUSEKEEPING_SERVICE 9
#define TC_EVENT_SERVICE 10
#define TC_FUNCTION_MANAGEMENT_SERVICE 11
#define TC_TIME_MANAGEMENT_SERVICE 12
#define TC_SCHEDULING_SERVICE 13

/* Subservice types */
#define TM_TIME_SET_IN_UTC 0

/*SCHEDULING SERVICE*/
#define SCHS_ENABLE_RELEASE             1 /*subservice 01, Telecommand to enable the release of telecommands from schedule pool*/
#define SCHS_DISABLE_RELEASE            2 /*subservice 02, Telecommand to disable the release of telecommands from schedule pool*/
#define SCHS_RESET_POOL                 3 /*subservice 03, Telecommand to reset the schedule pool*/
#define SCHS_INSERT_ELEMENT             4 /*subservice 04, Telecommand to insert a tc_tm_pkt in schedule pool*/
#define SCHS_REMOVE_ELEMENT             5 /*subservice 05, Telecommand to delete a tc_tm_pkt from schedule pool*/
#define SCHS_TIME_SHIFT_SEL             6 /*subservice 06, Telecommand to time shift (+/-) selected active schedule packet*/
#define SCHS_TIME_SHIFT_ALL             7 /*subservice 07, Telecommand to time shift (+/-) all schedule packets*/
#define SCHS_DETAILED_SCH_REPORT        8 /*subservice 08, Telemetry response (to subservice 10)*/
#define SCHS_SIMPLE_SCH_REPORT          9 /*subservice 09, Telemetry response (to subservice 11)*/
#define SCHS_REPORT_SCH_DETAILED        10 /*subservice 10, Telecommand to report schedules in detailed form*/
#define SCHS_REPORT_SCH_SUMMARY         11 /*subservice 11, Telecommand to report schedules in summary form*/
#define SCHS_LOAD_SCHEDULES             12 /*subservice 12, Telecommand to load schedules from perm storage*/
#define SCHS_SAVE_SCHEDULES             13 /*subservice 13, Telecommand to save schedules on perm storage*/



/* Utility definitions */
union _cnv {
  double cnvD;
  float cnvF;
  uint32_t cnv32;
  uint16_t cnv16[4];
  uint8_t cnv8[8];
};

#endif /* SERVICES_H */
