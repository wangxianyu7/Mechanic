/**
 * Creation of a new task pool
 * ===========================
 *
 * In this example we create new task pools during the simulation
 *
 * Compilation
 * -----------
 *
 *    mpicc -std=c99 -fPIC -Dpic -shared -lmechanic -lhdf5 -lhdf5_hl \
 *        mechanic_module_ex_createpool.c -o libmechanic_module_ex_createpool.so
 *
 * Using the module
 * ----------------
 *
 *    mpirun -np 4 mechanic -p ex_createpool -x 10 -y 20
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
#include "mechanic.h"

/**
 * Implements Init()
 */
int Init(init *i) {
  i->pools = 10;
  return SUCCESS;
}

/**
 * Implements Storage()
 *
 * Each worker will return 1x3 result array. The master node will combine the worker
 * result arrays into one dataset suitable to process with Gnuplot PM3D. The final dataset
 * will be available at /Pools/pool-ID/Tasks/result.
 */
int Storage(pool *p) {
  p->task->storage[0].layout = (schema) {
    .name = "result",
    .rank = 2,
    .dims[0] = 1,
    .dims[1] = 3,
    .sync = 1,
    .use_hdf = 1,
    .storage_type = STORAGE_PM3D,
    .datatype = H5T_NATIVE_DOUBLE
  };

  return SUCCESS;
}

/**
 * Implements TaskProcess()
 */
int TaskProcess(pool *p, task *t) {
  double buffer_one[1][3];

  // The vertical position of the pixel
  buffer_one[0][0] = t->location[0];

  // The horizontal position of the pixel
  buffer_one[0][1] = t->location[1];

  // The state of the system
  buffer_one[0][2] = t->tid;

  MWriteData(t, "result", &buffer_one[0][0]);
  
  return TASK_FINALIZE;
}

/**
 * Implements PoolProcess()
 */
int PoolProcess(pool **allpools, pool *current) {
  if (current->pid < 5) return POOL_CREATE_NEW;
  return POOL_FINALIZE;
}

