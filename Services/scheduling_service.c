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
  * @author UpSat, Thomas Ganley
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
          sc_s_state.schs_services_enabled[s] = true;
      }

      // Load Schedules from storage.
      return scheduling_service_load_schedules();
  }

   /**
    * @brief
    * 		Serves requests to Scheduling service, unique entry point
    *     TODO: fix get_pkt() and route_pkt()
    * @param tc_tm_packet
    * 		Pointer to TC/TM packet
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_app( csp_packet_t *tc_tm_packet){

      SAT_returnState exec_state = SATR_ERROR;
      SC_pkt *sc_packet;

      if(!C_ASSERT(tc_tm_packet != NULL) == true) { return SATR_ERROR; }

      switch( tc_tm_packet->data[0]){
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
                          }
                      }
                  }
              break;
          case SCHS_REMOVE_ELEMENT: /*selection criteria is destined APID and Seq.Count*/
              exec_state = scheduling_remove_element(tc_tm_packet->data);
              break;
          case SCHS_TIME_SHIFT_SEL:
              exec_state = scheduling_time_shift_sel(tc_tm_packet->data);
              break;
          case SCHS_TIME_SHIFT_ALL:
              exec_state = scheduling_time_shift_all(tc_tm_packet->data);
              break;
          case SCHS_REPORT_SCH_DETAILED:
              csp_packet_t *sch_rep_d_pkt = get_pkt(PKT_NORMAL);
              exec_state = scheduling_service_report_detailed(sch_rep_d_pkt,
                           (TC_TM_app_id)tc_tm_packet->dest_id, tc_tm_packet->data[0], tc_tm_packet->data[1]);
              route_pkt(sch_rep_d_pkt);
              break;
          case SCHS_REPORT_SCH_SUMMARY:
              csp_packet_t *sch_rep_s_pkt = get_pkt(PKT_NORMAL);
              exec_state = scheduling_service_report_summary(sch_rep_s_pkt, (TC_TM_app_id)tc_tm_packet->dest_id);
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
    *     TODO: Fix get_time(), replace route_pkt
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_release_expired() {

      for (uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++) {
          if (sch_mem_pool.sc_mem_array[i].pos_taken == true
              && sch_mem_pool.sc_mem_array[i].enabled == true){

                uint32_t time = get_time();
                if(sch_mem_pool.sc_mem_array[i].release_time <= time){
                route_pkt(&(sch_mem_pool.sc_mem_array[i].tc_pck));
                sch_mem_pool.sc_mem_array[i].pos_taken = false;
                sc_s_state.nmbr_of_ld_sched--;
                sc_s_state.sch_arr_full = false;
              }
          }
      }/*go to check next schedule*/
      return SATR_OK;
  }

   /**
    * @brief
    * 		Insert a given Schedule packet on the schedule array
    * @details
    * 		Unique entry point to service
    *     TODO: When would this be used?
    *     TODO: Use CSP, fix get_time
    * @param posit
    * 		Position in the schedule to write to
    * @param theSchpck
    * 		The SC_pkt to insert in the schedule
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_hard_insert_element( uint8_t posit, SC_pkt theSchpck){

      sch_mem_pool.sc_mem_array[posit].sch_id = theSchpck.sch_id;
      sch_mem_pool.sc_mem_array[posit].enabled = theSchpck.enabled;
      sch_mem_pool.sc_mem_array[posit].sch_evt = theSchpck.sch_evt;
      sch_mem_pool.sc_mem_array[posit].release_time = theSchpck.release_time;

      // Relative and repetitive times convert to unix timestamps
      sch_mem_pool.sc_mem_array[posit].release_time += get_time();

      sch_mem_pool.sc_mem_array[posit].pos_taken = theSchpck.pos_taken;

      uint8_t i=0;
      sch_mem_pool.sc_mem_array[posit].tc_pck.length = theSchpck.tc_pck.length;
      sch_mem_pool.sc_mem_array[posit].tc_pck.id = theSchpck.tc_pck.id;

      for( ;i<theSchpck.tc_pck.len;i++){
          sch_mem_pool.sc_mem_array[posit].tc_pck.data[i] = theSchpck.tc_pck.data[i];
      }
      sc_s_state.nmbr_of_ld_sched++;
      if(find_schedule_pos() == NULL){sc_s_state.sch_arr_full = true;}
      return SATR_OK;
  }

   /**
    * @brief
    * 		Reset the schedule memory pool
    * @details
    * 		Marks every schedule position as invalid and eligible for allocation
    *     to a new scheduling packet
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_reset_pool(){

      uint8_t g = 0;
      sc_s_state.nmbr_of_ld_sched = 0;
      sc_s_state.sch_arr_full = false;

      /*mark every pos as !valid, = available*/
      for( ;g<SC_MAX_STORED_SCHEDULES;g++ ){
          sch_mem_pool.sc_mem_array[g].enabled = false;
          sch_mem_pool.sc_mem_array[g].pos_taken = false;
      }
      return SATR_OK;
  }

   /**
    * @brief
    * 		Removes a given schedule packet from the schedule array
    * @param data
    * 		Pointer to data field of tc packet
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_remove_element(uint8_t * data){

      uint16 id = 0;
      id = (*(data+1) | id) << 8;
      id = *(data+2) | id;

      for(uint8_t i=0;i<SC_MAX_STORED_SCHEDULES;i++){
          if (sch_mem_pool.sc_mem_array[i].sch_id == id){
              sch_mem_pool.sc_mem_array[i].enabled = false;
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
    *     TODO: Fix get_time()
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
      uint16_t sch_id = 0;
      uint32_t time = 0;

      /*The next 2 bytes are the id field, combine them to a uint16_t*/
      sch_id = (sch_id | tc_pkt->data[2]) << 8;
      sch_id = sch_id | tc_pkt->data[1];

      /*The next byte is for the schedule event time type*/
      (*sc_pkt).sch_evt = (SC_event_time_type) tc_pkt->data[3];
      if (!C_ASSERT((*sc_pkt).sch_evt < LAST_EVENTTIME) == true) {

          return SATR_SCHS_RLS_TT_ID_INVLD;
      }

      /*The next 4 bytes are the time fields, combine them to a uint32_t*/
      time = (time | tc_pkt->data[7]) << 8;
      time = (time | tc_pkt->data[6]) << 8;
      time = (time | tc_pkt->data[5]) << 8;
      time = (time | tc_pkt->data[4]);

      // Relative and repetitive times convert to unix timestamps
      switch((*sc_pck).sch_evt){
        case RELATIVE:
          time += get_time();
      }

      (*sc_pkt).release_time = time;
      (*sc_pkt).pos_taken = true;
      (*sc_pkt).enabled = true;

      /*copy the internal TC packet for future use*/
      /*  tc_pkt is a TC containing 8 bytes of data related to scheduling service.
       *  After those 8 bytes, a 'whole_inner_tc' packet starts.
       */
      return scheduling_copy_inner_tc( &(tc_pkt->data[8]), &((*sc_pkt).tc_pck), (uint16_t) tc_pkt->len - 8);
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
    *           Checking the CRC?
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

      //uint8_t tmp_crc[2];
      if(!C_ASSERT(buf != NULL && pkt != NULL && pkt->data != NULL) == true)  { return SATR_ERROR; }
      if(!C_ASSERT(size < MAX_PKT_SIZE) == true)                              { return SATR_ERROR; }

      //tmp_crc[0] = buf[size - 1];
      //checkSum(buf, size-2, &tmp_crc[1]); /* -2 for excluding the checksum bytes*/

      cnv8_16((uint8_t*)&buf[0], &pkt->length);
      cnv8_32((uint8_t*)&buf[2], &pkt->id);

      }

      for(int i = 0; i < pkt->length; i++) {
          pkt->data[i] = buf[6+i];
      }

      return SATR_OK;
  }

   /**
    * @brief
    * 		Time shift all currently active schedules
    * @details
    * 		Add/subtract the execution time
    *     TODO: Fix get_time()
    * @param time_v
    * 		Pointer to time value to be added/subtracted
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_time_shift_all(uint8_t *time_v){

      uint32_t ushift_time = 0;
      uint32_t time_now = get_time()
      cnv8_32(time_v+1, &ushift_time);
      for(uint8_t pos = 0; pos < SC_MAX_STORED_SCHEDULES; pos++) {
          if( sch_mem_pool.sc_mem_array[pos].pos_taken == true){
              switch(sch_mem_pool.sc_mem_array[pos].sch_evt){
                case RELATIVE:
                case ABSOLUTE:
                      uint32_t rele_time = sch_mem_pool.sc_mem_array[pos].release_time;
                      uint8_t neg = (ushift_time >> 31) & 0x1;
                      uint32_t shift_time_val = (~ushift_time)+ 1;
                      uint32_t new_release_t = 0;
                      if(neg){ /*then subtract it from release time*/
                          if(shift_time_val >= rele_time){ /*subtraction not possible, erroneous state*/
                              continue;
                          }
                          new_release_t = rele_time - shift_time_val;
                          if((new_release_t < time_now)){
                              continue;
                          }
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          continue;
                      }
                      /*then add it to release time*/
                        new_release_t = rele_time + ushift_time;
                      if( new_release_t <= MAX_RELEASE_TIME ){ /*too far to execute, we will not exist until then*/
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          return SATR_OK;
                      }
                      break;
                  }
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
    *     TODO: Fix get_time()
    * @param data_v
    * 		Pointer to start of scheduling telecommand data field
    * @return
    * 		The execution state
    */
  SAT_returnState scheduling_time_shift_sel(uint8_t *data_v){

      uint16_t id = 0;
      cnv8_16(&data_v[1], &id);
      uint32_t ushift_time = 0;
      cnv8_32(&data_v[3], &ushift_time);

      uint32_t time_now = get_time();
      for (uint8_t pos = 0; pos < SC_MAX_STORED_SCHEDULES; pos++) {
          if (sch_mem_pool.sc_mem_array[pos].sch_id == id &&
              sch_mem_pool.sc_mem_array[pos].enabled == true ){
              switch(sch_mem_pool.sc_mem_array[pos].sch_evt){
                  case RELATIVE:
                  case ABSOLUTE:
                      uint32_t rele_time = sch_mem_pool.sc_mem_array[pos].release_time;
                      uint8_t neg = (ushift_time >> 31) & 0x1;
                      uint32_t shift_time_val = (~ushift_time)+ 1;
                      uint32_t new_release_t = 0;
                      if(neg){ /*then subtract it from release time*/
                          if(shift_time_val >= rele_time){ /*subtraction not possible, erroneous state*/
                              return SATR_ERROR;
                          }
                          new_release_t = rele_time - shift_time_val;
                          if((new_release_t < time_now)){
                              return SATR_ERROR;
                          }
                          sch_mem_pool.sc_mem_array[pos].release_time = new_release_t;
                          return SATR_OK;
                      }
                      /*then add it to release time*/
                      new_release_t = rele_time + ushift_time;
                      if( new_release_t <= MAX_RELEASE_TIME ){ /*to far to execute, we will not exist until then*/
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
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].sch_id
          base+=1;
          pkt->data[base] = sch_mem_pool.sc_mem_array[i].sch_evt;
          base+=1;
          cnv32_8(sch_mem_pool.sc_mem_array[i].release_time, &pkt->data[base]);
          base+=4;
      }
      pkt->length = 8*SC_MAX_STORED_SCHEDULES;
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
  SAT_returnState scheduling_service_report_detailed(csp_packet_t *pkt, TC_TM_app_id dest_id, uint16_t sch_id){

      scheduling_service_crt_pkt_TM(pkt, SCHS_DETAILED_SCH_REPORT, dest_id);
      for(uint8_t i = 0; i < SC_MAX_STORED_SCHEDULES; i++){
          if(sch_mem_pool.sc_mem_array[i].sch_id == sch_id){
              uint8_t base = 0;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].pos_taken;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].enabled;
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].sch_id
              base+=1;
              pkt->data[base] = sch_mem_pool.sc_mem_array[i].sch_evt;
              base+=1;
              cnv32_8(sch_mem_pool.sc_mem_array[i].release_time, &pkt->data[base]);
              base+=4;
              cnv16_8(sch_mem_pool.sc_mem_array[i].tc_pck.length, &pkt->data[base]);
              base+=2;
              cnv32_8(sch_mem_pool.sc_mem_array[i].tc_pck.id, &pkt->data[base]);
              base+=4;
              for(uint16_t p=0;p<sch_mem_pool.sc_mem_array[i].tc_pck.len;p++){
                  pkt->data[base] = sch_mem_pool.sc_mem_array[i].tc_pck.data[p];
                  base+=1;
              }
              base+=1;
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

      crt_pkt(pkt, SYSTEM_APP_ID, TM, TC_ACK_NO, TC_SCHEDULING_SERVICE, sid, dest_app_id);
      pkt->len = 0;
      return SATR_OK;
  }

  /**
   * @brief
   * 		Saves schedule pool to persistent storage
   * @details
   * 		Everything is saved sequentially in a buffer, one after another
   *    TODO: Using check?
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
              cnv16_8(sch_mem_pool.sc_mem_array[s].sch_id, &sche_tc_buffer[f_s]);
              f_s+=2;
              sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].enabled;
              f_s+=1;
              sche_tc_buffer[f_s] = (uint8_t)sch_mem_pool.sc_mem_array[s].sch_evt;
              f_s+=1;
              cnv32_8(sch_mem_pool.sc_mem_array[s].release_time,&sche_tc_buffer[f_s]); //11
              f_s+=4;
              /*TC parsing begins here*/
              cnv32_8(sch_mem_pool.sc_mem_array[s].tc_pck.id, sche_tc_buffer[f_s]);
              f_s+=4;
              /*copy tc payload data*/
              uint16_t i = 0;
              for(;i<sch_mem_pool.sc_mem_array[s].tc_pck.length;i++){
                  sche_tc_buffer[f_s] = sch_mem_pool.sc_mem_array[s].tc_pck.data[i];
                  f_s+=1;
              }
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
            cnv8_16(temp, &sch_mem_pool.sc_mem_array[s].tc_pck.length);

            if(read(filedesc, sche_tc_buffer, &sch_mem_pool.sc_mem_array[s].tc_pck.length + 12) < 0){
              return SATR_ERROR;
            }else{
              state = SATR_OK;
            }

            if( state == SATR_OK){
                uint16_t f_s=0;
                /*start loading the sch packet*/
                cnv8_16(sche_tc_buffer[f_s], sch_mem_pool.sc_mem_array[s].sch_id);
                f_s+=2;
                sch_mem_pool.sc_mem_array[s].enabled = sche_tc_buffer[f_s];
                f_s+=1;
                sch_mem_pool.sc_mem_array[s].sch_evt = (SC_event_time_type) sche_tc_buffer[f_s];
                f_s+=1;
                cnv8_32(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].release_time);
                f_s+=4;
                /*TC parsing begins here*/
                cnv8_32(&sche_tc_buffer[f_s], &sch_mem_pool.sc_mem_array[s].tc_pck.id);
                f_s+=4;
                /*copy tc payload data*/
                uint16_t i = 0;
                for(;i<sch_mem_pool.sc_mem_array[s].tc_pck.length;i++){
                    sch_mem_pool.sc_mem_array[s].tc_pck.data[i] = sche_tc_buffer[f_s];
                    f_s+=1;
                }
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
