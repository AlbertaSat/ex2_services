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

#ifndef HOUSEKEEPING_SERVICE_H
#define HOUSEKEEPING_SERVICE_H

#include "services.h"

/* Housekeeping service address & port*/

#define HK_PARAMETERS_REPORT 25
//#define TM_HK_PARAMETERS_REPORT 21

#define HK_PR_ERR -1
#define HK_PR_OK 0

extern unsigned int count;

/*Define hk service tasks*/
SAT_returnState hk_service_app(csp_packet_t *pkt);
SAT_returnState tc_hk_param_rep(csp_packet_t *packet);
csp_packet_t* hk_param_rep();

/*hk data sample*/
typedef enum {
  EPS_CURRENT_VAL,
  EPS_CURRENT_STATE,
  EPS_VOLTAGE_VAL,
  EPS_VOLTAGE_STATE,
  EPS_TEMPERATURE_VAL,
  EPS_TEMPERATURE_STATE,
  EPS_ALERT,
  EPS_SIZE
}data_sample;

#endif /* HOUSEKEEPING_SERVICE_H */