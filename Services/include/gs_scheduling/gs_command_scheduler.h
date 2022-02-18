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

/*
 * gs_command_scheduler.h
 *
 *  Created on: Nov. 22, 2021
 *      Author: Grace Yi
 */

#ifndef EX2_SYSTEM_INCLUDE_GS_COMMAND_SCHEDULER_H_
#define EX2_SYSTEM_INCLUDE_GS_COMMAND_SCHEDULER_H_

#include "os_task.h"
#include "system.h"
#include <FreeRTOS.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "rtcmk.h"
#include "csp_types.h"
#include "ex2_time.h"

char fileName1[] = "VOL0:/gs_cmds.TMP";
char fileName2[] = "VOL0:/gs_cmds_rep.TMP";
char EOL = '\n';
char space = ' ';
char asterisk = '*';
int delay_aborted = 0;

#define MAX_NUM_CMDS 2
#define MAX_CMD_LENGTH 50
#define MAX_BUFFER_LENGTH 500
#define ASTERISK                                                                                                  \
    255 //'*' is binary 42 in ASCII.
    //TODO: convert the asterisk from 42 to 255 on the python gs code
        // Since seconds and minutes can both use the value 42, to avoid conflict, asterisk will be replaced with 255.

static char cmd_buff[MAX_BUFFER_LENGTH];

typedef enum { SUCCESS = 0, FAILURE = 1 } Scheduler_Result;

//struct tm {
//   uint8_t tm_sec;         /* seconds, range 0 to 59, use * for repetition */
//   uint8_t tm_min;         /* minutes, range 0 to 59, use * for repetition */
//   uint8_t tm_hour;        /* hours, range 0 to 23, use * for repetition   */
//   uint8_t tm_mday;        /* day of the month, range 1 to 31              */
//   uint8_t tm_mon;         /* month, range 0 to 11                         */
//   uint8_t tm_year;        /* The number of years since 1900               */
//   uint8_t tm_wday;        /* day of the week, range 0 to 6                */
//   uint16_t tm_yday;       /* day in the year, range 0 to 365              */
//   uint8_t tm_isdst;       /* daylight saving time                         */
//}


// Structure inspired by: https://man7.org/linux/man-pages/man5/crontab.5.html
typedef struct __attribute__((packed)) {
    // TODO: determine if second accuracy is needed
    tmElements_t scheduled_time;
    char gs_command[MAX_CMD_LENGTH]; // place holder for storing commands, increase/decrease size as needed
} scheduled_commands_t;

typedef struct __attribute__((packed)) {
    // TODO: determine if second accuracy is needed
    uint32_t unix_time;
    uint32_t frequency; //frequency the cmd needs to be executed in seconds, value of 0 means the cmd is not repeated
    char gs_command[MAX_CMD_LENGTH]; // place holder for storing commands, increase/decrease size as needed
} scheduled_commands_unix_t;

typedef struct __attribute__((packed)) {
    // TODO: determine if second accuracy is needed
    uint8 non_rep_cmds;
    uint8 rep_cmds;
} number_of_cmds_t;

static number_of_cmds_t num_of_cmds;

//scheduled_commands_t scheduled_command[MAX_NUM_CMDS];

SAT_returnState gs_cmds_scheduler_service_app(csp_packet_t *gs_cmds);
SAT_returnState start_gs_scheduler_service(void *param);
SAT_returnState get_scheduled_gs_command();
SAT_returnState calc_cmd_frequency(scheduled_commands_t* cmds, int number_of_cmds, scheduled_commands_unix_t *sorted_cmds);
SAT_returnState sort_cmds(scheduled_commands_unix_t *sorted_cmds, int number_of_cmds);
Scheduler_Result execute_non_rep_gs_cmds(void);
Scheduler_Result execute_rep_gs_cmds(void);
static scheduled_commands_t *prv_get_cmds_scheduler();

#endif /* EX2_SYSTEM_INCLUDE_GS_COMMAND_SCHEDULER_H_ */
