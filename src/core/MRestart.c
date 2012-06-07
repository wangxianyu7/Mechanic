/**
 * The restart mode
 */
#include "MRestart.h"

/**
 * @function
 * Performs restart-related tasks
 */
int Restart(module *m, pool **pools, int *pool_counter) {
  int mstat = 0;
  int i, j, size;
  hid_t h5location, group, tasks, attr_id;
  char path[LRC_CONFIG_LEN];

  if (m->node == MASTER) {
    h5location = H5Fopen(m->filename, H5F_ACC_RDONLY, H5P_DEFAULT);

    /* (A) Get the last pool ID and set the pool counter */
    group = H5Gopen(h5location, LAST_GROUP, H5P_DEFAULT);
    attr_id = H5Aopen_name(group, "Id");
    H5Aread(attr_id, H5T_NATIVE_INT, pool_counter);
    H5Aclose(attr_id);
    H5Gclose(group);

    Message(MESSAGE_INFO, "Last pool ID: %d\n", *pool_counter);
  }

  /* (B) Recreate the memory layout */
  MPI_Bcast(pool_counter, 1, MPI_INT, MASTER, MPI_COMM_WORLD);

  for (i = 0; i <= *pool_counter; i++) {
    Storage(m, pools[i]);
  }

  /* (C) Read data for all pools */
  if (m->node == MASTER) {
    for (i = 0; i <= *pool_counter; i++) {
      sprintf(path, POOL_PATH, pools[i]->pid);
      group = H5Gopen(h5location, path, H5P_DEFAULT);

      /* Read pool board */
      ReadData(group, 1, pools[i]->board);

      /* Read pool storage banks */
      for (j = 0; j < m->pool_banks; j++) {
        size = GetSize(pools[i]->storage[j].layout.rank, pools[i]->storage[j].layout.dim);
        if (size > 0 && pools[i]->storage[j].layout.use_hdf) {
          ReadData(group, 1, &(pools[i]->storage[j]));
        }
      }

      /* Read task data */
      // @todo implement STORAGE_BASIC for task groups
      tasks = H5Gopen(group, "Tasks", H5P_DEFAULT);
      for (j = 0; j < m->task_banks; j++) {
        size = GetSize(pools[i]->task->storage[j].layout.rank, pools[i]->task->storage[j].layout.dim);
        if (size > 0 && pools[i]->task->storage[j].layout.use_hdf) {
          if (pools[i]->task->storage[j].layout.storage_type == STORAGE_PM3D
            || pools[i]->task->storage[j].layout.storage_type == STORAGE_LIST
            || pools[i]->task->storage[j].layout.storage_type == STORAGE_BOARD) {
            ReadData(tasks, 1, &(pools[i]->task->storage[j]));
          }
        }
      }
      H5Gclose(tasks);
      H5Gclose(group);
    }
    H5Fclose(h5location);
  }

  /* Broadcast pool data */
  for (i = 0; i <= *pool_counter; i++) {
    for (j = 0; j < m->pool_banks; j++) {
      if (pools[i]->storage[j].layout.sync) {
        size = GetSize(pools[i]->storage[j].layout.rank, pools[i]->storage[j].layout.dim);
        if (size > 0) {
          MPI_Bcast(&(pools[i]->storage[j].data[0][0]), size, MPI_DOUBLE, MASTER, MPI_COMM_WORLD);
        }
      }
    }
  }

  return mstat;
}
