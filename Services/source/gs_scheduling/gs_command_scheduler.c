/*
 * gs_command_scheduler.c
 *
 *  Created on: Nov. 22, 2021
 *      Author: Grace Yi
 */

#include "semphr.h"
#include "services.h"
#include "system.h"
#include <FreeRTOS.h>
#include <string.h>
#include <csp/csp.h>
#include "housekeeping_service.h"
#include "task_manager.h"
#include "gs_command_scheduler.h"
#include "os_task.h"
#include "services.h"
#include <redposix.h> //include for file system

/**
 * @brief
 *      Private. Collect scheduled commands from the groundstation
 * @param gs_cmds
 *      pointer to the struct of all the gs commands
 * @return SAT_returnState
 *      SATR_OK or SATR_ERROR
 */
SAT_returnState gs_cmds_scheduler_service_app(csp_packet_t *gs_cmds) {
    TaskHandle_t SchedulerHandler;
    // allocating buffer for MAX_NUM_CMDS numbers of incoming commands
    scheduled_commands_t *cmds = (scheduled_commands_t*)calloc(MAX_NUM_CMDS, sizeof(scheduled_commands_t));
    // parse the commands
    int number_of_cmds = prv_set_gs_scheduler(&gs_cmds, &cmds);
    // calculate frequency of cmds. Non-repetitive commands have a frequency of 0
    scheduled_commands_unix_t *sorted_cmds = (scheduled_commands_unix_t*)calloc(number_of_cmds, sizeof(scheduled_commands_unix_t));
    calc_cmd_frequency(&cmds, number_of_cmds, &sorted_cmds);
    // open file that stores the cmds in the SD card
    int32_t fout = red_open(fileName1, RED_O_RDONLY | RED_O_RDWR);

    // if file does not exist, create a scheduler
    if (fout == -1) {
        // sort the commands
        sort_cmds(&sorted_cmds, number_of_cmds);
        // write cmds to file
        write_cmds_to_file(1, &sorted_cmds, number_of_cmds, fileName1);
        // create the scheduler
        //TODO: review stack size
        xTaskCreate(vSchedulerHandler, "scheduler", 1000, NULL, GS_SCHEDULER_TASK_PRIO, SchedulerHandler);
    }

    // if file already exists, modify the existing scheduler
    else if (fout != -1) {
        // TODO: use mutex/semaphores to protect the file while being written
        // get file size through file stats
        REDSTAT scheduler_stat;
        int32_t f_stat = red_fstat(fout, &scheduler_stat);
        if (f_stat == -1) {
            printf("Unexpected error %d from f_stat()\r\n", (int)red_errno);
            ex2_log("Failed to read file stats from: '%s'\n", fileName1);
            red_close(fout);
            return FAILURE;
        }
        // get number of existing cmds
        uint32_t num_existing_cmds = scheduler_stat.st_size / sizeof(scheduled_commands_unix_t);
        int total_cmds = number_of_cmds + num_existing_cmds;
        // TODO: use error handling to check calloc was successful
        scheduled_commands_unix_t *existing_cmds = (scheduled_commands_unix_t*)calloc(num_existing_cmds, sizeof(scheduled_commands_unix_t));
        scheduled_commands_unix_t *updated_cmds = (scheduled_commands_unix_t*)calloc(total_cmds, sizeof(scheduled_commands_unix_t));
        // read file
        int32_t f_read = red_read(fout, &existing_cmds, (uint32_t)scheduler_stat.st_size);
        if (f_read == -1) {
            printf("Unexpected error %d from red_read()\r\n", (int)red_errno);
            ex2_log("Failed to read file: '%s'\n", fileName1);
            red_close(fout);
            return FAILURE;
        }
        // combine new commands and old commands into a single struct for sorting
        memcpy(&updated_cmds,&sorted_cmds,sizeof(sorted_cmds));
        memcpy((updated_cmds+number_of_cmds),&existing_cmds,sizeof(existing_cmds));
        sort_cmds(&updated_cmds, total_cmds);
        // write new cmds to file
        write_cmds_to_file(1, &updated_cmds, total_cmds, fileName1);
        // close file
        red_close(fout);
        // set Abort delay flag to 1
        if (xTaskAbortDelay(SchedulerHandler) == pdPASS) {
            delay_aborted = 1;
        }
        // free calloc
        free(existing_cmds);
        free(updated_cmds);
    }

    // free calloc
    free(cmds);
    free(sorted_cmds);

    /*consider if struct should hold error codes returned from these functions*/
    return SATR_OK;
}

/*------------------------------Private-------------------------------------*/

/**
 * @brief
 *      Parse and store groundstation commands from the buffer to the array @param cmds
 * @param cmd_buff
 *      pointer to the buffer that stores the groundstation commands
 * @return Scheduler_Result
 *      FAILURE or SUCCESS
 */
int prv_set_gs_scheduler(char *cmd_buff, scheduled_commands_t *cmds) {
    int number_of_cmds = 0;
    // Parse the commands
    // Initialize counters that point to different locations in the string of commands
    int old_str_position = 0;
    int str_position_1 = 0;
    int str_position_2 = 0;

    while (number_of_cmds < MAX_NUM_CMDS) {
        // A carraige followed by a space or nothing indicates there is no more commands
        // TODO: determine if this is the best way to detect the end of the gs cmd script
        if ((cmd_buff[str_position_1] == EOL && cmd_buff[str_position_1 + 1] == space) || (cmd_buff[str_position_1] == EOL && cmd_buff[str_position_1 + 1] == "")) {
            break;
        }
        /*-----------------------Fetch time in seconds-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Second = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Second = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Second = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch time in minutes-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit value
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Minute = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Minute = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Minute = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch time in hour-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit value
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Minute = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Minute = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Hour = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch the month-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit value
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Month = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Month = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Month = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch the number of years since 1900-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit value
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Year = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Year = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Year = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch day of the week-----------------------*/
        //Count the number of digits
        if (cmd_buff[str_position_1] != space) {
            str_position_1++;
        }
        str_position_2 = str_position_1;
        //Count the number of spaces following the digits
        if (cmd_buff[str_position_2] == space) {
            str_position_2++;
        }
        //Single digit value
        if (str_position_2 - str_position_1 == 1) {
            if (cmd_buff[str_position_1 - 1] == asterisk) {
                (cmds + number_of_cmds)->scheduled_time.Wday = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.Wday = atoi(cmd_buff[str_position_1 - 1]);
            }
        }
        //Multiple digits
        if (str_position_2 - str_position_1 != 1) {
            //Create a buffer to store characters before the space
            char substring [str_position_2 - old_str_position];
            //Initialize buffer
            memset(substring, "", (str_position_2 - old_str_position));
            memcpy(substring, cmd_buff[old_str_position], (str_position_2 - old_str_position));
            //Convert string in the buffer into integer value
            (cmds + number_of_cmds)->scheduled_time.Wday = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;

        /*-----------------------Fetch gs command-----------------------*/
        //Count the number of characters in the command
        if (cmd_buff[str_position_2] == EOL) {
            //TODO: discuss whether this is the best way to log error
            ex2_log("command is empty");
        }
        if (cmd_buff[str_position_2] != EOL) {
            str_position_2++;
        }
        //Store command into structure
        memcpy((cmds + number_of_cmds)->gs_command, cmd_buff[old_str_position], (str_position_2 - old_str_position));

        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        number_of_cmds++;
    }
    
    return number_of_cmds;
}

/**
 * @brief
 *      For non-repetitive commands: turn scheduled time into unix time, and set frequency of command to 0
 *      For repetitive commands: calculate the unix time of the first execution, and the frequency each command will be executed at
 * @param cmds
 *      pointer to the structure that stores the parsed groundstation commands
 * @param number_of_cmds
 *      number of commands in @param cmds
 * @param sorted_cmds
 *      pointer to the structure that stores the groundstation commands after frequency has been calculated
 * @return SAT_returnState
 *      SATR_OK or SATR_ERROR
 */
SAT_returnState calc_cmd_frequency(scheduled_commands_t *cmds, int number_of_cmds, scheduled_commands_unix_t *sorted_cmds) {
    /*--------------------------------Initialize structures to store sorted commands--------------------------------*/
    //TODO: Confirm that the entire struct has been initialized with zeros
    scheduled_commands_unix_t *non_reoccurring_cmds = (scheduled_commands_unix_t*)calloc(number_of_cmds, sizeof(scheduled_commands_unix_t));
    scheduled_commands_t *reoccurring_cmds = (scheduled_commands_t*)calloc(number_of_cmds, sizeof(scheduled_commands_t));
    num_of_cmds.non_rep_cmds = 0;
    num_of_cmds.rep_cmds = 0;

    int unix_time_buff;
    int j_rep = 0;
    int j_non_rep = 0;

    for (int j = 0; j < number_of_cmds; j++) {
        // Separate the non-repetitve and repetitve commands, the sum of time fields should not exceed the value of ASTERISK (255) if non-repetitive
        if ((cmds+j)->scheduled_time.Second + (cmds+j)->scheduled_time.Minute + (cmds+j)->scheduled_time.Hour + (cmds+j)->scheduled_time.Month < ASTERISK) {

            // Store the non-repetitve commands into the new struct non_reoccurring_cmds
            if (j_non_rep < number_of_cmds) {
                // Convert the time into unix time for sorting convenience
                unix_time_buff = makeTime(cmds->scheduled_time);
                (non_reoccurring_cmds+j_non_rep)->unix_time = unix_time_buff;
                if( unix_time_buff == -1 ) {
                    // TODO: create error handling routine to handle this error during cmd sorting and execution
                    ex2_log("Error: unable to make time using makeTime\n");
                    // TODO: delete this cmd if makeTime fails
                }
                memcpy((non_reoccurring_cmds+j_non_rep)->gs_command, (cmds+j)->gs_command, MAX_CMD_LENGTH);
                (non_reoccurring_cmds+j_non_rep)->frequency = 0; //set frequency to zero for non-repetitive cmds
                j_non_rep++;
            }
        
            else if (j_rep < number_of_cmds) {
                // Store the repetitve commands into the new struct reoccurring_cmds
                memcpy(reoccurring_cmds+j_rep, cmds+j, 10 + MAX_CMD_LENGTH);
                j_rep++;
            }
        }
    }

    // update the number of commands
    num_of_cmds.non_rep_cmds = j_non_rep;
    num_of_cmds.rep_cmds = j_rep;

    /*--------------------------------calculate the frequency of repeated cmds--------------------------------*/
    struct tmElements_t time_buff;
    //TODO: check that all callocs have been freed
    scheduled_commands_unix_t *repeated_cmds_buff = (scheduled_commands_unix_t*)calloc(j_rep, sizeof(scheduled_commands_unix_t));
    // Obtain the soonest time that the command will be executed, and calculate the frequency it needs to be executed at
    for (int j=0; j < j_rep; j++) {
        time_buff.Wday = (reoccurring_cmds+j)->scheduled_time.Wday;
        time_buff.Month = (reoccurring_cmds+j)->scheduled_time.Month;
        memcpy((reoccurring_cmds+j)->gs_command,(repeated_cmds_buff+j)->gs_command,sizeof((repeated_cmds_buff+j)->gs_command));
        // If command repeats every second
        if ((reoccurring_cmds+j)->scheduled_time.Hour == asterisk && (reoccurring_cmds+j)->scheduled_time.Minute == asterisk && (reoccurring_cmds+j)->scheduled_time.Second == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buff.Hour);
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buff.Minute);
            RTCMK_ReadSeconds(RTCMK_ADDR, &time_buff.Second);
            //convert the first execution time into unix time. Add 60 seconds to allow processing time
            (repeated_cmds_buff+j)->unix_time = makeTime(time_buff) + 60;
            (repeated_cmds_buff+j)->frequency = 1; //1 second
            continue;
        }
        // If command repeats every minute
        if ((reoccurring_cmds+j)->scheduled_time.Hour == asterisk && (reoccurring_cmds+j)->scheduled_time.Minute == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buff.Hour);
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buff.Minute);
            time_buff.Second = (reoccurring_cmds+j)->scheduled_time.Second;
            //convert the first execution time into unix time. Add 60 seconds to allow processing time
            (repeated_cmds_buff+j)->unix_time = makeTime(time_buff) + 60;
            (repeated_cmds_buff+j)->frequency = 60; //1 min
            continue;
        }
        // If command repeats every hour
        if ((reoccurring_cmds+j)->scheduled_time.Hour == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buff.Hour);
            time_buff.Minute = (reoccurring_cmds+j)->scheduled_time.Minute;
            time_buff.Second = (reoccurring_cmds+j)->scheduled_time.Second;
            //convert the first execution time into unix time. If the hour is almost over, increase the hour by one
            time_t current_time;
            time_t scheduled_time = makeTime(time_buff);
            if (scheduled_time - RTCMK_GetUnix(&current_time) < 60) {
                (repeated_cmds_buff+j)->unix_time = scheduled_time + 3600;
            }
            else {
                (repeated_cmds_buff+j)->unix_time = scheduled_time;
            }
            (repeated_cmds_buff+j)->frequency = 3600; //1 hr
            continue;
        }
    }

    /*--------------------------------Combine non-repetitive and repetitive commands into a single struct--------------------------------*/
    memcpy(sorted_cmds,repeated_cmds_buff,sizeof(repeated_cmds_buff);
    memcpy((sorted_cmds+j_rep),non_reoccurring_cmds,sizeof(scheduled_commands_unix_t)*j_non_rep);

    // free calloc
    free(non_reoccurring_cmds);
    free(reoccurring_cmds);
    free(repeated_cmds_buff);
    
    //prv_give_lock(cmds);
    return &SATR_OK;
}

/**
 * @brief
 *      Sort groundstation commands from the lowest unix time to highest unix time
 * @param sorted_cmds
 *      pointer to the structure that stores the groundstation commands
 * @param number_of_cmds
 *      number of commands in the struct @param sorted_cmds
 * @return SAT_returnState
 *      SATR_OK or SATR_ERROR
 */
SAT_returnState sort_cmds(scheduled_commands_unix_t *sorted_cmds, int number_of_cmds) {
/*--------------------------------Sort the list using selection sort--------------------------------*/
    scheduled_commands_unix_t sorting_buff;
    int ptr1, ptr2, min_ptr;
    for (ptr1 = 0; ptr1 < number_of_cmds; ptr1++) {
        for (ptr2 = ptr1+1; ptr2 < number_of_cmds; ptr2++) {
            //find minimum unix time
            if ((sorted_cmds+ptr1)->unix_time < (sorted_cmds+ptr2)->unix_time) {
                min_ptr = ptr1;
            }
            else {
                min_ptr = ptr2;
            }
        }
        //swap the minimum with the current
        if ((sorted_cmds+ptr1)->unix_time != (sorted_cmds+min_ptr)->unix_time) {
            memcpy(sorting_buff, (sorted_cmds+ptr1), sizeof(scheduled_commands_unix_t));
            memcpy((sorted_cmds+ptr1), (sorted_cmds+min_ptr), sizeof(scheduled_commands_unix_t));
            memcpy((sorted_cmds+min_ptr), sorting_buff, sizeof(scheduled_commands_unix_t));
        }
    }
    return &SATR_OK;
}


//TODO: consider writing a function to retrieve the list of scheduled commands
/*static scheduled_commands_t *prv_get_cmds_scheduler() {
    //if (!prvEps.eps_lock) {
        //prvEps.eps_lock = xSemaphoreCreateMutex();
    //}
    //configASSERT(prvEps.eps_lock);
    return &prvGS_cmds;
}
*/

/*--------------------------------Private----------------------------------------*/

/**
 * @brief
 *      Write groundstation commands to the given file location
 * @details
 *      Writes scheduled groundstation commands to the SD card
 *      Order of writes must match the appropriate read function
 * @param filenumber
 *     uint16_t number to seek to in file
 * @param scheduled_cmds
 *      Struct containing groundstation commands to be executed at a given time
 * @return Scheduler_Result
 *      FAILURE or SUCCESS
 */
Scheduler_Result write_cmds_to_file(uint16_t filenumber, scheduled_commands_unix_t *scheduled_cmds, int number_of_cmds, char fileName) {
    int32_t fout = red_open(fileName, RED_O_CREAT | RED_O_RDWR); // open or create file to write binary
    if (fout == -1) {
        printf("Unexpected error %d from red_open()\r\n", (int)red_errno);
        ex2_log("Failed to open or create file to write: '%s'\n", fileName);
        return FAILURE;
    }
    uint32_t needed_size = sizeof(number_of_cmds * sizeof(scheduled_commands_unix_t));

    red_errno = 0;
    /*The order of writes and subsequent reads must match*/
    red_write(fout, &scheduled_cmds, needed_size);
    // red_write(fout, &all_hk_data->hk_timeorder, sizeof(all_hk_data->hk_timeorder));
    // red_write(fout, &all_hk_data->Athena_hk, sizeof(all_hk_data->Athena_hk));
    // red_write(fout, &all_hk_data->EPS_hk, sizeof(all_hk_data->EPS_hk));

    if (red_errno != 0) {
        ex2_log("Failed to write to file: '%s'\n", fileName);
        red_close(fout);
        return FAILURE;
    }
    red_close(fout);
    return SUCCESS;
}

/*//TODO: is a watchdog needed?
static uint32_t svc_wdt_counter = 0;
static uint32_t get_svc_wdt_counter() { return svc_wdt_counter; }
SAT_returnState start_gs_cmds_scheduler_service(void);
*/

/**
 * @brief
 *      FreeRTOS gs scheduler server task
 * @details
 *      Accepts incoming gs scheduler packets and executes the application
 * @param void* param
 * @return SAT_returnState
 */
SAT_returnState start_gs_scheduler_service(void *param) {
    csp_socket_t *sock;
    sock = csp_socket(CSP_SO_RDPREQ);
    csp_bind(sock, TC_GS_SCHEDULER_SERVICE);
    csp_listen(sock, SERVICE_BACKLOG_LEN);   // TODO: SERVICE_BACKLOG_LEN constant TBD
    //svc_wdt_counter++;

    for (;;) {
        csp_conn_t *conn;
        csp_packet_t *packet;
        if ((conn = csp_accept(sock, CSP_MAX_TIMEOUT)) == NULL) {
            /* timeout */
            continue;
        }
        //TODO: is a watchdog needed?
        //svc_wdt_counter++;

        while ((packet = csp_read(conn, 50)) != NULL) {
            if (gs_cmds_scheduler_service_app(&packet) != SATR_OK) {
                //TODO: define max # of commands that can be scheduled per CSP packet, incorporate this limit into the gs ops manual
                ex2_log("Error responding to packet");
            }
        }
        csp_close(conn); // frees buffers used
    }
}


///*------------------------------------------------------------------------------------------*/
//    csp_conn_t *conn;
//    if (!csp_send(conn, packet, 50)) { // why are we all using magic number?
//            ex2_log("Failed to send packet");
//            csp_buffer_free(packet);
//            return FAILURE;
//        }
//
///*------------------------------------------------------------------------------------------*/
//int csp_sendto_reply(const csp_packet_t * request_packet, csp_packet_t * reply_packet, uint32_t opts, uint32_t timeout) {
//	if (request_packet == NULL)
//		return CSP_ERR_INVAL;
//
//	return csp_sendto(request_packet->id.pri, request_packet->id.src, request_packet->id.sport, request_packet->id.dport, opts, reply_packet, timeout);
//}
//
///**
//   Send a packet as a reply to a request (without a connection).
//   Calls csp_sendto() with the source address and port from the request.
//   @param[in] request incoming request
//   @param[in] reply reply packet
//   @param[in] opts connection options, see @ref CSP_CONNECTION_OPTIONS. -->refer to RDP, this is a bit/flag in the header, but for now this goes into the opt, obc won't respond unless this is present
//   @param[in] timeout unused as of CSP version 1.6
//   @return #CSP_ERR_NONE on success, otherwise an error code and the reply must be freed by calling csp_buffer_free().
//*/
//int csp_sendto_reply(const csp_packet_t * request, csp_packet_t * reply, uint32_t opts, uint32_t timeout);
