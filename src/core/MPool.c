/**
 * @file
 * The Task Pool related functions.
 */
#include "MPool.h"

/**
 * @function
 * Loads the task pool
 */
pool* PoolLoad(module *m, int pid) {
  pool *p = NULL;
  
  /* Allocate pool pointer */
  p = (pool*) malloc(sizeof(pool));
  if (!p) Error(CORE_ERR_MEM);

  /* Allocate pool data banks */
  p->storage = (storage*) malloc(m->layer.init.banks_per_pool * sizeof(storage));
  if (!p->storage) Error(CORE_ERR_MEM);

  /* Allocate task pointer */
  p->task = (task*) malloc(sizeof(task));
  if (!p->task) Error(CORE_ERR_MEM);
  
  p->task->storage = (storage*) malloc(m->layer.init.banks_per_task * sizeof(storage));
  if (!p->task->storage) Error(CORE_ERR_MEM);

  /* Setup some sane defaults */
  p->pid = pid;
  sprintf(p->name, "pool-%04d", p->pid);
  
  /* Pool data group */
  if (m->node == MASTER) {
    p->location = H5Gcreate(m->location, p->name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  }

  return p;
}

/**
 * @function
 * Initializes the task pool.
 */
int PoolInit(module *m, pool *p) {
  int mstat = 0;

  return mstat;
}

/**
 * @function
 * Prepares the pool.
 */
int PoolPrepare(module *m, pool *p) {
  int mstat = 0, i = 0;
  query *q;
  setup s = m->layer.setup;
  int size;

  if (m->node == MASTER) {
    q = LoadSym(m, "PoolPrepare", LOAD_DEFAULT);
    if (q) mstat = q(p, s);
    mstat = WritePoolData(p);
  }

  for (i = 0; i < m->pool_banks; i++) {
    if (p->storage[i].layout.sync) {
      size = GetSize(p->storage[i].layout.rank, p->storage[i].layout.dim);
      MPI_Bcast(&(p->storage[i].data[0][0]), size, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
    }
  }

  return mstat;
}

/**
 * @function
 * Processes the pool.
 */
int PoolProcess(module *m, pool *p) {
  int pool_create = 0;
  setup s = m->layer.setup;
  query *q;

  if (m->node == MASTER) {
    q = LoadSym(m, "PoolProcess", LOAD_DEFAULT);
    if (q) pool_create = q(p, s);
  }

  MPI_Bcast(&pool_create, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

  return pool_create;
}

/**
 * @function
 * Finalizes the pool
 */
void PoolFinalize(module *m, pool *p) {
  if (p->storage) {
    if (p->storage->data) {
      FreeDoubleArray(p->storage->data, p->storage->layout.dim);
    }
    free(p->storage);
  }
  if (p->task) {
    if (p->task->storage) free(p->task->storage);
    free(p->task);
  }
  if (m->node == MASTER) H5Gclose(p->location);

  free(p);
}
