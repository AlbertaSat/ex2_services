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
 * @file
 * @author
 * @date
 */

#include "dfgm/dfgm_service.h"

#include <FreeRTOS.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <main/system.h>

#include "dfgm.h" // Need to rewrite dfgm.h (hardware interface)
#include "services.h"
#include "task_manager/task_manager.h"

#include <limits.h>

SAT_returnState dfgm_service_app(csp_packet_t *packet);

static uint32_t svc_wdt_counter = 0;
static uint32_t get_svc_wdt_counter() { return svc_wdt_counter; }

/**
 * @brief
 *      FreeRTOS dfgm server task
 * @details
 *      Accepts incoming dfgm service packets and executes
 *      the application
 * @param void* param
 * @return None
 */
void dfgm_service(void *param) {
    // socket initialization
    csp_socket_t *sock;
    sock = csp_socket(CSP_SO_RDPREQ); // creates socket
    csp_bind(sock, TC_DFGM_SERVICE); // binds service to socket
    csp_listen(sock, SERVICE_BACKLOG_LEN); // listens for packets

    svc_wdt_counter++;

    for (;;) {
        // establish a connection
        csp_packet_t *packet;
        csp_conn_t *conn;
        if ((conn = csp_accept(sock, DELAY_WAIT_TIMEOUT)) == NULL) {
            svc_wdt_counter++;
            /* timeout */
            continue;
        }
        svc_wdt_counter++;

        // read packets
        while ((packet = csp_read(conn, 50)) != NULL) {
            if (dfgm_service_app(packet) != SATR_OK) {
                // something went wrong in the service
                csp_buffer_free(packet);
            } else {
                if (!csp_send(conn, packet, 50)) {
                    csp_buffer_free(packet);
                }
            }
        }
        csp_close(conn);
    }
}

/**
 * @brief
 *      Start the dfgm server task
 * @details
 *      Starts the FreeRTOS task responsible for accepting incoming
 *      dfgm service requests
 * @param None
 * @return SAT_returnState
 *      success report
 */
SAT_returnState start_dfgm_service(void) {
    TaskHandle_t svc_tsk;
    taskFunctions svc_funcs = {0};
    svc_funcs.getCounterFunction = get_svc_wdt_counter;

    if (xTaskCreate((TaskFunction_t)dfgm_service, "dfgm_service", 1024, NULL,
                    NORMAL_SERVICE_PRIO, &svc_tsk) != pdPASS) {
        ex2_log("FAILED TO CREATE TASK start_dfgm_service\n");
        return SATR_ERROR;
    }
    ex2_register(svc_tsk, svc_funcs);
    ex2_log("DFGM service started\n");
    return SATR_OK;
}

/**
 * @brief
 *      Takes a CSP packet and switches based on the subservice command
 * @details
 *      Reads/Writes data from DFGM EHs as subservices
 * @attention
 *      Some subservices return the aggregation of error status of multiple HALs
 * @param *packet
 *      The CSP packet
 * @return SAT_returnState
 *      Success or failure
 */
SAT_returnState dfgm_service_app(csp_packet_t *packet) {
    uint8_t ser_subtype = (uint8_t)packet->data[SUBSERVICE_BYTE];
    int8_t status;
    SAT_returnState return_state = SATR_OK; // OK until error encountered

    switch (ser_subtype) {
    case DFGM_RUN: {
        // Get runtime from packet (What data type to use for max value?)
        int runtime;
        cnv8_32(&packet->data[IN_DATA_BYTE], &runtime);

        // Run DFGM Task for amount of seconds in runtime
        status = HAL_DFGM_run(runtime);

        // Return success status of subservice
        memcpy(&packet->data[STATUS_BYTE], &status, sizeof(int8_t));
        set_packet_length(packet, sizeof(int8_t) + 1);
        break;
    }

    case DFGM_START: {
        // Run DFGM Task for INT_MAX seconds
        status = HAL_DFGM_run(INT_MAX);

        // Return success status of subservice
        memcpy(&packet->data[STATUS_BYTE], &status, sizeof(int8_t));
        set_packet_length(packet, sizeof(int8_t) + 1);
        break;
    }

    case DFGM_STOP: {
        // Interrupt the task if it's running
        status = HAL_DFGM_stop();

        // Return success status of subservice
        memcpy(&packet->data[STATUS_BYTE], &status, sizeof(int8_t));
        set_packet_length(packet, sizeof(int8_t) + 1);
        break;
    }

    case DFGM_FILTER: {
        // Get filter mode (either 10 Hz or 1 Hz)
        uint8_t filter_mode = (uint8_t)packet->data[IN_DATA_BYTE];

        // if else statement here for 2 different HAL functions or an if else statement in the HAL function
        status = HAL_DFGM_filterTo10Hz();
        status = HAL_DFGM_filterTo1Hz();

        // Return success status of subservice
        memcpy(&packet->data[STATUS_BYTE], &status, sizeof(int8_t));
        set_packet_length(packet, sizeof(int8_t) + 1);
        break;
    }

    case DFGM_GET_HK: {
        // Get DFGM HK data
        DFGM_Housekeeping HK;
        status = HAL_DFGM_getHK(&HK); // Should check if HK is most recent

        // Convert data types into what's used in packet (refer to line 252 of communication_service.c)
        //HK.attribute1 = csp_htonflt(HK.attribute1);

        // Return success status of subservice
        memcpy(&packet->data[STATUS_BYTE], &status, sizeof(int8_t));
        memcpy(&packet->data[OUT_DATA_BYTE], &HK, sizeof(HK));
        set_packet_length(packet, sizeof(int8_t) + sizeof(HK) + 1);
        break;
    }

    default:
        ex2_log("No such subservice!\n");
        return_state = SATR_PKT_ILLEGAL_SUBSERVICE;
    }

    return return_state;
    }
}




