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

char fileName1[] = "VOL0:/gs_cmds_non_rep.TMP";
char fileName2[] = "VOL0:/gs_cmds_rep.TMP";
static number_of_cmds_t num_of_cmds;
char EOL = '\n';
char space = ' ';
char asterisk = '*';

/**
 * @brief
 *      Private. Collect scheduled commands from the groundstation
 * @param gs_cmds
 *      pointer to struct of all the gs commands
 * @return Result
 *      FAILURE or SUCCESS
 */
Result collect_scheduled_cmds(scheduled_commands_t *gs_cmds) {
/*populate struct by calling appropriate functions*/
#ifndef GS_SCHEDULER_IS_STUBBED
    int GS_scheduler_return_code = get_scheduled_gs_command();
#endif /* GS_SCHEDULER_IS_STUBBED */

    /*consider if struct should hold error codes returned from these functions*/
    return SUCCESS;
}

/*------------------------------Public-------------------------------------*/
/* Binary Commands to EPS */

#define SET_SCHEDULER_PERIOD 255
#define SCHEDULER_REQUEST_TIMEOUT 1000
#define GS_SCHEDULED_CMDS 8 //port TBD

//static scheduled_commands_t prvGS_cmds[MAX_NUM_CMDS];

SAT_returnState get_scheduled_gs_command() {
    //TODO: test the code with a cmd of this format: <server name> "." <service name> "." <subservice name>
    //      <arguments>
    uint8_t cmd = 0; // 'subservice' command
    //Initialize buffer
    memset(cmd_buff, 0x00, MAX_BUFFER_LENGTH);
/**
   Perform an entire request & reply transaction.
   Creates a connection, send \a outbuf, wait for reply, copy reply to \a inbuf and close the connection.
   @param[in] prio priority, see #csp_prio_t
   @param[in] dst destination address
   @param[in] dst_port destination port
   @param[in] timeout timeout in mS to wait for a reply
   @param[in] outbuf outgoing data (request)
   @param[in] outlen length of data in \a outbuf (request)
   @param[out] inbuf user provided buffer for receiving data (reply)
   @param[in] inlen length of expected reply, -1 for unknown size (inbuf MUST be large enough), 0 for no reply.
   @param[in] opts connection options, see @ref CSP_CONNECTION_OPTIONS.
   @return 1 or reply size on success, 0 on failure (error, incoming length does not match, timeout)
*/    
    csp_transaction_w_opts(CSP_PRIO_LOW, OBC_APP_ID, GS_SCHEDULED_CMDS, SCHEDULER_REQUEST_TIMEOUT, &cmd, sizeof(cmd),
                           &cmd_buff, sizeof(cmd_buff), CSP_O_CRC32);
    // TODO: data is little endian? must convert to host order
    //prv_instantaneous_telemetry_letoh(&cmds_buf);
    prv_set_gs_scheduler(cmd_buff);
    return SATR_OK;
}

/**
 * @brief
 *      Parse and store groundstation commands from the buffer to the array @param cmds
 * @param cmd_buff
 *      pointer to the buffer that stores the groundstation commands
 * @return Result
 *      FAILURE or SUCCESS
 */
static inline void prv_set_gs_scheduler(char* cmd_buff) {
    //scheduled_commands_t *cmds = prvGS_cmds;
    scheduled_commands_t *cmds;
    // allocating memory for MAX_NUM_CMDS numbers of struct scheduled_commands_t
    cmds = (scheduled_commands_t*) malloc(MAX_NUM_CMDS * sizeof(scheduled_commands_t));
    
    //prv_get_lock(cmds);
    
    //Parse the commands
    //Initialize counters that point to different locations in the string of commands
    int old_str_position = 0;
    int str_position_1 = 0;
    int str_position_2 = 0;

    //Initialize counter for number of commands parsed 
    int number_of_cmds = 0;

    while (number_of_cmds < MAX_NUM_CMDS) {
        //A carraige followed by a space or nothing indicates there is no more commands
        //TODO: determine if this is the best way to detect the end of the gs cmd script
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
                (cmds + number_of_cmds)->scheduled_time.tm_sec = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_sec = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_sec = atoi(substring);
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
                (cmds + number_of_cmds)->scheduled_time.tm_min = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_min = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_min = atoi(substring);
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
                (cmds + number_of_cmds)->scheduled_time.tm_hour = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_hour = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_hour = atoi(substring);
        }
        old_str_position = str_position_2;
        str_position_1 = str_position_2;
        str_position_1++;

        /*-----------------------Fetch day of the month-----------------------*/
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
                (cmds + number_of_cmds)->scheduled_time.tm_mday = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_mday = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_mday = atoi(substring);
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
                (cmds + number_of_cmds)->scheduled_time.tm_mon = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_mon = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_mon = atoi(substring);
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
                (cmds + number_of_cmds)->scheduled_time.tm_year = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_year = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_year = atoi(substring);
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
                (cmds + number_of_cmds)->scheduled_time.tm_wday = ASTERISK;
            }
            else {
                (cmds + number_of_cmds)->scheduled_time.tm_wday = atoi(cmd_buff[str_position_1 - 1]);
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
            (cmds + number_of_cmds)->scheduled_time.tm_wday = atoi(substring);
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

    sort_scheduled_cmds(&cmds, number_of_cmds);
    //write_cmds_to_file(1, &cmds);
    //prv_give_lock(cmds);
    return;
}

/**
 * @brief
 *      Sort and store groundstation commands from the buffer to the array @param non_reoccurring_cmds @param reoccurring_cmds
 * @param cmds
 *      pointer to the structure that stores the groundstation commands
 * @return SAT_returnState
 *      SATR_OK or SATR_ERROR
 */
SAT_returnState sort_scheduled_cmds (scheduled_commands_t* cmds, int number_of_cmds) {
    /*--------Initialize structures to store sorted commands--------*/
    //TODO: Confirm that the entire struct has been initialized with zeros
    scheduled_commands_unix_t* non_reoccurring_cmds = (scheduled_commands_unix_t*)calloc(number_of_cmds,sizeof(scheduled_commands_unix_t));
    scheduled_commands_t* reoccurring_cmds = (scheduled_commands_t*)calloc(number_of_cmds,sizeof(scheduled_commands_t));
    num_of_cmds.non_rep_cmds = 0;
    num_of_cmds.rep_cmds = 0;


    int unix_time_buff;
    int j_rep = 0;
    int j_non_rep = 0;


    for (int j = 0; j < number_of_cmds; j++) {
        //Separate the non-repetitve and repetitve commands, the sum of time fields should not exceed the value of ASTERISK (255) if non-repetitive
        if ((cmds+j)->scheduled_time.tm_sec + (cmds+j)->scheduled_time.tm_min + (cmds+j)->scheduled_time.tm_hour + (cmds+j)->scheduled_time.tm_mday + (cmds+j)->scheduled_time.tm_mon < ASTERISK) {

            //Store the non-repetitve commands into the new struct non_reoccurring_cmds
            if (j_non_rep < number_of_cmds) {
                //Convert the time into unix time for sorting convenience
                unix_time_buff = mktime(&cmds->scheduled_time);
                (non_reoccurring_cmds+j_non_rep)->unix_time = unix_time_buff;
                if( unix_time_buff == -1 ) {
                    //TODO: create error handling routine to handle this error during cmd sorting and execution
                    ex2_log("Error: unable to make time using mktime\n");
                    //TODO: delete this cmd if mktime fails
                }
                memcpy((non_reoccurring_cmds+j_non_rep)->gs_command, (cmds+j)->gs_command, MAX_CMD_LENGTH);
                j_non_rep++;
            }
        
            else if (j_rep < number_of_cmds) {
                //Store the repetitve commands into the new struct reoccurring_cmds
                memcpy(reoccurring_cmds+j_rep, cmds+j, 10 + MAX_CMD_LENGTH);
                j_rep++;
            }
        }
    } 

    //store the number of commands
    num_of_cmds.non_rep_cmds = j_non_rep;
    num_of_cmds.rep_cmds = j_rep;

    /*--------------------------------Sort the non-repetitive commands using selection sort--------------------------------*/
    scheduled_commands_unix_t sorting_buff;
    int ptr1, ptr2, min_ptr;
    for (ptr1 = 0; ptr1 < j_non_rep; ptr1++) {
        for (ptr2 = ptr1+1; ptr2 < j_non_rep; ptr2++) {
            //find minimum unix time
            if ((non_reoccurring_cmds+ptr1)->unix_time < (non_reoccurring_cmds+ptr2)->unix_time) {
                min_ptr = ptr1;
            }
            else {
                min_ptr = ptr2;
            }
        }
        //swap the minimum unix time cmd with the previous cmd
        if ((non_reoccurring_cmds+ptr1)->unix_time != (non_reoccurring_cmds+min_ptr)->unix_time) {
            sorting_buff.unix_time = (non_reoccurring_cmds+ptr1)->unix_time;
            memcpy(sorting_buff.gs_command, (non_reoccurring_cmds+ptr1)->gs_command, MAX_CMD_LENGTH);
            (non_reoccurring_cmds+ptr1)->unix_time = (non_reoccurring_cmds+min_ptr)->unix_time;
            memcpy((non_reoccurring_cmds+ptr1)->gs_command, (non_reoccurring_cmds+min_ptr)->gs_command, MAX_CMD_LENGTH);
            (non_reoccurring_cmds+min_ptr)->unix_time = sorting_buff.unix_time;
            memcpy((non_reoccurring_cmds+min_ptr)->gs_command, sorting_buff.gs_command, MAX_CMD_LENGTH);
        }
    }

    //Store the sorted non-repetitive commands into the SD card
    write_cmds_to_file(1, &non_reoccurring_cmds, j_non_rep, fileName1);
    free(non_reoccurring_cmds);

    /*------------------------------------------------Sort the repetitve commands------------------------------------------------*/
    struct tm time_buffer;
    repeated_commands_t* repeated_cmds = (repeated_commands_t*)calloc(j_rep, sizeof(repeated_commands_t));
    //Obtain the first time that the command will be executed, and calculate the frequency it needs to be executed at
    for (int j=0; j < j_rep; j++) {
        time_buffer.tm_isdst = (reoccurring_cmds+j)->scheduled_time.tm_isdst;
        time_buffer.tm_yday = (reoccurring_cmds+j)->scheduled_time.tm_yday;
        time_buffer.tm_wday = (reoccurring_cmds+j)->scheduled_time.tm_wday;
        time_buffer.tm_mon = (reoccurring_cmds+j)->scheduled_time.tm_mon;
        time_buffer.tm_mday = (reoccurring_cmds+j)->scheduled_time.tm_mday;
        //If command repeats every second
        if ((reoccurring_cmds+j)->scheduled_time.tm_hour == asterisk && (reoccurring_cmds+j)->scheduled_time.tm_min == asterisk && (reoccurring_cmds+j)->scheduled_time.tm_sec == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buffer.tm_hour);
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buffer.tm_min);
            RTCMK_ReadSeconds(RTCMK_ADDR, &time_buffer.tm_sec);
            //convert the first execution time into unix time. Add 60 seconds to allow processing time
            (repeated_cmds+j)->unix_time = mktime(&time_buffer) + 60;
            (repeated_cmds+j)->frequency = 1; //1 second
            memcpy((reoccurring_cmds+j)->gs_command,(repeated_cmds+j)->gs_command,sizeof((repeated_cmds+j)->gs_command));
            continue;
        }
        //If command repeats every minute at a specific second
        if ((reoccurring_cmds+j)->scheduled_time.tm_hour == asterisk && (reoccurring_cmds+j)->scheduled_time.tm_min == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buffer.tm_hour);
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buffer.tm_min);
            time_buffer.tm_sec = (reoccurring_cmds+j)->scheduled_time.tm_sec;
            //convert the first execution time into unix time. Add 60 seconds to allow processing time
            (repeated_cmds+j)->unix_time = mktime(&time_buffer) + 60;
            (repeated_cmds+j)->frequency = 60; //1 min
            memcpy((reoccurring_cmds+j)->gs_command,(repeated_cmds+j)->gs_command,sizeof((repeated_cmds+j)->gs_command));
            continue;
        }
        //If command repeats every hour
        if ((reoccurring_cmds+j)->scheduled_time.tm_hour == asterisk) {
            //TODO: consider edge cases where the hour increases as soon as this function is executed - complete
            RTCMK_ReadHours(RTCMK_ADDR, &time_buffer.tm_hour);
            time_buffer.tm_min = (reoccurring_cmds+j)->scheduled_time.tm_min;
            time_buffer.tm_sec = (reoccurring_cmds+j)->scheduled_time.tm_sec;
            //convert the first execution time into unix time. If the hour is almost over, increase the hour by one
            time_t current_time;
            if (mktime(&time_buffer) - RTCMK_GetUnix(&current_time) < 60) {
                (repeated_cmds+j)->unix_time = mktime(&time_buffer) + 3600;
            }
            else {
                (repeated_cmds+j)->unix_time = mktime(&time_buffer);
            }
            (repeated_cmds+j)->frequency = 3600; //1 hr
            memcpy((reoccurring_cmds+j)->gs_command,(repeated_cmds+j)->gs_command,sizeof((repeated_cmds+j)->gs_command));
            continue;
        }
        else {
            time_buffer.tm_hour = (reoccurring_cmds+j)->scheduled_time.tm_hour;
        }
        if((reoccurring_cmds+j)->scheduled_time.tm_min == asterisk) {
            //TODO: consider edge cases where the min increases as soon as this function is executed
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buffer.tm_min);
        }
        else {
            time_buffer.tm_min = (reoccurring_cmds+j)->scheduled_time.tm_min;
        }
        if((reoccurring_cmds+j)->scheduled_time.tm_sec == asterisk) {
            //TODO: consider edge cases where the sec increases as soon as this function is executed
            RTCMK_ReadMinutes(RTCMK_ADDR, &time_buffer.tm_sec);
        }
        else {
            time_buffer.tm_sec = (reoccurring_cmds+j)->scheduled_time.tm_sec;
        }

        //Convert the time into unix time for sorting convenience
        (repeated_cmds+j)->unix_time = mktime(&time_buffer);

    }
    
    //Store the sorted repetitive commands into the SD card
    //write_cmds_to_file(2, &repeated_cmds, j_rep, fileName2);
    free(reoccurring_cmds);
    free(repeated_cmds);
    
    //prv_give_lock(cmds);
    return SATR_OK;
}



//TODO: consider writing a function to retrieve the list of scheduled commands
static scheduled_commands_t *prv_get_cmds_scheduler() {
    //if (!prvEps.eps_lock) {
        //prvEps.eps_lock = xSemaphoreCreateMutex();
    //}
    //configASSERT(prvEps.eps_lock);
    return &prvGS_cmds;
}

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
 * @return Result
 *      FAILURE or SUCCESS
 */
Result write_cmds_to_file(uint16_t filenumber, scheduled_commands_unix_t *scheduled_cmds, int number_of_cmds, char fileName) {
    int32_t fout = red_open(fileName, RED_O_CREAT | RED_O_RDWR); // open or create file to write binary
    if (fout == -1) {
        printf("Unexpected error %d from red_open()\r\n", (int)red_errno);
        ex2_log("Failed to open or create file to write: '%s'\n", fileName);
        return FAILURE;
    }
    uint16_t needed_size = sizeof(number_of_cmds * sizeof(scheduled_commands_unix_t));

    red_errno = 0;
    /*The order of writes and subsequent reads must match*/
    red_write(fout, &scheduled_cmds, sizeof(needed_size));
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

Result execute_non_rep_gs_cmds(void *pvParameters) {
    // Call gs command scheduler and have it store scheduled commands to SD card, then execute them in the time sequence given
    TickType_t xLastWakeTime;
    int number_of_cmds = num_of_cmds.non_rep_cmds;
    //TODO: use calloc - complete
    //calloc initializes each block with a default value ‘0’
    scheduled_commands_unix_t* non_reoccurring_cmds = (scheduled_commands_unix_t*)calloc(number_of_cmds,sizeof(scheduled_commands_unix_t));

    //open file from SD card
    int32_t fout = red_open(fileName1, RED_O_RDONLY | RED_O_RDWR); // open or create file to write binary
    if (fout == -1) {
        printf("Unexpected error %d from red_open()\r\n", (int)red_errno);
        ex2_log("Failed to open or create file to write: '%s'\n", fileName1);
        return FAILURE;
    }
    //read file
    int32_t f_read = red_read(fileName1, non_reoccurring_cmds, sizeof(non_reoccurring_cmds));
    if (f_read == -1) {
        printf("Unexpected error %d from red_read()\r\n", (int)red_errno);
        ex2_log("Failed to read file: '%s'\n", fileName1);
        return FAILURE;
    }

    //initialize the first cmd to be executed
    xLastWakeTime = xTaskGetTickCount();
    int delay_time = (non_reoccurring_cmds+1)->unix_time - (non_reoccurring_cmds)->unix_time; //in seconds
    TickType_t delay_ticks = pdMS_TO_TICKS(1000 * delay_time); //in # of ticks
    //Run the scheduled cmds with CSP
    //TODO: figure out how to use this function
    int csp_sendto_reply(const csp_packet_t * request, csp_packet_t * reply, uint32_t opts, uint32_t timeout);

    for (int i = 1; i < number_of_cmds; i++) {
        // Wait for the next cycle.
        vTaskDelayUntil( &xLastWakeTime, delay_ticks );

        //Run the scheduled cmds with CSP
        int csp_sendto_reply(const csp_packet_t * request, csp_packet_t * reply, uint32_t opts, uint32_t timeout);

        //return the time the next cmd should be executed
        delay_time = (non_reoccurring_cmds+i+1)->unix_time - (non_reoccurring_cmds+i)->unix_time; //in seconds
        delay_ticks = pdMS_TO_TICKS(1000 * delay_time); //in # of ticks
    }

    //close file from SD card
    int32_t f_close = red_close(fileName1); 
    if (f_close == -1) {
        printf("Unexpected error %d from red_close()\r\n", (int)red_errno);
        ex2_log("Failed to close file: '%s'\n", fileName1);
        return FAILURE;
    }

    //free up the stack once the cmds have been executed
    free(non_reoccurring_cmds);
}

static uint32_t svc_wdt_counter = 0;
static uint32_t get_svc_wdt_counter() { return svc_wdt_counter; }
SAT_returnState start_gs_cmds_scheduler_service(void);

/**
 * @brief
 *      FreeRTOS gs scheduler server task
 * @details
 *      Accepts incoming gs scheduler packets and executes the application
 * @param void* param
 * @return None
 */
void gs_scheduler_service(void *param) {
    csp_socket_t *sock;
    sock = csp_socket(CSP_SO_RDPREQ);
    csp_bind(sock, TC_GS_SCHEDULER_SERVICE);
    csp_listen(sock, SERVICE_BACKLOG_LEN);   // TODO: constant TBD
    svc_wdt_counter++;

    for (;;) {
        csp_conn_t *conn;
        csp_packet_t *packet;
        if ((conn = csp_accept(sock, CSP_MAX_TIMEOUT)) == NULL) {
            /* timeout */
            continue;
        }
        svc_wdt_counter++;

        while ((packet = csp_read(conn, 50)) != NULL) {
            if (gs_cmds_scheduler_service_app(conn, packet) != SATR_OK) {
                ex2_log("Error responding to packet");
            }
        }
        csp_close(conn); // frees buffers used
    }
}


/*------------------------------------------------------------------------------------------*/
    csp_conn_t *conn;
    if (!csp_send(conn, packet, 50)) { // why are we all using magic number?
            ex2_log("Failed to send packet");
            csp_buffer_free(packet);
            return FAILURE;
        }

/*------------------------------------------------------------------------------------------*/
int csp_sendto_reply(const csp_packet_t * request_packet, csp_packet_t * reply_packet, uint32_t opts, uint32_t timeout) {
	if (request_packet == NULL)
		return CSP_ERR_INVAL;

	return csp_sendto(request_packet->id.pri, request_packet->id.src, request_packet->id.sport, request_packet->id.dport, opts, reply_packet, timeout);
}

/**
   Send a packet as a reply to a request (without a connection).
   Calls csp_sendto() with the source address and port from the request.
   @param[in] request incoming request
   @param[in] reply reply packet
   @param[in] opts connection options, see @ref CSP_CONNECTION_OPTIONS. -->refer to RDP, this is a bit/flag in the header, but for now this goes into the opt, obc won't respond unless this is present
   @param[in] timeout unused as of CSP version 1.6
   @return #CSP_ERR_NONE on success, otherwise an error code and the reply must be freed by calling csp_buffer_free().
*/
int csp_sendto_reply(const csp_packet_t * request, csp_packet_t * reply, uint32_t opts, uint32_t timeout);
