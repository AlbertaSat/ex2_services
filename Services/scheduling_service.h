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
  * @file scheduling_service.h
  * @author upSat, Thomas Ganley
  * @date 2020-07-14
  */

  #ifndef SCHEDULING_SERVICE_H
  #define SCHEDULING_SERVICE_H

  #include <stdlib.h>
  #include "service_utilities.h"
  //#include "time_management_service.h"
  #include "services.h"
  // #include "pkt_pool.h"
  // #include "sysview.h"

  #define SC_MAX_STORED_SCHEDULES 15 // Arbitrary

  typedef enum {
    ABSOLUTE = 0,
    SCHEDULE = 1,
    SUBSCHEDULE = 2,
    REPETITIVE = 4,
    LAST_EVENTTIME = 5
  }SC_event_time_type;

  typedef struct {
          /* This is the application id that the telecommand it is destined to.
           * This info will be extracted from the encapsulated TC packet.
           */
      TC_TM_app_id app_id;

          /* This is the sequence count of the telecommand packet.
           * This info will be extracted from the encapsulated TC packet.
           */
      uint8_t seq_count;

          /* If the specific schedule command is enabled.
           * Enabled = 1, Disabled = 0.
           */
      uint8_t enabled;

          /* Currently not supported by this implementation.*
           * For this specific implementation is set to 1 (one)
           * for every schedule packet.
           */
      uint8_t sub_schedule_id;

          /* Success or failure of the dependent telecommand, Success=1, Failure=0
           * For this specific implementation is set to 1 (success)
           * for every schedule packet.
           */
      uint8_t assmnt_type;

          /* Determines the release type for the telecommand.
           * See: SC_event_time_type
           */
      SC_event_time_type sch_evt;

          /* Absolute or relative time of telecommand execution,
           * this field has meaning relative to sch_evt member.
           */
      uint32_t release_time;

          /* This is a delta time which when added to the release time of the scheduled telecommand, the command
           * is expected to complete execution.
           * Timeout execution is only set if telecommand sets interlocks, so for our
           * current implementation will be always 0 (zero)
           */
       uint32_t timeout;

          /* The actual telecommand packet to be scheduled and executed
           *
           */
      csp_packet_t tc_pck;

          /* Declares a schedule position as pos_taken or !pos_taken.
           * If a schedule position is noted as !pos_taken, it can be replaced
           * by a new SC_pkt packet.
           * When a schedule goes for execution,
           * automatically its position becomes !pos_taken (exception for repetitive SC_pkt(s) ).
           * pos_taken=true, 1, !pos_taken=false ,0
           */
      uint8_t pos_taken;

  }SC_pkt;


  typedef struct{
      /*Holds structures, containing Scheduling Telecommands*/
      SC_pkt sc_mem_array[SC_MAX_STORED_SCHEDULES];

      /* Memory array for inner TC data payload
       * One to one mapping with the sc_mem_array
       */
      uint8_t innerd_tc_data[SC_MAX_STORED_SCHEDULES][MAX_PKT_LEN];

  }Schedule_pkt_pool;

  extern Schedule_pkt_pool sch_mem_pool;

  typedef struct {
      /*Number of loaded schedules*/
      uint8_t nmbr_of_ld_sched;

      /* Defines the state of the Scheduling service,
      * if enabled the release of TC is running.
      * Enable = true, 1
      * Disabled = false, 0
      */
      uint8_t schs_service_enabled;

      /* Schedules memory pool is full.
       * Full = true, 1
       * space avaliable = false, 0
       */
      uint8_t sch_arr_full;

      /* This array holds the value 1 (True), if
       * the specified APID scheduling is enabled.
       * OBC_APP_ID = 1, starts at zero index.
       */
      uint8_t schs_apids_enabled[LAST_APP_ID-1];

  }Scheduling_service_state;

  extern Scheduling_service_state sc_s_state;

  extern SAT_returnState route_pkt(csp_packet_t *pkt);
  extern SAT_returnState crt_pkt(csp_packet_t *pkt, TC_TM_app_id app_id, uint8_t type, uint8_t ack, uint8_t ser_type, uint8_t ser_subtype, TC_TM_app_id dest_id);

  /* Initialization */
  SAT_returnState scheduling_service_init();

  /* Routing function based on subservice */
  SAT_returnState scheduling_app(csp_packet_t* spacket);

  /* Execute schedule */
  SAT_returnState scheduling_release_expired();

  /* Directly insert scheduling packet */
  SAT_returnState scheduling_hard_insert_element( uint8_t posit, SC_pkt theSchpck );

  /* Enable/Disable the execution of schedules */
  SAT_returnState scheduling_toggle_apid_release( uint8_t subtype, uint8_t apid );

  /* Removing schedules */
  SAT_returnState scheduling_reset_pool();

  SAT_returnState scheduling_remove_element(uint8_t apid, uint16_t seqc );

  /* Extracting Schedule packet from TC */
  SC_pkt* find_schedule_pos();

  SAT_returnState scheduling_parse_and_extract( SC_pkt *sc_pkt, csp_packet_t *tc_pkt );

  uint8_t check_existing_sch(uint8_t apid, uint8_t seqc);

  SAT_returnState scheduling_copy_inner_tc(const uint8_t *buf, csp_packet_t *pkt, const uint16_t size);

  /* Time Shifting */
  SAT_returnState scheduling_time_shift_all(uint8_t *time_v);

  SAT_returnState scheduling_time_shift_sel(uint8_t *data_v);

  /* Creating reports */
  SAT_returnState scheduling_service_report_summary(csp_packet_t *pkt, TC_TM_app_id dest_id);

  SAT_returnState scheduling_service_report_detailed(csp_packet_t *pkt, TC_TM_app_id dest_id, uint8_t apid, uint8_t seqcnt);

  SAT_returnState scheduling_service_crt_pkt_TM(csp_packet_t *pkt, uint8_t sid, TC_TM_app_id dest_app_id );

  /* Interacting with permanent memory */
  SAT_returnState scheduling_service_save_schedules();

  SAT_returnState scheduling_service_load_schedules();

  #endif
