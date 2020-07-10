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
 * @file housekeeping_service.c
 * @author Haoran Qi, Andrew Rooney
 * @date 2020-07-07
 */
#include "housekeeping_service.h"

#include <FreeRTOS.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "service_response.h"
#include "service_utilities.h"
#include "services.h"
#include "system.h"

extern Service_Queues_t service_queues;
//Define the # of data collecting amount 
unsigned int count = 0;



SAT_returnState hk_service_app(csp_packet_t *pkt) {
  uint8_t ser_subtype = (uint8_t)pkt->data[0];
  portBASE_TYPE err;
  switch (ser_subtype) {
    case HK_PARAMETERS_REPORT:
	  err = tc_hk_param_rep(pkt);
      if (err != SATR_ERROR) {
        csp_log_info("HK_REPORT_PARAMETERS TASK FINISHED");
		return SATR_ERROR;
			}
      ex2_log("Starting Service Response Task\n");
      service_response_task();  
      break;

    default:
      csp_log_error("HK SERVICE NOT FOUND SUBTASK");
      csp_buffer_free(pkt);
      return SATR_ERROR;
  }

  return SATR_OK;
}

/* NB: Basically hk_para_rep will be wrriten in the hardware/platform file.*/

csp_packet_t* hk_param_rep(){
	csp_packet_t *packet = csp_buffer_get(100);
	if (packet == NULL) {
		/* Could not get buffer element */
		ex2_log("Failed to get CSP buffer");
		csp_buffer_free(packet);
		return NULL;
	}
// 	snex2_log((char *) packet->data[1], csp_buffer_data_size(), "HK
//  		   Data Sample -- EPS CRRENT: 23mA", ++count);	
	packet->data[1] = 16;
	++count;
	//tranfer the task from TC to TM for enabling ground response task
	packet->data[0] = TM_HK_PARAMETERS_REPORT;
	packet->length = (strlen((char *) packet->data) + 1);

	return packet;
}

SAT_returnState tc_hk_param_rep(csp_packet_t* packet){
  //execute #25 subtask: parameter report, collecting data from platform
  	packet = hk_param_rep();
    if(packet == NULL){
    	ex2_log("HOUSEKEEPING SERVICE REPORT: DATA COLLECTING FAILED");
    	return SATR_ERROR;
  	}
    if (xQueueSendToBack(service_queues.response_queue, packet, 
						 NORMAL_TICKS_TO_WAIT) != pdPASS) {
    	return SATR_ERROR;
  	}

    return SATR_OK;
}

