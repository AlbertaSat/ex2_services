/*
 * MacTest.h
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */

#ifndef EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MACTEST_H_
#define EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MACTEST_H_

class MacTest {
public:
  MacTest(int start);
  void add(int val);
  int val();

private:
  int value;
};

#endif /* EX2_SERVICES_SERVICES_INCLUDE_COMMUNICATION_MACTEST_H_ */
