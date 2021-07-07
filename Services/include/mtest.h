/*
 * mtest.h
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */

#ifndef EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MTEST_H_
#define EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MTEST_H_

#ifdef __cplusplus
extern "C" {
#endif

struct mtest;
typedef struct mtest mtest_t;

mtest_t *mtest_create(int start);
void mtest_destroy(mtest_t *m);

void mtest_add(mtest_t *m, int val);
int mtest_val(mtest_t *m);

#ifdef __cplusplus
}
#endif


#endif /* EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MTEST_H_ */
