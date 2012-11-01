/**
 * @file
 * The Worker node
 */
#include "Taskfarm.h"

/**
 * @brief Performs worker node operations
 *
 * @param m The module pointer
 * @param p The current pool pointer
 *
 * @return 0 on success, error code otherwise
 */
int Worker(module *m, pool *p) {
  int mstat = SUCCESS;
  int node, tag, intag;
  int k = 0;

  MPI_Status recv_status;

  task *t = NULL;
  checkpoint *c = NULL;
  storage *send_buffer = NULL, *recv_buffer = NULL;
  char *memory = NULL;

  node = m->node;
  intag = node;

  /* Initialize the task and checkpoint */
  t = TaskLoad(m, p, 0);
  c = CheckpointLoad(m, p, 0);

  /* Data buffers */
  send_buffer = calloc(1, sizeof(storage));
  if (!send_buffer) Error(CORE_ERR_MEM);

  recv_buffer = calloc(1, sizeof(storage));
  if (!recv_buffer) Error(CORE_ERR_MEM);

  /* Initialize data buffers */
  send_buffer->layout.size = sizeof(int) * (HEADER_SIZE);
  for (k = 0; k < m->task_banks; k++) {
    send_buffer->layout.size +=
      GetSize(p->task->storage[k].layout.rank, p->task->storage[k].layout.dim)*p->task->storage[k].layout.datatype_size;
  }

  recv_buffer->layout.size = send_buffer->layout.size;
  
  memory = malloc(send_buffer->layout.size);
  if (!memory) Error(CORE_ERR_MEM);

  memory = malloc(recv_buffer->layout.size);
  if (!memory) Error(CORE_ERR_MEM);

  while (1) {

    MPI_Recv(&(memory[0]), (int)recv_buffer->layout.size, MPI_CHAR,
        MASTER, intag, MPI_COMM_WORLD, &recv_status);

    mstat = Unpack(m, &memory, p, t, &tag);
    CheckStatus(mstat);
    printf("worker received task %d %d %d\n",
        t->tid, t->location[0], t->location[1]);
    
    if (tag == TAG_TERMINATE) {
      break;
    } else {

      mstat = TaskPrepare(m, p, t);
      CheckStatus(mstat);

      mstat = TaskProcess(m, p, t);
      CheckStatus(mstat);

      t->status = TASK_FINISHED;
      tag = TAG_RESULT;

      mstat = Pack(m, &memory, p, t, tag);
      CheckStatus(mstat);

      MPI_Send(&(memory), (int)send_buffer->layout.size, MPI_CHAR,
          MASTER, intag, MPI_COMM_WORLD);
    }

  }

//  MPI_Barrier(MPI_COMM_WORLD);

  /* Finalize */
  CheckpointFinalize(m, p, c);
  TaskFinalize(m, p, t);

  if (send_buffer) {
   // if (send_buffer->memory) free(send_buffer->memory); //Free(send_buffer);
//    free(send_buffer->memory);
    free(send_buffer);
  }

  if (recv_buffer) {
    //if (recv_buffer->memory) Free(recv_buffer);
    //free(&(recv_buffer->memory[0]));
    free(recv_buffer);
  }

  return mstat;
}

