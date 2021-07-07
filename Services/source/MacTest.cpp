/*
 * MacTest.cpp
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */

#include <MacTest.h>

MacTest::MacTest(int start)
{
  value = start;
}

void MacTest::add(int val)
{
  value += val;
}

int MacTest::val()
{
  return value;
}
