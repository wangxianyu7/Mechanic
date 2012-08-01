#ifndef MECHANIC_CHECKPOINT_H
#define MECHANIC_CHECKPOINT_H

#include "Mechanic2.h"
#include "MCommon.h"
#include "MModules.h"
#include "MLog.h"
#include "MTask.h"

checkpoint* CheckpointLoad(module *m, pool *p, int cid);
int CheckpointPrepare(module *m, pool *p, checkpoint *c);
int CheckpointProcess(module *m, pool *p, checkpoint *c);
void CheckpointReset(module *m, pool *p, checkpoint *c, int cid);
void CheckpointFinalize(module *m, pool *p, checkpoint *c);
int Backup(module *m, setup *s);

#endif
