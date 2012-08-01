/**
 * @file
 * The Mechanic2 public interface
 */
#include "Mechanic2.h"

/**
 * @brief Common messaging interface
 *
 * @param type The type of the message
 * @param message The message to display
 */
void Message(int type, char *message, ...) {
  static char message2[2048];
  va_list args;

  va_start(args, message);
    vsprintf(message2, message, args);
    if (type == MESSAGE_INFO)    printf("-- %s", message2);
    if (type == MESSAGE_COMMENT) printf("#  %s", message2);
    if (type == MESSAGE_OUTPUT) printf("   %s", message2);
    if (type == MESSAGE_ERR) printf("!! %s", message2);
		if (type == MESSAGE_WARN) printf(".. %s", message2);
  va_end(args);
}

/**
 * @brief Prints the dataset
 *
 * @param type The output prefix type
 * @param dataset The HDF5 dataset handler
 *
 * @todo Do it better (i.e. for other datatypes)
 */
void PrintDataset(int type, hid_t dataset) {
  hid_t dataspace, datatype;
  hsize_t dimsf[2], maxdims[2];
  int dims[MAX_RANK];
  int i, j;
  double **buffer;

  dataspace = H5Dget_space(dataset);
  datatype = H5Dget_type(dataset);
  H5Sget_simple_extent_dims(dataspace, dimsf, maxdims);

  dims[0] = (int)dimsf[0];
  dims[1] = (int)dimsf[1];

  buffer = AllocateBuffer(2, dims);
  H5Dread(dataset, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, &buffer[0][0]);

  // @todo do it better
  for (i = 0; i < dims[0]; i++) {
    for (j = 0; j < dims[1]; j++) {
      Message(MESSAGE_OUTPUT, "%f ", buffer[i][j]);
    }
    Message(MESSAGE_OUTPUT, "\n");
  }

  FreeBuffer(buffer);

  H5Sclose(dataspace);
}

/**
 * @brief Get the 1D size of the array
 *
 * @param rank The rank of the array
 * @param dims The dimensions of the array
 *
 * @return The 1D size of the array
 */
int GetSize(int rank, int *dims){
  int i = 0, size = 0;

  size = dims[0];
  for (i = 1; i < rank; i++) {
    size = size * dims[i];
  }

  return size;
}
/**
 * @brief Allocate memory buffer
 *
 * see http://www.hdfgroup.org/ftp/HDF5/examples/misc-examples/h5_writedyn.c
 * see http://stackoverflow.com/questions/5104847/mpi-bcast-a-dynamic-2d-array
 *
 * @param rank The rank of the array to allocate
 * @param dims The dimensions of the array to allocate
 *
 * @return Allocated array, NULL otherwise
 * @todo A generic approach?
 */
double** AllocateBuffer(int rank, int *dims) {
  double** array = NULL;
  int i = 0, size = 0;

  size = GetSize(rank, dims);

  if (size > 0) {
    array = calloc(dims[0]*sizeof(double*), sizeof(double*));
    if (array) {
      array[0] = calloc(size*sizeof(double), sizeof(double));
      for (i = 0; i < dims[0]; i++) array[i] = array[0] + i*dims[1];
    }
  }

  return array;
}

/**
 * @brief Free memory buffer
 *
 * see http://www.hdfgroup.org/ftp/HDF5/examples/misc-examples/h5_writedyn.c
 *
 * @param array The array pointer to free
 */
void FreeBuffer(double **array) {
  if (array[0]) free(array[0]);
  if (array) free(array);
}

