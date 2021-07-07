/*
 * mac_service.h
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */

#ifndef EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MAC_SERVICE_H_
#define EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MAC_SERVICE_H_

#include <FreeRTOS.h>
#include <stdio.h>

#include "services.h"

SAT_returnState mac_service_app();

SAT_returnState start_mac_service(void);

#endif /* EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MAC_SERVICE_H_ */
