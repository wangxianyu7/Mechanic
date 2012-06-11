/**
 * Change the storage layout for a specific task pool
 * ==================================================
 *
 * In this example we create new task pools during the simulation, and we change the
 * storage layout for one of them.
 *
 * Compilation
 * -----------
 *
 *     mpicc -fPIC -Dpic -shared mechanic_module_chpoollayout.c -o
 *     libmechanic_module_chpoollayout.so
 *
 * Using the module
 * ----------------
 *
 *    mpirun -np 4 mechanic2 -p chpoollayout -x 10 -y 20
 *
 * Listing the contents of the data file
 * -------------------------------------
 *
 *    h5ls -r mechanic-master-00.h5
 *
 * Getting the data
 * ----------------
 *
 *    h5dump -d/Pools/pool-0000/Tasks/result mechanic-master-00.h5
 *    h5dump -d/Pools/pool-0001/Tasks/result mechanic-master-00.h5
 *    h5dump -d/Pools/pool-0002/Tasks/result mechanic-master-00.h5
 *    h5dump -d/Pools/pool-0003/Tasks/result mechanic-master-00.h5
 */
#include "MMechanic2.h"

/**
 * Implements Storage()
 *
 * Each worker will return 1x3 result array. The master node will combine the worker
 * result arrays into one dataset suitable to process with Gnuplot PM3D. The final dataset
 * will be available at /Pools/pool-ID/Tasks/result.
 */
int Storage(pool *p, setup *s) {
  p->task->storage[0].layout = (schema) {
    .path = "result",
    .rank = 2,
    .dim[0] = 1,
    .dim[1] = 3,
    .use_hdf = 1,
    .storage_type = STORAGE_PM3D,
  };

  // Change the layout at the pool-0004
  if (p->pid == 4) {
    p->task->storage[0].layout.dim[1] = 5;
  }

  return SUCCESS;
}

/**
 * Implements TaskProcess()
 */
int TaskProcess(pool *p, task *t, setup *s) {

  // The horizontal position of the pixel
  t->storage[0].data[0][0] = t->location[0];

  // The vertical position of the pixel

  t->storage[0].data[0][1] = t->location[1];

  // The state of the system
  t->storage[0].data[0][2] = t->tid;

  // We are at pool-0004
  if (p->pid == 4) {
    t->storage[0].data[0][3] = t->tid + 3.0;
    t->storage[0].data[0][4] = t->tid + 4.0;
  }

  return SUCCESS;
}

/**
 * Implements PoolProcess()
 */
int PoolProcess(pool **allpools, pool *current, setup *s) {
  if (current->pid < 5) return POOL_CREATE_NEW;
  return POOL_FINALIZE;
}

