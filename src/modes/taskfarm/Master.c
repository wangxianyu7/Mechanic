/**
 * @file
 * The master node (MPI Blocking communication)
 */
#include "Taskfarm.h"

/**
 * Implements Init()
 */
int Init(init *i) {
  i->min_cpu_required = 2;
  return SUCCESS;
}

/**
 * @brief Performs master node operations
 *
 * @param m The module pointer
 * @param p The current pool pointer
 *
 * @return 0 on success, error code otherwise
 */
int Master(module *m, pool *p) {
  int mstat = SUCCESS, ice = 0;
  int i = 0, k = 0, cid = 0, terminated_nodes = 0;
  int tag = TAG_TERMINATE;
  int header[HEADER_SIZE] = HEADER_INIT;
  unsigned int c_offset = 0;
  short ****board_buffer = NULL;
  int send_node;
  size_t header_size;
  clock_t loop_in, loop_out;
  double cpu_time;

  MPI_Status mpi_status;
  
  storage *send_buffer = NULL, *recv_buffer = NULL, *temp_buffer = NULL;

  task *t = NULL;
  task *tc = NULL;
  checkpoint *c = NULL;

  // Initialize the temporary task board buffer
  board_buffer = AllocateShort4(p->board);
  ReadData(p->board, &board_buffer[0][0][0][0]);

  if (m->verbose) Message(MESSAGE_INFO, "Completed %04d of %04d tasks\n", p->completed, p->pool_size);

  // Data buffers
  send_buffer = calloc(1, sizeof(storage));
  if (!send_buffer) Error(CORE_ERR_MEM);

  recv_buffer = calloc(1, sizeof(storage));
  if (!recv_buffer) Error(CORE_ERR_MEM);

  temp_buffer = calloc(1, sizeof(storage));
  if (!temp_buffer) Error(CORE_ERR_MEM);

  // Initialize the task and checkpoint
  t = M2TaskLoad(m, p, 0);
  tc = M2TaskLoad(m, p, 0);
  c = CheckpointLoad(m, p, 0);

  header_size = sizeof(int) * (HEADER_SIZE);

  // Initialize data buffers
  send_buffer->layout.size = header_size;
  for (k = 0; k < p->task_banks; k++) {
    send_buffer->layout.size +=
      GetSize(p->task->storage[k].layout.rank, p->task->storage[k].layout.dims) * p->task->storage[k].layout.datatype_size;
  }

  recv_buffer->layout.size = send_buffer->layout.size;
  temp_buffer->layout.size = send_buffer->layout.size;
  
  send_buffer->memory = calloc(send_buffer->layout.size, sizeof(unsigned char));
  if (!send_buffer->memory) Error(CORE_ERR_MEM);

  recv_buffer->memory = calloc(recv_buffer->layout.size, sizeof(unsigned char));
  if (!recv_buffer->memory) Error(CORE_ERR_MEM);

  temp_buffer->memory = calloc(temp_buffer->layout.size, sizeof(unsigned char));
  if (!temp_buffer->memory) Error(CORE_ERR_MEM);

  // Specific for the restart mode. The restart file is already full of completed tasks
  if (p->completed == p->pool_size) goto finalize;

  // Start the clock
  loop_in = clock();

  // Send initial tasks to all workers
  for (i = 1; i < m->mpi_size; i++) {
    mstat = GetNewTask(m, p, t, board_buffer);
    CheckStatus(mstat);
    t->node = i;

    if (mstat != NO_MORE_TASKS) {
      mstat = Pack(m, send_buffer->memory, p, t, TAG_DATA);
      CheckStatus(mstat);
      board_buffer[t->location[0]][t->location[1]][t->location[2]][0] = TASK_IN_USE;
      if (m->stats) board_buffer[t->location[0]][t->location[1]][t->location[2]][1] = t->node;
      board_buffer[t->location[0]][t->location[1]][t->location[2]][2] = t->cid;
    } else {
      tag = TAG_TERMINATE;
      mstat = CopyData(&tag, send_buffer->memory, sizeof(int));
      CheckStatus(mstat);
      terminated_nodes++;
    }

    MPI_Send(&(send_buffer->memory[0]), send_buffer->layout.size, MPI_CHAR,
        i, TAG_DATA, MPI_COMM_WORLD);
        
    mstat = M2Send(MASTER, i, TAG_DATA, m, p);
    CheckStatus(mstat);
  }

  // The task farm loop (Blocking communication)
  while (1) {

    // Check for ICE file
    ice = Ice();
    if (ice == CORE_ICE) {
      Message(MESSAGE_WARN, "The ICE file has been detected. Flushing checkpoints\n");
    }

    // Flush checkpoint buffer and write data, reset counter
    if ((c->counter > (c->size-1)) || ice == CORE_ICE) {

      WriteData(p->board, &board_buffer[0][0][0][0]);
      mstat = M2CheckpointPrepare(m, p, c);
      CheckStatus(mstat);

      mstat = CheckpointProcess(m, p, c);
      CheckStatus(mstat);

      cid++;

      // Reset the checkpoint
      CheckpointReset(m, p, c, cid);
    }

    // Do simple Abort on ICE
    if (ice == CORE_ICE) Abort(CORE_ICE);

    // Wait for any operation to complete
    MPI_Recv(&(recv_buffer->memory[0]), recv_buffer->layout.size, MPI_CHAR,
      MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);

    send_node = mpi_status.MPI_SOURCE;

    // Get the data header
    mstat = CopyData(recv_buffer->memory, header, header_size);
    CheckStatus(mstat);

    if (header[0] == TAG_RESULT) p->completed++;

    mstat = M2Receive(MASTER, send_node, header[0], m, p, recv_buffer->memory);
    CheckStatus(mstat);

    // Copy data to the checkpoint buffer
    if (header[0] == TAG_RESULT || header[0] == TAG_CHECKPOINT) {
      c_offset = c->counter * recv_buffer->layout.size;
      mstat = CopyData(recv_buffer->memory, c->storage->memory + c_offset, recv_buffer->layout.size);
      
      c->counter++;
    }

    board_buffer[header[3]][header[4]][header[5]][0] = header[2];
    if (m->stats) board_buffer[header[3]][header[4]][header[5]][1] = send_node;
    board_buffer[header[3]][header[4]][header[5]][2] = header[6];

    if (header[0] == TAG_RESULT) {
      mstat = GetNewTask(m, p, t, board_buffer);
      CheckStatus(mstat);

      if (mstat != NO_MORE_TASKS) {

        mstat = Pack(m, send_buffer->memory, p, t, TAG_DATA);
        CheckStatus(mstat);

        board_buffer[t->location[0]][t->location[1]][t->location[2]][0] = TASK_IN_USE;
        if (m->stats) board_buffer[t->location[0]][t->location[1]][t->location[2]][1] = send_node;
        board_buffer[t->location[0]][t->location[1]][t->location[2]][2] = t->cid;

        MPI_Send(&(send_buffer->memory[0]), send_buffer->layout.size, MPI_CHAR,
            send_node, TAG_DATA, MPI_COMM_WORLD);

        mstat = M2Send(MASTER, send_node, TAG_DATA, m, p);
        CheckStatus(mstat);

      } else {
        Message(MESSAGE_DEBUG, "Master: no more tasks after %d of %d completed\n", p->completed, p->pool_size);
      }
    } else if (header[0] == TAG_CHECKPOINT) {
      tc->tid = header[1];
      tc->status = header[2];
      tc->location[0] = header[3];
      tc->location[1] = header[4];
      tc->location[2] = header[5];
      tc->cid = header[6];
      tc->node = send_node;
      
      mstat = CopyData(header, temp_buffer->memory, header_size);
      CheckStatus(mstat);

      mstat = CopyData(recv_buffer->memory + header_size, temp_buffer->memory + header_size, temp_buffer->layout.size - header_size);
      CheckStatus(mstat);
        
      MPI_Send(&(temp_buffer->memory[0]), temp_buffer->layout.size, MPI_CHAR,
          send_node, TAG_DATA, MPI_COMM_WORLD);

      mstat = M2Send(MASTER, send_node, TAG_DATA, m, p);
      CheckStatus(mstat);
    } else {
      // This should not happen
      Message(MESSAGE_ERR, "Unknown receive tag: %d\n");
      Abort(CORE_ERR_MPI);
    }

    if (p->completed == p->pool_size) break;
  }

  loop_out = clock();
  cpu_time = (double)(loop_out - loop_in)/CLOCKS_PER_SEC;
  if (m->showtime) Message(MESSAGE_INFO, "Computation loop completed. CPU time: %f\n", cpu_time);

  Message(MESSAGE_DEBUG, "Completed %d tasks\n", p->completed);

  WriteData(p->board, &board_buffer[0][0][0][0]);
  mstat = M2CheckpointPrepare(m, p, c);
  CheckStatus(mstat);

  mstat = CheckpointProcess(m, p, c);
  CheckStatus(mstat);

finalize:

  // Terminate all workers
  for (i = 1; i < m->mpi_size - terminated_nodes; i++) {
    tag = TAG_TERMINATE;
    mstat = CopyData(&tag, send_buffer->memory, sizeof(int));
    CheckStatus(mstat);

    MPI_Send(&(send_buffer->memory[0]), send_buffer->layout.size, MPI_CHAR,
        i, TAG_DATA, MPI_COMM_WORLD);
        
    mstat = M2Send(MASTER, i, tag, m, p);
    CheckStatus(mstat);
  }

  CheckpointFinalize(m, p, c);
  TaskFinalize(m, p, t);
  TaskFinalize(m, p, tc);

  if (send_buffer) {
    free(send_buffer->memory);
    free(send_buffer);
  }

  if (recv_buffer) {
    free(recv_buffer->memory);
    free(recv_buffer);
  }

  if (temp_buffer) {
    free(temp_buffer->memory);
    free(temp_buffer);
  }

  if (board_buffer) {
    free(board_buffer);
  }

  return mstat;
}

