/*
 * mac_service.c
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */

#include "communication/mac_service.h"
#include "communication/mtest.h"

SAT_returnState mac_service_app();

/**
 * @brief
 *      FreeRTOS mac server task
 * @details
 *      Accepts incoming communication service packets and executes the
 * application
 * @param void* param
 * @return None
 */
void mac_service(void *param) {
  mtest_t *m = param;
  mtest_add(m, 12);
  printf("%dn", mtest_val(m));
  for (;;) {
    if (mac_service_app()) {
    }
  }
}

/**
 * @brief
 *      Start the mac server task
 * @details
 *      Starts the FreeRTOS task responsible for accepting incoming
 *      communication service requests
 * @param None
 * @return SAT_returnState
 *      success report
 */
SAT_returnState start_mac_service(void) {

  mtest_t *m = mtest_create(4);
  mtest_add(m, 6);
  printf("%dn", mtest_val(m));
  //    mtest_destroy(m);

  if (xTaskCreate((TaskFunction_t)mac_service,
      "mac_service", 1024, m, NORMAL_SERVICE_PRIO,
      NULL) != pdPASS) {
    ex2_log("FAILED TO CREATE TASK start_mac_service\n");
    return SATR_ERROR;
  }
  ex2_log("Mac service handler started\n");
  return SATR_OK;
}


SAT_returnState mac_service_app() {
  return 0;
}
