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

  #include "scheduling_service.h"

  uint8_t sche_tc_buffer[MAX_PKT_LEN+14+1] // Arbitrary size
  Scheduling_service_state sc_s_state;
  Schedule_pkt_pool sch_mem_pool;

  // TODO: replace these externs with our corresponding functions
  // extern void wdg_reset_SCH();
  // extern SAT_returnState mass_storage_schedule_load_api(MS_sid sid, uint32_t sch_number, uint8_t *buf);
  // extern SAT_returnState mass_storage_storeFile(MS_sid sid, uint32_t file, uint8_t *buf, uint16_t *size);
  // extern uint32_t HAL_GetTick(void);


   /**
    * @brief
    * 		Initilizes the scheduling service
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_service_init(){


      // Allocate inner TC memory and mark positions as empty
      for(uint8_t s=0;s<SC_MAX_STORED_SCHEDULES;s++){
          sch_mem_pool.sc_mem_array[s].tc_pck.data = sch_mem_pool.innerd_tc_data[s];
          sch_mem_pool.sc_mem_array[s].pos_taken = false;
      }

      // Update packet pool state
      sc_s_state.nmbr_of_ld_sched = 0;
      sc_s_state.sch_arr_full = false;
      for(uint8_t s=0; s<LAST_APP_ID-1; s++){
          sc_s_state.schs_apids_enabled[s] = true;
      }

      // Load Schedules from storage.
      return scheduling_service_load_schedules();
  }

   /**
    * @brief
    * 		Serves requests to Scheduling service, unique entry point
    * @details
    * 		TODO: change get_pkt and route_pkt, investigate SYSVIEW_PRINT
    * @param tc_tm_packet
    * 		Pointer to TC/TM packet
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_app( csp_packet_t *tc_tm_packet){

      SAT_returnState exec_state = SATR_ERROR;
      SC_pkt *sc_packet;

      if(!C_ASSERT(tc_tm_packet != NULL) == true) { return SATR_ERROR; }

      switch( tc_tm_packet->ser_subtype){
          case SCHS_ENABLE_RELEASE:
              exec_state = scheduling_toggle_apid_release( SCHS_ENABLE_RELEASE, tc_tm_packet->data[3] /*apid*/);
              break;
          case SCHS_DISABLE_RELEASE:
              exec_state = scheduling_toggle_apid_release( SCHS_DISABLE_RELEASE, tc_tm_packet->data[3] /*apid*/);
              break;
          case SCHS_RESET_POOL:
              exec_state = scheduling_reset_pool();
              break;
          case SCHS_INSERT_ELEMENT:
              if( (sc_packet = find_schedule_pos()) == NULL){
                  exec_state = SATR_SCHS_FULL;
              }
              else{
                      exec_state = scheduling_parse_and_extract(sc_packet, tc_tm_packet);
                      if(exec_state == SATR_OK){
                          /*Place the packet into the scheduling array*/
                          sc_s_state.nmbr_of_ld_sched++;
                          if (sc_s_state.nmbr_of_ld_sched == SC_MAX_STORED_SCHEDULES){
                              /*schedule array has become full*/
                              sc_s_state.sch_arr_full = true;
                              //SYSVIEW_PRINT("SCHS PKT ADDED");
                          }
                      }
                  }
              break;
          case SCHS_REMOVE_ELEMENT: /*selection criteria is destined APID and Seq.Count*/
              if(!C_ASSERT( tc_tm_packet->data[1] < LAST_APP_ID) == true) { return SATR_ERROR; }
              /*if(!C_ASSERT( seqcnt <= MAX_SEQ_CNT) == true) { return SATR_ERROR; }*/
              exec_state = scheduling_remove_element( tc_tm_packet->data[1], tc_tm_packet->data[2]);
              break;
          case SCHS_TIME_SHIFT_SEL:
              if(!C_ASSERT( tc_tm_packet->data[5] < LAST_APP_ID) == true) { return SATR_ERROR; }
              /*if(!C_ASSERT( seqcnt <= MAX_SEQ_CNT) == true) { return SATR_ERROR; }*/
              exec_state = scheduling_time_shift_sel(tc_tm_packet->data);
              break;
          case SCHS_TIME_SHIFT_ALL:
              exec_state = scheduling_time_shift_all(tc_tm_packet->data);
              break;
          case SCHS_REPORT_SCH_DETAILED:
              csp_packet_t *sch_rep_d_pkt = get_pkt(PKT_NORMAL);
              if(!C_ASSERT(sch_rep_d_pkt != NULL)==true) { return SATR_ERROR; }
              exec_state = scheduling_service_report_detailed(sch_rep_d_pkt,
                           (TC_TM_app_id)tc_tm_packet->dest_id, tc_tm_packet->data[0], tc_tm_packet->data[1]);
              tc_tm_packet->verification_state = exec_state;
              route_pkt(sch_rep_d_pkt);
              break;
          case SCHS_REPORT_SCH_SUMMARY:
              csp_packet_t *sch_rep_s_pkt = get_pkt(PKT_NORMAL);
              if(!C_ASSERT(sch_rep_s_pkt != NULL)==true) { return SATR_ERROR; }
              exec_state = scheduling_service_report_summary(sch_rep_s_pkt, (TC_TM_app_id)tc_tm_packet->dest_id);
              tc_tm_packet->verification_state = exec_state;
              route_pkt(sch_rep_s_pkt);
              break;
          case SCHS_LOAD_SCHEDULES: /*Load TCs from permanent storage*/
              exec_state = scheduling_service_load_schedules();
              break;
          case SCHS_SAVE_SCHEDULES: /*Save TCs to permanent storage*/
              exec_state = scheduling_service_save_schedules();
              break;
      }

      tc_tm_packet->verification_state = exec_state;
      return exec_state;
  }


   /**
    * @brief
    * 		Check schedule elements in array and release if their time is up
    * @details
    *     Investigate SYSVIEW_PRINT
    *     What does wdg_reset_SCH() do?
    * @attention
    *     For every element:
    *     Check apid enable -> Check relative or absolute ->
    *     if !enabled, if time >= release time, mark !valid
    *     else if time >= release time, then execute it
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_release_expired() {

      for (uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++) {
          if (sch_mem_pool.sc_mem_array[i].pos_taken == true && /*if a valid schedule exists*/
              sch_mem_pool.sc_mem_array[i].enabled &&
              sc_s_state.schs_apids_enabled[(sch_mem_pool.sc_mem_array[i].app_id) - 1] == true){ /*if scheduling enabled for this APID */

              switch(sch_mem_pool.sc_mem_array[i].sch_evt){
                  /*case ABSOLUTE:
                      uint32_t boot_secs = HAL_GetTick();
                      if(sch_mem_pool.sc_mem_array[i].release_time <= (boot_secs / 1000)){
                          route_pkt(&(sch_mem_pool.sc_mem_array[i].tc_pck));
                          sch_mem_pool.sc_mem_array[i].pos_taken = false;
                          sc_s_state.nmbr_of_ld_sched--;
                          sc_s_state.sch_arr_full = false;
                      }
                      break;*/
                  case REPETITIVE:
                      uint32_t qb_time = return_time_QB50();
                      if(!C_ASSERT(qb_time >= MIN_VALID_QB50_SECS) == true ) {
                          wdg_reset_SCH();
                          return SATR_QBTIME_INVALID; }
                      SYSVIEW_PRINT("SCHS CHECKING SCH TIME");
                      if(sch_mem_pool.sc_mem_array[i].release_time <= qb_time){ /*time to execute*/
                          SYSVIEW_PRINT("SCHS ROUTING PKT");
                          route_pkt(&(sch_mem_pool.sc_mem_array[i].tc_pck));
                          if(!(sch_mem_pool.sc_mem_array[i].timeout <=0)){ /*to save the else, and go for rescheduling*/
                              sch_mem_pool.sc_mem_array[i].release_time =
                                  (sch_mem_pool.sc_mem_array[i].release_time + sch_mem_pool.sc_mem_array[i].timeout);
                              sch_mem_pool.sc_mem_array[i].pos_taken = true;
                              sch_mem_pool.sc_mem_array[i].enabled = true;
                              SYSVIEW_PRINT("SCHS SCHEDULE RESCHEDULED");
                              continue;
                          }/*timeout field is positive */
                          sch_mem_pool.sc_mem_array[i].pos_taken = false;
                          sch_mem_pool.sc_mem_array[i].enabled = false;
                          sc_s_state.nmbr_of_ld_sched--;
                          sc_s_state.sch_arr_full = false;
                          SYSVIEW_PRINT("SCHS SCHEDULE POS FREED");
                      }
                      break;
               }
          }
      }/*go to check next schedule*/
      wdg_reset_SCH();
      return SATR_OK;
  }

   /**
    * @brief
    * 		Insert a given Schedule packet on the schedule array
    * @details
    * 		Unique entry point to service
    *     TODO: When would this be used?
    *     TODO: Use CSP
    * @param posit
    * 		Position in the schedule to write to
    * @param theSchpck
    * 		The SC_pkt to insert in the schedule
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_hard_insert_element( uint8_t posit, SC_pkt theSchpck){

      sch_mem_pool.sc_mem_array[posit].app_id = theSchpck.app_id;
      sch_mem_pool.sc_mem_array[posit].assmnt_type = theSchpck.assmnt_type;
      sch_mem_pool.sc_mem_array[posit].enabled = theSchpck.enabled;
      sch_mem_pool.sc_mem_array[posit].release_time = theSchpck.release_time;
      sch_mem_pool.sc_mem_array[posit].sch_evt = theSchpck.sch_evt;
      sch_mem_pool.sc_mem_array[posit].seq_count = theSchpck.seq_count;
      sch_mem_pool.sc_mem_array[posit].sub_schedule_id = theSchpck.sub_schedule_id;
      sch_mem_pool.sc_mem_array[posit].timeout = theSchpck.timeout;
      sch_mem_pool.sc_mem_array[posit].pos_taken = theSchpck.pos_taken;
      sch_mem_pool.sc_mem_array[posit].timeout = theSchpck.timeout;

      sch_mem_pool.sc_mem_array[posit].tc_pck.ack = theSchpck.tc_pck.ack;
      sch_mem_pool.sc_mem_array[posit].tc_pck.app_id = theSchpck.tc_pck.app_id;
      uint8_t i=0;
      for( ;i<theSchpck.tc_pck.len;i++){
          sch_mem_pool.sc_mem_array[posit].tc_pck.data[i] = theSchpck.tc_pck.data[i];
      }
      sch_mem_pool.sc_mem_array[posit].tc_pck.dest_id = theSchpck.tc_pck.dest_id;
      sch_mem_pool.sc_mem_array[posit].tc_pck.len = theSchpck.tc_pck.len;
      sch_mem_pool.sc_mem_array[posit].tc_pck.seq_count = theSchpck.tc_pck.seq_count;
      sch_mem_pool.sc_mem_array[posit].tc_pck.seq_flags = theSchpck.tc_pck.seq_flags;
      sch_mem_pool.sc_mem_array[posit].tc_pck.ser_subtype = theSchpck.tc_pck.ser_subtype;
      sch_mem_pool.sc_mem_array[posit].tc_pck.ser_type = theSchpck.tc_pck.ser_type;
      sch_mem_pool.sc_mem_array[posit].tc_pck.verification_state = theSchpck.tc_pck.verification_state;

      return SATR_OK;
  }

   /**
    * @brief
    * 		Enable or disable the specified apid release
    * @details
    * 		If the release for a specific APID is enabled (true),
    *     then the schedule packets destined for this APID can be released
    *     TODO: unclear about reason for C_ASSERTs
    * @param subtype
    * 		SCHS_ENABLE_RELEASE or SCHS_DISABLE_RELEASE
    * @param apid
    * 		APID to enable or disable
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_toggle_apid_release( uint8_t subtype, uint8_t apid  ){

      if(!C_ASSERT( subtype == SCHS_ENABLE_RELEASE ||
                    subtype == SCHS_DISABLE_RELEASE ) == true) { return SATR_ERROR; }
      if(!C_ASSERT( apid < LAST_APP_ID) == true)               { return SATR_ERROR; }

      if( subtype == SCHS_ENABLE_RELEASE ){
          sc_s_state.schs_apids_enabled[apid-1] = true; }
      else{
          sc_s_state.schs_apids_enabled[apid-1] = false; }

      return SATR_OK;
  }

   /**
    * @brief
    * 		Reset the schedule memory pool
    * @details
    * 		Marks every schedule position as invalid and eligible for allocation
    *     to a new scheduling packet. Also, enables every APID release.
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_reset_pool(){

      uint8_t g = 0;
      sc_s_state.nmbr_of_ld_sched = 0;
      sc_s_state.sch_arr_full = false;

      /*mark every pos as !valid, = available*/
      for( ;g<SC_MAX_STORED_SCHEDULES;g++ ){
          sch_mem_pool.sc_mem_array[g].pos_taken = false;
      }
      /*enable release for all apids*/
      for( g=0;g<LAST_APP_ID-1;g++ ){
          sc_s_state.schs_apids_enabled[g] = true;
      }

      //TODO: reload schedules from storage?
      return SATR_OK;
  }

   /**
    * @brief
    * 		Removes a given schedule packet from the schedule array
    * @details
    * 		TODO: Are we using "sequence count" or an alternative?
    * @param apid
    * 		First identifier: APID to which the schedule packet is destined
    * @param seqc
    * 		Second identifier: Sequence count of the packet
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_remove_element(uint8_t apid, uint16_t seqc){

      for(uint8_t i=0;i<SC_MAX_STORED_SCHEDULES;i++){
          if (sch_mem_pool.sc_mem_array[i].seq_count == seqc &&
              sch_mem_pool.sc_mem_array[i].app_id == apid ){
              sch_mem_pool.sc_mem_array[i].pos_taken = false;
              sc_s_state.nmbr_of_ld_sched--;
              sc_s_state.sch_arr_full = false;
              return SATR_OK;
          }
      }
      return SATR_ERROR; /*selection criteria not met*/
  }

   /**
    * @brief
    * 		Find a free schedule pool position
    * @return
    * 		Pointer to free SC_pkt position in array or NULL if none exists
    */
  SC_pkt* find_schedule_pos() {

      for (uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++) {
          if (!sch_mem_pool.sc_mem_array[i].pos_taken) {
              return &(sch_mem_pool.sc_mem_array[i]);
          }
      }
      return NULL;
  }

   /**
    * @brief
    * 		Parse scheduling telecommand to extract scheduling packet
    * @details
    * 		From data field of telecommand packet, will extract elements of the
    *     scheduling packet
    *     TODO: Why are all the C_ASSERTs necessary?
    *           Return states don't exist yet. Any renames?
    * @param sc_pkt
    * 		Scheduling packet to write to
    * @param tc_pkt
    * 		Telecommand packet to read from
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_parse_and_extract(SC_pkt *sc_pkt, csp_packet_t *tc_pkt) {

      /*extract the packet and route accordingly*/
      uint32_t time = 0;
      uint32_t exec_timeout = 0;
      uint8_t offset = 14;

      /*extract the scheduling packet from the data pointer*/
      (*sc_pkt).sub_schedule_id = tc_pkt->data[0];
      if (!C_ASSERT((*sc_pkt).sub_schedule_id == 1) == true) {
          return SATR_SCHS_ID_INVALID;
      }

      (*sc_pkt).assmnt_type = tc_pkt->data[4];
      if (!C_ASSERT((*sc_pkt).assmnt_type == 1) == true) {

          return SATR_SCHS_ASS_TP_ID_INVLD;
      }

      (*sc_pkt).sch_evt = (SC_event_time_type) tc_pkt->data[5];
      if (!C_ASSERT((*sc_pkt).sch_evt < LAST_EVENTTIME) == true) {

          return SATR_SCHS_RLS_TT_ID_INVLD;
      }

      /*7,8,9,10th bytes are the time fields, combine them to a uint32_t*/
      time = (time | tc_pkt->data[9]) << 8;
      time = (time | tc_pkt->data[8]) << 8;
      time = (time | tc_pkt->data[7]) << 8;
      time = (time | tc_pkt->data[6]);
      /*read execution time out fields*/
      exec_timeout = (exec_timeout | tc_pkt->data[13]) << 8;
      exec_timeout = (exec_timeout | tc_pkt->data[12]) << 8;
      exec_timeout = (exec_timeout | tc_pkt->data[11]) << 8;
      exec_timeout = (exec_timeout | tc_pkt->data[10]);

      if( exec_timeout < MIN_SCH_REP_INTRVL ) {
          SYSVIEW_PRINT("SCHS PKT REJECTED LOW REP INTRVL");
          return SATR_SCHS_TIM_INVLD; }

      /*extract data from internal TC packet ( app_id )*/
      (*sc_pkt).app_id = (TC_TM_app_id)tc_pkt->data[offset + 1];
      if (!C_ASSERT((*sc_pkt).app_id < LAST_APP_ID) == true){
          return SATR_PKT_ILLEGAL_APPID;
      }
      (*sc_pkt).seq_count = (*sc_pkt).seq_count | (tc_pkt->data[offset + 2] >> 2);
      (*sc_pkt).seq_count << 8;
      (*sc_pkt).seq_count = (*sc_pkt).seq_count | (tc_pkt->data[offset + 3]);
      if(check_existing_sch((*sc_pkt).app_id, (*sc_pkt).seq_count)){
          SYSVIEW_PRINT("SCHS PKT REJECTED, ALREADY EXISTS");
          return SATR_SCHS_INTRL_LGC_ERR; }

      (*sc_pkt).release_time = time;
      (*sc_pkt).timeout = exec_timeout;
      (*sc_pkt).pos_taken = true;
      (*sc_pkt).enabled = true;

      /*copy the internal TC packet for future use*/
      /*  tc_pkt is a TC containing 14 bytes of data related to scheduling service.
       *  After those 14 bytes, a 'whole_inner_tc' packet starts.
       *
       *  The 'whole_inner_tc' offset in the tc_pkt's data payload is: 15 (16th byte).
       *
       *  The length of the 'whole_inner_tc' is tc_pkt->data - 14 bytes
       *
       *  Within the 'whole_inner_tc' the length of the 'inner' goes for:
       *  16+16+16+32+(tc_pkt->len - 11)+16 bytes.
       */
      return scheduling_copy_inner_tc( &(tc_pkt->data[14]), &((*sc_pkt).tc_pck), (uint16_t) tc_pkt->len - 14);
  }

   /**
    * @brief
    * 		Checks if a scpedified scheduling packet exists
    * @details
    * 		The APID's schedule release must be enabled as well
    *     TODO: Using sequence count?
    * @param apid
    * 		First identifier: APID to look for
    * @param seqc
    * 		Seconds identifier: Scheduling packet's sequence count
    * @return
    * 		The execution state
    */
  uint8_t check_existing_sch(uint8_t apid, uint8_t seqc){
      for(uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++){
          if(sch_mem_pool.sc_mem_array[i].pos_taken == true && /*if a valid schedule exists*/
              sch_mem_pool.sc_mem_array[i].enabled == true){
              if( sch_mem_pool.sc_mem_array[i].app_id == apid &&
                  sch_mem_pool.sc_mem_array[i].seq_count == seqc ){
                  return true;
              }
          }
      }
      return false;
  }

   /**
    * @brief
    * 		Copy the inner telecommand to a scheduling packet
    * @details
    * 		The data field of a scheduling telecommand will contain header info
    *     for the scheduling packet, plus the telecommand to execute, which
    *     needs to be extracted.
    *     TODO: Rename params? "pkt" and "buf" are unclear
    *           Change extracted parameters to fit csp packet (!!)
    * @param buf
    * 		Address of the first element in the inner telecommand
    * @param pkt
    * 		Pointer to the scheduling packet's telecommand field
    * @param size
    *     Length of the inner telecommand
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_copy_inner_tc(const uint8_t *buf, csp_packet_t *pkt, const uint16_t size) {

      uint8_t tmp_crc[2];
      uint8_t ver, dfield_hdr, ccsds_sec_hdr, tc_pus;
      if(!C_ASSERT(buf != NULL && pkt != NULL && pkt->data != NULL) == true)  { return SATR_ERROR; }
      if(!C_ASSERT(size < MAX_PKT_SIZE) == true)                              { return SATR_ERROR; }

      tmp_crc[0] = buf[size - 1];
      checkSum(buf, size-2, &tmp_crc[1]); /* -2 for excluding the checksum bytes*/
      ver = buf[0] >> 5;
      pkt->type = (buf[0] >> 4) & 0x01;
      dfield_hdr = (buf[0] >> 3) & 0x01;

      pkt->app_id = (TC_TM_app_id)buf[1];

      pkt->seq_flags = buf[2] >> 6;

      cnv8_16((uint8_t*)&buf[2], &pkt->seq_count);
      pkt->seq_count &= 0x3FFF;

      cnv8_16((uint8_t*)&buf[4], &pkt->len);

      ccsds_sec_hdr = buf[6] >> 7;

      tc_pus = buf[6] >> 4;

      pkt->ack = 0x04 & buf[6];

      pkt->ser_type = buf[7];
      pkt->ser_subtype = buf[8];
      pkt->dest_id = (TC_TM_app_id)buf[9];

      pkt->verification_state = SATR_PKT_INIT;

      if(!C_ASSERT(pkt->app_id < LAST_APP_ID) == true) {
          pkt->verification_state = SATR_PKT_ILLEGAL_APPID;
          return SATR_PKT_ILLEGAL_APPID;
      }

      if(!C_ASSERT(pkt->len == size - ECSS_HEADER_SIZE - 1) == true) {
          pkt->verification_state = SATR_PKT_INV_LEN;
          return SATR_PKT_INV_LEN;
      }
      pkt->len = pkt->len - ECSS_DATA_HEADER_SIZE - ECSS_CRC_SIZE + 1;

      if(!C_ASSERT(tmp_crc[0] == tmp_crc[1]) == true) {
          pkt->verification_state = SATR_PKT_INC_CRC;
          return SATR_PKT_INC_CRC;
      }

      if(!C_ASSERT(services_verification_TC_TM[pkt->ser_type][pkt->ser_subtype][pkt->type] == 1) == true) {
          pkt->verification_state = SATR_PKT_ILLEGAL_PKT_TP;
          return SATR_PKT_ILLEGAL_PKT_TP;
      }

      if(!C_ASSERT(ver == ECSS_VER_NUMBER) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(tc_pus == ECSS_PUS_VER) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(ccsds_sec_hdr == ECSS_SEC_HDR_FIELD_FLG) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(pkt->type == TC || pkt->type == TM) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(dfield_hdr == ECSS_DATA_FIELD_HDR_FLG) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(pkt->ack == TC_ACK_NO || pkt->ack == TC_ACK_ACC) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      if(!C_ASSERT(pkt->seq_flags == TC_TM_SEQ_SPACKET) == true) {
          pkt->verification_state = SATR_ERROR;
          return SATR_ERROR;
      }

      for(int i = 0; i < pkt->len; i++) {
          pkt->data[i] = buf[ECSS_DATA_OFFSET+i];
      }

      return SATR_OK;
  }

   /**
    * @brief
    * 		Time shift all currently active schedules
    * @details
    * 		For repetitive schedules add/subtract the repetition time
    *     For one time schedules add/subtract the execution time
    *     TODO: Include other sch_evt types? Write ABSOLUTE case
    *     Does this take 4 uint8_ts to create a uint32_t time?
    * @param time_v
    * 		Pointer to time value to be added/subtracted
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_time_shift_all(uint8_t *time_v){

      uint32_t ushift_time = 0;
      cnv8_32(time_v, &ushift_time);
      for(uint8_t pos = 0; pos < SC_MAX_STORED_SCHEDULES; pos++) {
          if( sch_mem_pool.sc_mem_array[pos].pos_taken == true){
              switch(sch_mem_pool.sc_mem_array[pos].sch_evt){
                  case REPETITIVE:
                      uint32_t rele_time = sch_mem_pool.sc_mem_array[pos].release_time;
                      uint32_t qb_time_now = return_time_QB50();
                      uint8_t neg = (ushift_time >> 31) & 0x1;
                      uint32_t shift_time_val = (~ushift_time)+ 1;
                      uint32_t new_release_t = 0;
                      if(neg){ /*then subtract it from release time*/
                          if(shift_time_val >= rele_time){ /*subtraction not possible, erroneous state*/
                              break;
                          }
                          new_release_t = rele_time - shift_time_val;
                          if((new_release_t < qb_time_now)){
                              break;
                          }
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          break;
                      }
                      /*then add it to release time*/
                      new_release_t = rele_time + ushift_time;
                      if( new_release_t <= MAX_QB_SECS ){ /*to far to execute, we will not exist until then*/
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          return SATR_OK;
                      }
                      break;
                  break;
              }
          }
      }
      return SATR_OK;
  }

   /**
    * @brief
    * 		Time shift selected schedule packet
    * @details
    * 		From data field of telecommand packet, will extract elements of the
    *     scheduling packet
    *     TODO: Include other sch_evt types? Write ABSOLUTE case
    * @param data_v
    * 		Pointer to start of scheduling telecommand data field
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_time_shift_sel(uint8_t *data_v){

      uint8_t apid = data_v[5];
      uint8_t seqc = data_v[6];
      uint32_t ushift_time = 0;
      cnv8_32(data_v, &ushift_time);
      if(!C_ASSERT( apid < LAST_APP_ID) == true) { return SATR_ERROR; }
      /*if(!C_ASSERT( seqcnt <= MAX_SEQ_CNT) == true) { return SATR_ERROR; }*/
      for (uint8_t pos = 0; pos < SC_MAX_STORED_SCHEDULES; pos++) {
          if (sch_mem_pool.sc_mem_array[pos].seq_count == seqc &&
              sch_mem_pool.sc_mem_array[pos].app_id == apid &&
              sch_mem_pool.sc_mem_array[pos].enabled == true ){
              switch(sch_mem_pool.sc_mem_array[pos].sch_evt){
                  case REPETITIVE:
                      uint32_t rele_time = sch_mem_pool.sc_mem_array[pos].release_time;
                      uint32_t qb_time_now = return_time_QB50();
                      uint8_t neg = (ushift_time >> 31) & 0x1;
                      uint32_t shift_time_val = (~ushift_time)+ 1;
                      uint32_t new_release_t = 0;
                      if(neg){ /*then subtract it from release time*/
                          if(shift_time_val >= rele_time){ /*subtraction not possible, erroneous state*/
                              return SATR_ERROR;
                          }
                          new_release_t = rele_time - shift_time_val;
                          if((new_release_t < qb_time_now)){
                              return SATR_ERROR;
                          }
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          return SATR_OK;
                      }
                      /*then add it to release time*/
                      new_release_t = rele_time + ushift_time;
                      if( new_release_t <= MAX_QB_SECS ){ /*to far to execute, we will not exist until then*/
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          return SATR_OK;
                      }
                      return SATR_ERROR;
                      break;
              }
          }
      }
      return SATR_ERROR; /*schedule not found*/
  }

   /**
    * @brief
    * 		Handles the simple report request
    * @details
    * 		This reports basic info about every schedule position
    *     TODO: Rename parameters?
    * @param pkt
    * 		Telemetry packet to write to
    * @param dest_id
    * 		APID of the packet's destination
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_service_report_summary(csp_packet_t *pkt, TC_TM_app_id dest_id){

      if(!C_ASSERT(pkt != NULL) == true) { return SATR_ERROR; }
      scheduling_service_crt_pkt_TM(pkt, SCHS_SIMPLE_SCH_REPORT, dest_id);
      uint8_t base = 0;
      for(uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++){
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].pos_taken;
          base+=1;
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].enabled;
          base+=1;
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].app_id;
          base+=1;
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].seq_count;
          base+=1;
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].sch_evt;
          base+=1;
          cnv32_8(sch_mem_pool.sc_mem_array[i].release_time, &pkt->data[base]);
          base+=4;
          cnv32_8(sch_mem_pool.sc_mem_array[i].timeout, &pkt->data[base]);
          base+=4;
      }
      pkt->len = 13*SC_MAX_STORED_SCHEDULES;
      return SATR_OK;
  }

   /**
    * @brief
    * 		Handles the full report request
    * @details
    * 		This reports specific info about a schedule. Does the reverse of
    *     adding to the schedule pool.
    *     TODO: Rename parameters?
    *           Need to use CSP fields (!!)
    * @param pkt
    * 		Telemetry packet to write to
    * @param dest_id
    * 		APID of the packet's destination
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_service_report_detailed(csp_packet_t *pkt, TC_TM_app_id dest_id, uint8_t apid, uint8_t seqcnt){

      if(!C_ASSERT(pkt != NULL) == true) { return SATR_ERROR; }
      if(!C_ASSERT( apid < LAST_APP_ID) == true) { return SATR_ERROR; }
      /*if(!C_ASSERT( seqcnt <= MAX_SEQ_CNT) == true) { return SATR_ERROR; }*/

      scheduling_service_crt_pkt_TM(pkt, SCHS_DETAILED_SCH_REPORT, dest_id);
      for(uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++){
          if(sch_mem_pool.sc_mem_array[i].app_id == apid &&
             sch_mem_pool.sc_mem_array[i].seq_count == seqcnt){
              uint8_t base =0;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.app_id;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.type;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.seq_flags;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.seq_count;
              base+=1;
              cnv16_8(sch_mem_pool.sc_mem_array[i].tc_pck.len, &pkt->data[base]);
              base+=2;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.ack;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.ser_type;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.ser_subtype;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.dest_id;
              base+=1;
              for(uint16_t p=0;p<sch_mem_pool.sc_mem_array[i].tc_pck.len;p++){
                  pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.data[p];
                  base+=1;
              }
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.verification_state;
              pkt->len = base;
              break;
          }
      }
      return SATR_OK;
  }

  /**
   * @brief
   * 		Creates a telemetry packet
   * @details
   * 		TODO: Use CSP create packet function
   * @param pkt
   * 		Telemetry packet to write to
   * @param sid
   * 		Subservice ID (for telemetry)
   * @param dest_app_id
   *    Telemetry packet's destination APID
   * @return
   * 		The execution state
   */
  SAT_returnState scheduling_service_crt_pkt_TM(csp_packet_t *pkt, uint8_t sid, TC_TM_app_id dest_app_id ){

      if(!C_ASSERT(dest_app_id < LAST_APP_ID) == true)  { return SATR_ERROR; }
      crt_pkt(pkt, SYSTEM_APP_ID, TM, TC_ACK_NO, TC_SCHEDULING_SERVICE, sid, dest_app_id);
      pkt->len = 0;
      return SATR_OK;
  }

  /**
   * @brief
   * 		Saves schedule pool to persistent storage
   * @details
   * 		Everything is saved sequentially in a buffer, one after another
   *    TODO: Substitute mass storage service function call
   * @return
   * 		The execution state
   */
  SAT_returnState scheduling_service_save_schedules(){
      int filedesc = open("saved_sche.txt", O_WRONLY | O_CREAT, S_IWUSR);
      if(filedesc < 0){return SATR_ERROR;}

      /*convert the Schedule packet from Schedule_pkt_pool format to an array of linear bytes*/
      for(uint8_t s=0;s<SC_MAX_STORED_SCHEDULES;s++){
          if( sch_mem_pool.sc_mem_array[s].pos_taken == true &&
              sch_mem_pool.sc_mem_array[s].enabled   == true){

              memset(sche_tc_buffer,0x00,MAX_PKT_LEN+14+1);
              uint16_t f_s=0;
              /*save the tc's data length in the first 2 bytes*/
              cnv16_8(sch_mem_pool.sc_mem_array[s].tc_pck.length, &sche_tc_buffer[f_s]);
              f_s+=2;
              /*start saving sch packet*/
              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].app_id;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].seq_count;
              f_s+=1;
      //        cnv16_8(sch_mem_pool.sc_mem_array[s].seq_count, &sche_tc_buffer[3]);
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].enabled;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].sub_schedule_id;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].num_of_sch_tc;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].intrlck_set_id;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].intrlck_ass_id;
              f_s+=1;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].assmnt_type;
              f_s+=1;
              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].sch_evt;
              f_s+=1;
              cnv32_8(sch_mem_pool.sc_mem_array[s].release_time,&sche_tc_buffer[f_s]); //11
              f_s+=4;
              cnv32_8(sch_mem_pool.sc_mem_array[s].timeout,&sche_tc_buffer[f_s]); //15
              f_s+=4;
              /*TC parsing begins here*/
              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].tc_pck.id;
              f_s+=1;
//              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.type;
//              f_s+=1;
//              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.seq_flags;
//              f_s+=1;
//              cnv16_8(sch_mem_pool.sc_mem_array[s].tc_pck.seq_count,&sche_tc_buffer[f_s]);
//              f_s+=2;
//              cnv16_8(sch_mem_pool.sc_mem_array[s].tc_pck.len,&sche_tc_buffer[f_s]);
//              f_s+=2;
//              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.ack;
//              f_s+=1;
//              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.ser_type;
//              f_s+=1;
//              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.ser_subtype;
//              f_s+=1;
//              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].tc_pck.dest_id;
//              f_s+=1;
              /*copy tc payload data*/
              uint16_t i = 0;
              for(;i<sch_mem_pool.sc_mem_array[s].tc_pck.length;i++){
                  sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.data[i];
                  f_s+=1;
              }
//              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].tc_pck.verification_state;
//              f_s+=1;
              uint8_t chk = 0;
              for(uint16_t l=0;l<f_s-1;l++){
                  chk = chk ^ sche_tc_buffer[l];
              }
              sche_tc_buffer[f_s] = chk;
              f_s+=1;

              if(write(filedesc, sche_tc_buffer, f_s) != f_s){
                return SATR_ERROR;
              }
          }
      }
      if(close(filedesc) < 0){return SATR_ERROR;}

      return SATR_OK;
  }

  /**
   * @brief
   * 		Loads schedule pool from persisten storage
   * @details
   * 		Everything is loaded sequentially from a buffer
   *    TODO: Correct endianness?
   * @return
   * 		The execution state
   */
    SAT_returnState scheduling_service_load_schedules(){

        SAT_returnState state = SATR_ERROR;

        int filedesc = open("saved_sche.txt", O_RDONLY);
        if(filedesc < 0){return SATR_ERROR;}

        for(uint8_t s=0;s<SC_MAX_STORED_SCHEDULES;s++){

            memset(sche_tc_buffer,0x00,MAX_PKT_LEN+14+1);

            uint8_t temp[2] = {0};
            if(read(filedesc, temp, 2) < 0){return SATR_ERROR;}
            cnv8_16LE(temp, &sch_mem_pool.sc_mem_array[s].tc_pck.length);

            if(read(filedesc, sche_tc_buffer, &sch_mem_pool.sc_mem_array[s].tc_pck.length + 19) < 0){
              return SATR_ERROR;
            }else{
              state = SATR_OK;
            }

            if( state == SATR_OK){
                uint16_t f_s=0;
                /*start loading the sch packet*/
                sch_mem_pool.sc_mem_array[s].app_id = (TC_TM_app_id)sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].seq_count = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].enabled = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].sub_schedule_id = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].num_of_sch_tc = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].intrlck_set_id = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].intrlck_ass_id = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].assmnt_type = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].sch_evt = (SC_event_time_type) sche_tc_buffer[f_s];
                f_s+=1;
                cnv8_32(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].release_time);
                f_s+=4;
                cnv8_32(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].timeout);
                f_s+=4;
                /*TC parsing begins here*/
                sch_mem_pool.sc_mem_array[s].tc_pck.id = (TC_TM_app_id) sche_tc_buffer[f_s];
                f_s+=1;
//                sch_mem_pool.sc_mem_array[s].tc_pck.type = sche_tc_buffer[f_s];
//                f_s+=1;
//                sch_mem_pool.sc_mem_array[s].tc_pck.seq_flags = sche_tc_buffer[f_s];
//                f_s+=1;
//                cnv8_16LE(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].tc_pck.seq_count);
//                f_s+=2;
//                cnv8_16LE(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].tc_pck.len);
//                f_s+=2;
//                sch_mem_pool.sc_mem_array[s].tc_pck.ack = sche_tc_buffer[f_s];
//                f_s+=1;
//                sch_mem_pool.sc_mem_array[s].tc_pck.ser_type = sche_tc_buffer[f_s];
//                f_s+=1;
//                sch_mem_pool.sc_mem_array[s].tc_pck.ser_subtype = sche_tc_buffer[f_s];
//                f_s+=1;
//                sch_mem_pool.sc_mem_array[s].tc_pck.dest_id = (TC_TM_app_id) sche_tc_buffer[f_s];
//                f_s+=1;
                /*copy tc payload data*/
                uint16_t i = 0;
                for(;i<sch_mem_pool.sc_mem_array[s].tc_pck.length;i++){
                    sch_mem_pool.sc_mem_array[s].tc_pck.data[i] = sche_tc_buffer[f_s];
                    f_s+=1;
                }
//                sch_mem_pool.sc_mem_array[s].tc_pck.verification_state = (SAT_returnState) sche_tc_buffer[f_s];
//                f_s+=1;
                uint8_t l_chk = sche_tc_buffer[f_s];

                uint8_t chk = 0;
                for(uint16_t l=0;l<f_s-1;l++){
                    chk = chk ^ sche_tc_buffer[l];
                }

                if( l_chk == chk && chk!= 0x0 ){
                    sch_mem_pool.sc_mem_array[s].pos_taken = true;
                    sch_mem_pool.sc_mem_array[s].enabled = true;
                }
                else{
                    sch_mem_pool.sc_mem_array[s].pos_taken = false;
                    sch_mem_pool.sc_mem_array[s].enabled = false;
                }
            }
        }
        if(close(filedesc) < 0){return SATR_ERROR;}

        return state;
    }
