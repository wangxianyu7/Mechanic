/**
 * @file
 * The checkpoint interface
 */
#include "MCheckpoint.h"

/**
 * @brief Load the checkpoint
 *
 * @param m The module pointer
 * @param p The current pool pointer
 * @param cid The current checkpoint id
 *
 * @return The checkpoint pointer, NULL otherwise
 */
checkpoint* CheckpointLoad(module *m, pool *p, int cid) {
  checkpoint *c = NULL;
  int i = 0;

  /* Allocate checkpoint pointer */
  c = calloc(sizeof(checkpoint), sizeof(checkpoint));
  if (!c) Error(CORE_ERR_MEM);

  c->storage = calloc(sizeof(storage), sizeof(storage));
  if (!c->storage) Error(CORE_ERR_MEM);

  c->storage->layout = (schema) STORAGE_END;
  c->storage->data = NULL;

  c->cid = cid;
  c->counter = 0;
  c->size = p->checkpoint_size / (m->mpi_size-1);
  c->size = c->size * (m->mpi_size-1);
  if (c->size == 0) c->size = m->mpi_size-1;

  /* The storage buffer */
  c->storage->layout.rank = 2;
  c->storage->layout.dim[0] = c->size;
  c->storage->layout.dim[1] = 3 + MAX_RANK; // offset: tag, tid, status, location

  for (i = 0; i < m->task_banks; i++) {
    c->storage->layout.dim[1] +=
      GetSize(p->task->storage[i].layout.rank, p->task->storage[i].layout.dim);
  }

  c->storage->data = AllocateBuffer(c->storage->layout.rank, c->storage->layout.dim);

  for (i = 0; i < c->size; i++) {
    c->storage->data[i][1] = TASK_EMPTY; // t->tid
    c->storage->data[i][2] = TASK_EMPTY; // t->status
  }

  return c;
}

/**
 * @brief Prepare the checkpoint
 *
 * @param m The module pointer
 * @param p The current pool pointer
 * @param c The current checkpoint pointer
 *
 * @return 0 on success, error code otherwise
 */
int CheckpointPrepare(module *m, pool *p, checkpoint *c) {
  int mstat = SUCCESS;
  query *q = NULL;
  setup *s = &(m->layer.setup);

  if (m->node == MASTER) {
    q = LoadSym(m, "CheckpointPrepare", LOAD_DEFAULT);
    if (q) mstat = q(p, c, s);
    CheckStatus(mstat);
  }

  return mstat;
}

/**
 * @brief Process the checkpoint
 *
 * @param m The module pointer
 * @param p The current pool pointer
 * @param c The current checkpoint pointer
 *
 * @return 0 on success, error code otherwise
 */
int CheckpointProcess(module *m, pool *p, checkpoint *c) {
  int mstat = SUCCESS;
  int i = 0, j = 0, k = 0, l = 0;
  hid_t h5location, group, tasks, datapath;
  hsize_t dims[MAX_RANK], offsets[MAX_RANK];
  char path[LRC_CONFIG_LEN];
  task *t;
  int size, position;

  Backup(m, &m->layer.setup);

  /* Commit data for the task board */
  h5location = H5Fopen(m->filename, H5F_ACC_RDWR, H5P_DEFAULT);
  H5CheckStatus(h5location);

  sprintf(path, POOL_PATH, p->pid);

  group = H5Gopen(h5location, path, H5P_DEFAULT);
  H5CheckStatus(group);

  mstat = CommitData(group, 1, p->board);
  CheckStatus(mstat);

  /* Update pool data */
  mstat = CommitData(group, m->pool_banks, p->storage);
  CheckStatus(mstat);

  tasks = H5Gopen(group, TASKS_GROUP, H5P_DEFAULT);
  H5CheckStatus(tasks);

  t = TaskLoad(m, p, 0);
  position = 5;
  for (j = 0; j < m->task_banks; j++) {
    if (p->task->storage[j].layout.storage_type == STORAGE_PM3D ||
        p->task->storage[j].layout.storage_type == STORAGE_LIST ||
        p->task->storage[j].layout.storage_type == STORAGE_BOARD) {

      dims[0] = p->board->layout.dim[0];
      dims[1] = p->board->layout.dim[1];

      for (i = 0; i < c->size; i++) {
        t->tid = c->storage->data[i][1];
        t->status = c->storage->data[i][2];
        t->location[0] = c->storage->data[i][3];
        t->location[1] = c->storage->data[i][4];

        if (t->tid != TASK_EMPTY && t->status != TASK_EMPTY) {

          offsets[0] = 0;
          offsets[1] = 0;

          if (t->storage[j].layout.storage_type == STORAGE_PM3D) {
            offsets[0] = (t->location[0] + dims[0]*t->location[1])
              * t->storage[j].layout.dim[0];
            offsets[1] = 0;
          }
          if (t->storage[j].layout.storage_type == STORAGE_LIST) {
            offsets[0] = t->tid * t->storage[j].layout.dim[0];
            offsets[1] = 0;
          }
          if (t->storage[j].layout.storage_type == STORAGE_BOARD) {
            offsets[0] = t->location[0] * t->storage[j].layout.dim[0];
            offsets[1] = t->location[1] * t->storage[j].layout.dim[1];
          }

          t->storage[j].layout.offset[0] = offsets[0];
          t->storage[j].layout.offset[1] = offsets[1];

          Vec2Array(&c->storage->data[i][position], t->storage[j].data,
              t->storage[j].layout.rank, t->storage[j].layout.dim);

          /* Commit data to the pool */
          for (k = (int)offsets[0]; k < (int)offsets[0] + t->storage[j].layout.dim[0]; k++) {
            for (l = (int)offsets[1]; l < (int)offsets[1] + t->storage[j].layout.dim[1]; l++) {
              p->task->storage[j].data[k][l] =
                t->storage[j].data[k-(int)offsets[0]][l-(int)offsets[1]];
            }
          }

          /* Commit data to master datafile */
          if (p->task->storage[j].layout.use_hdf) {
            mstat = CommitData(tasks, 1, &t->storage[j]);
            CheckStatus(mstat);
          }
        }
      }
    }

    if (p->task->storage[j].layout.storage_type == STORAGE_BASIC) {
      for (i = 0; i < c->size; i++) {
        t->tid = c->storage->data[i][1];
        t->status = c->storage->data[i][2];
        t->location[0] = c->storage->data[i][3];
        t->location[1] = c->storage->data[i][4];
        t->storage[j].layout.offset[0] = 0;
        t->storage[j].layout.offset[1] = 0;
        if (t->tid != TASK_EMPTY && t->status != TASK_EMPTY) {

          Vec2Array(&c->storage->data[i][position], t->storage[j].data,
              t->storage[j].layout.rank, t->storage[j].layout.dim);

          /* Commit data to the pool */
          p->tasks[t->tid]->tid = t->tid;
          p->tasks[t->tid]->status = t->status;
          p->tasks[t->tid]->location[0] = t->location[0];
          p->tasks[t->tid]->location[1] = t->location[1];

          for (k = 0; k < t->storage[j].layout.dim[0]; k++) {
            for (l = 0; l < t->storage[j].layout.dim[1]; l++) {
              p->tasks[t->tid]->storage[j].data[k][l] =
                t->storage[j].data[k][l];
            }
          }

          /* Commit data to master datafile */
          if (p->task->storage[j].layout.use_hdf) {
            sprintf(path, TASK_PATH, t->tid);
            datapath = H5Gopen(tasks, path, H5P_DEFAULT);
            H5CheckStatus(datapath);
            mstat = CommitData(datapath, 1, &t->storage[j]);
            CheckStatus(mstat);
            H5Gclose(datapath);
          }
        }
      }
    }
    size = GetSize(t->storage[j].layout.rank, t->storage[j].layout.dim);
    position = position + size;
  }

  TaskFinalize(m, p, t);

  H5Gclose(tasks);
  H5Gclose(group);
  H5Fclose(h5location);

  return mstat;
}

/**
 * @brief Reset the checkpoint pointer and update the checkpoint id
 *
 * @param m The module pointer
 * @param p The current pool pointer
 * @param c The current checkpoint pointer
 * @param cid The checkpoint id to use
 */
void CheckpointReset(module *m, pool *p, checkpoint *c, int cid) {
  int i = 0;
  c->cid = cid;
  c->counter = 0;

  for (i = 0; i < c->size; i++) {
    c->storage->data[i][1] = TASK_EMPTY;
    c->storage->data[i][2] = TASK_EMPTY;
  }
}

/**
 * @brief Finalize the checkpoint
 *
 * @param m The module pointer
 * @param p The current pool pointer
 * @param c The checkpoint pointer
 */
void CheckpointFinalize(module *m, pool *p, checkpoint *c) {
  if (c->storage->data) FreeBuffer(c->storage->data);
  if (c) free(c);
}

/**
 * @brief Create incremental backup
 *
 * @param m The module pointer
 * @param s The setup pointer
 *
 * @return 0 on success, error code otherwise
 */
int Backup(module *m, setup *s) {
  int i = 0, b = 0, mstat = SUCCESS;
  char *current_name, *backup_name, iter[4];
  struct stat current;
  struct stat backup;

  b = LRC_option2int("core", "checkpoint-files", s->head);

  for (i = b-2; i >= 0; i--) {
    snprintf(iter, 3, "%02d", i+1);
    backup_name = Name(LRC_getOptionValue("core", "name", s->head), "-master-", iter, ".h5");

    snprintf(iter, 3,"%02d", i);
    current_name = Name(LRC_getOptionValue("core", "name", s->head), "-master-", iter, ".h5");

    if (stat(current_name, &current) == 0) {
      if (stat(backup_name, &backup) < 0) {
        mstat = Copy(current_name, backup_name);
        CheckStatus(mstat);
      } else {
        if (i == 0) {
          mstat = Copy(current_name, backup_name);
          CheckStatus(mstat);
        } else {
          mstat = rename(current_name, backup_name);
          if (mstat < 0) Error(CORE_ERR_CHECKPOINT);
        }
      }
    }
    free(current_name);
    free(backup_name);
  }

  return mstat;
}
