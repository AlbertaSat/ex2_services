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
 * @file adcs_service.h
 * @author Quoc Trung Tran
 * @date Jul 08, 2021
 */

#ifndef EX2_SERVICES_SERVICES_INCLUDE_ADCS_ADCS_SERVICE_H_
#define EX2_SERVICES_SERVICES_INCLUDE_ADCS_ADCS_SERVICE_H_

#include <FreeRTOS.h>
#include "services.h"
#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <main/system.h>
#include "services.h"
#include "adcs.h"
#include "service_utilities.h"

typedef enum {
    ADCS_RESET,
    ADCS_RESET_LOG_POINTER,
    ADCS_ADVANCE_LOG_POINTER,
    ADCS_RESET_BOOT_REGISTERS,
    ADCS_FORMAT_SD_CARD,
    ADCS_ERASE_FILE,
    ADCS_LOAD_FILE_DOWNLOAD_BLOCK,
    ADCS_ADVANCE_FILE_LIST_READ_POINTER,
    ADCS_INITIATE_FILE_UPLOAD,
    ADCS_FILE_UPLOAD_PACKET,
    ADCS_FINALIZE_UPLOAD_BLOCK,
    ADCS_RESET_UPLOAD_BLOCK,
    ADCS_RESET_FILE_LIST_READ_POINTER,
    ADCS_INITIATE_DOWNLOAD_BURST,
    ADCS_GET_NODE_IDENTIFICATION,
    ADCS_GET_BOOT_PROGRAM_STAT,
    ADCS_GET_BOOT_INDEX,
    ADCS_GET_LAST_LOGGED_EVENT,
    ADCS_GET_SD_FORMAT_PROCESS,
    ADCS_GET_TC_ACK,
    ADCS_GET_FILE_DOWNLOAD_BUFFER,
    ADCS_GET_FILE_DOWNLOAD_BLOCK_STAT,
    ADCS_GET_FILE_INFO,
    ADCS_GET_INIT_UPLOAD_STAT,
    ADCS_GET_FINALIZE_UPLOAD_STAT,
    ADCS_GET_UPLOAD_CRC16_CHECKSUM,
    ADCS_GET_SRAM_LATCHUP_COUNT,
    ADCS_GET_EDAC_ERR_COUNT,
    ADCS_GET_COMMS_STAT,
    ADCS_SET_CACHE_EN_STATE,
    ADCS_SET_SRAM_SCRUB_SIZE,
    ADCS_SET_UNIXTIME_SAVE_CONFIG,
    ADCS_SET_HOLE_MAP,
    ADCS_SET_UNIX_T,
    ADCS_GET_CACHE_EN_STATE,
    ADCS_GET_SRAM_SCRUB_SIZE,
    ADCS_GET_UNIXTIME_SAVE_CONFIG,
    ADCS_GET_HOLE_MAP,
    ADCS_GET_UNIX_T,
    ADCS_CLEAR_ERR_FLAGS,
    ADCS_SET_BOOT_INDEX,
    ADCS_RUN_SELECTED_PROGRAM,
    ADCS_READ_PROGRAM_INFO,
    ADCS_COPY_PROGRAM_INTERNAL_FLASH,
    ADCS_GET_BOOTLOADER_STATE,
    ADCS_GET_PROGRAM_INFO,
    ADCS_COPY_INTERNAL_FLASH_PROGRESS,
    ADCS_DEPLOY_MAGNETOMETER_BOOM,
    ADCS_SET_ENABLED_STATE,
    ADCS_CLEAR_LATCHED_ERRS,
    ADCS_SET_ATTITUDE_CTR_MODE,
    ADCS_SET_ATTITUDE_ESTIMATE_MODE,
    ADCS_TRIGGER_ADCS_LOOP,
    ADCS_TRIGGER_ADCS_LOOP_SIM,
    ADCS_SET_ASGP4_RUNE_MODE,
    ADCS_TRIGGER_ASGP4,
    ADCS_SET_MTM_OP_MODE,
    ADCS_CNV2JPG,
    ADCS_SAVE_IMG,
    ADCS_SET_MAGNETORQUER_OUTPUT,
    ADCS_SET_WHEEL_SPEED,
    ADCS_SAVE_CONFIG,
    ADCS_SAVE_ORBIT_PARAMS,
    ADCS_GET_CURRENT_STATE,
    ADCS_GET_JPG_CNV_PROGESS,
    ADCS_GET_CUBEACP_STATE,
    ADCS_GET_SAT_POS_LLH,
    ADCS_GET_EXECUTION_TIMES,
    ADCS_GET_ACP_LOOP_STAT,
    ADCS_GET_IMG_SAVE_PROGRESS,
    ADCS_GET_MEASUREMENTS,
    ADCS_GET_ACTUATOR,
    ADCS_GET_ESTIMATION,
    ADCS_GET_ASGP4,
    ADCS_GET_RAW_SENSOR,
    ADCS_GET_RAW_GPS,
    ADCS_GET_STAR_TRACKER,
    ADCS_GET_MTM2_MEASUREMENTS,
    ADCS_GET_POWER_TEMP,
    ADCS_SET_POWER_CONTROL,
    ADCS_GET_POWER_CONTROL,
    ADCS_SET_ATTITUDE_ANGLE,
    ADCS_GET_ATTITUDE_ANGLE,
    ADCS_SET_TRACK_CONTROLLER,
    ADCS_GET_TRACK_CONTROLLER,
    ADCS_SET_LOG_CONFIG,
    ADCS_GET_LOG_CONFIG,
    ADCS_SET_INERTIAL_REF,
    ADCS_GET_INERTIAL_REF,
    ADCS_SET_SGP4_ORBIT_PARAMS,
    ADCS_GET_SGP4_ORBIT_PARAMS,
    ADCS_SET_SYSTEM_CONFIG,
    ADCS_GET_SYSTEM_CONFIG,
    ADCS_SET_MTQ_CONFIG,
    ADCS_SET_RW_CONFIG,
    ADCS_SET_RATE_GYRO,
    ADCS_SET_CSS_CONFIG,
    ADCS_SET_STAR_TRACK_CONFIG,
    ADCS_SET_CUBESENSE_CONFIG,
    ADCS_SET_MTM_CONFIG,
    ADCS_SET_DETUMBLE_CONFIG,
    ADCS_SET_YWHEEL_CONFIG,
    ADCS_SET_TRACKING_CONFIG,
    ADCS_SET_MOI_MAT,
    ADCS_SET_ESTIMATION_CONFIG,
    ADCS_SET_USERCODED_SETTING,
    ADCS_SET_ASGP4_SETTING,
    ADCS_GET_FULL_CONFIG
} ADCS_Subtype;

SAT_returnState adcs_service_app(csp_packet_t *packet);
SAT_returnState start_adcs_service(void);

#endif /* EX2_SERVICES_SERVICES_INCLUDE_ADCS_ADCS_SERVICE_H_ */
