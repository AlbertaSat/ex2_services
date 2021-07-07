/*
 * mtest.cpp
 *
 *  Created on: Jul. 6, 2021
 *      Author: knud
 */


#include <stdlib.h>
#include "mtest.h"
#include "MacTest.h"

struct mtest {
  void *obj;
};

mtest_t *mtest_create(int start)
{
  mtest_t *m;
  MacTest *obj;

  m      = (typeof(m))malloc(sizeof(*m));
  obj    = new MacTest(start);
  m->obj = obj;

  return m;
}

void mtest_destroy(mtest_t *m)
{
  if (m == NULL)
    return;
  delete static_cast<MacTest *>(m->obj);
  free(m);
}

void mtest_add(mtest_t *m, int val)
{
  MacTest *obj;

  if (m == NULL)
    return;

  obj = static_cast<MacTest *>(m->obj);
  obj->add(val);
}

int mtest_val(mtest_t *m)
{
  MacTest *obj;

  if (m == NULL)
    return 0;

  obj = static_cast<MacTest *>(m->obj);
  return obj->val();
}

