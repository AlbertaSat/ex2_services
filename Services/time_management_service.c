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
 * @file time_management_service.c
 * @author Andrew Rooney
 * @date 2020-06-06
 */

#include <FreeRTOS.h>
#include <csp/csp.h>
#include <stdio.h>

#include "service_utilities.h"
#include "time_management_service.h"

/* Include the appropriate HAL files */
#if (SYSTEM_APP_ID == DEMO_APP_ID_)
#include "demo_hal.h"
#endif

SAT_returnState time_management_app(csp_packet_t *pkt) {
  uint8_t ser_subtype = (uint8_t)pkt->data[0];
  struct time_utc temp_time;

  switch (ser_subtype) {
    case SET_TIME:
      ex2_log("SET TIME\n");
      cnv8_32(&pkt->data[1], &temp_time.unix_timestamp);
      if (!TIMESTAMP_ISOK(temp_time.unix_timestamp)) {
        ex2_log("it's %d\n", temp_time.unix_timestamp);
        printf("Bad timestamp format\n");
        return SATR_ERROR;
      }
      ex2_log("Set Time: %d\n", temp_time.unix_timestamp);
      fflush(stdout);
      set_time_UTC(temp_time);
      break;

  default:
      ex2_log("No such subservice\n");
      break;
  }

  return SATR_OK;
}

/**
 * @brief
 * 		Set UTC time
 * @details
 * 		Makes a call the the platform implementation of the time
 * handlers
 * @param struct time_utc utc
 *    a valid UTC timestamp to set the RTC to
 */
void set_time_UTC(struct time_utc utc) { HAL_sys_setTime(utc.unix_timestamp); }
