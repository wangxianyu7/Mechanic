#ifndef MECHANIC_POOL_H
#define MECHANIC_POOL_H

#include "MMechanic2.h"
#include "MTypes.h"
#include "MCommon.h"
#include "MLog.h"
#include "MModules.h"
#include "MStorage.h"

pool PoolLoad(module *m, int pid);
int PoolInit(module *m, pool *p);
int PoolPrepare(module *m, pool *p);
int PoolPostprocess(module *m, pool *p);
void PoolFinalize(module *m, pool *p);

#endif
