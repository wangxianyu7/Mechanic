/* MPI FARM SKELETON
 * Task farm model
 *
 * by mariusz slonina <mariusz.slonina@gmail.com>
 * with a little help of kacper kowalik <xarthisius.kk@gmail.com>
 *
 * last updated: 26/12/2009
 *
 */
#include "mpifarm_skel.h"
#include <string.h>

static int* map2d(int);
static void master(void);
static void slave(void);
void clearArray(MY_DATATYPE*,int);

/* Simplified struct for writing config data to hdf file */
typedef struct {
      char space[MAX_NAME_LENGTH];
      char varname[MAX_NAME_LENGTH];
      char value[MAX_VALUE_LENGTH];
    } simpleopts;

char* sep = "="; char* comm = "#";
char* inifile;
char* datafile;

/* settings */ 
int allopts = 0; //number of options read

/* farm */ 
int xres = 0, yres = 0, method = 0, datasetx = 1, datasety = 3;

/* mpi */ 
MPI_Status mpi_status;
int mpi_rank, mpi_size, mpi_dest = 0, mpi_tag = 0;
int farm_res = 0;
int npxc = 0; //number of all sent pixels
int count = 0; //number of pixels to receive
int source_tag = 0, data_tag = 2, result_tag = 59, terminate_tag = 99; //data tags

int masteralone = 0;
int nodes = 0;

unsigned int membersize, maxsize;
int position, msgsize;
char *buffer;

int i = 0, j = 0, k = 0, opts = 0, n = 0;

/* hdf */
hid_t file_id, dset_board, dset_data, dset_config, data_group;
hid_t configspace, configmemspace, mapspace, memmapspace, rawspace, memrawspace, maprawspace; 
hid_t plist_id;
hsize_t dimsf[2], dimsr[2], co[2], rco[2], off[2], stride[2];
int fdata[1][1];
herr_t hdf_status;
hid_t hdf_config_datatype;

/**
 * MAIN
 *
 */
int main(int argc, char* argv[]){  

  inifile = "config";
  
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  /**
   * Each slave knows exactly what is all about
   * -- it is much easier to handle
   */
  if(mpi_rank == 0){
    allopts = userdefined_readConfigValues(inifile, sep, comm, &d); 
   
    if (d.xres == 0 || d.yres == 0){
       printf("X/Y map resolution should not set to 0!\n");
       printf("If You want to do only one simulation, please set xres = 1, yres = 1\n");
       MPI_Abort(MPI_COMM_WORLD, 911);
    }
    userdefined_mpiBcast(mpi_rank,&d);
  }else{
    userdefined_mpiBcast(mpi_rank,&d);
  }

  /**
   * Create data pack
   */
  MPI_Pack_size(3, MPI_INT, MPI_COMM_WORLD, &membersize);
  maxsize = membersize;
  MPI_Pack_size(MAX_RESULT_LENGTH, MY_MPI_DATATYPE, MPI_COMM_WORLD, &membersize);
  maxsize += membersize;
  buffer = malloc(maxsize);
      
  if (mpi_size == 1){
     printf("You should have at least one slave!\n");
     masteralone = 1;

     /**
      * In the future we can allow the master node to perform all tasks
      * but for now, we just abort.
      */
     MPI_Abort(MPI_COMM_WORLD, 911);
  }

  if(mpi_rank == 0) {
      master();
	} else {
      slave(); 
  }
  
	MPI_Finalize();
	
  return 0;
}

/**
 * MASTER
 *
 */
static void master(void){

   int *tab; 
   masterData rawdata;
	 MY_DATATYPE rdata[MAX_RESULT_LENGTH][1];
   int i = 0, k = 0, j = 0;
   
   clearArray(rawdata.res,ITEMS_IN_ARRAY(rawdata.res));
   
   printf("MAP RESOLUTION = %dx%d.\n", d.xres, d.yres);
   printf("COMM_SIZE = %d.\n", mpi_size);
   printf("METHOD = %d.\n", d.method);

   /**
    * Master can do something useful before computations
    */
   userdefined_masterIN(mpi_size, &d);

   /**
    * Align farm resolution for given method
    */
   if (method == 0) farm_res = d.xres*d.yres; //one pixel per each slave
   if (method == 1) farm_res = d.xres; //sliceX
   if (method == 2) farm_res = d.yres; //sliceY
   if (method == 6) farm_res = userdefined_farmResolution(d.xres, d.yres);

   hsize_t dim[1];
   unsigned rr = 1;
 
   /**
    * HDF5 Storage
    *
    * We write data in the following scheme:
    * /config -- configuration file
    * /board -- map of computed pixels
    * /data -- output data group
    * /data/master -- master dataset
    *
    * Each slave can write own data files if needed.
    * In such case, please edit slaveIN/OUT functions in mpifarm_user.c.
    *
    */

   /* Create master datafile */
    file_id = H5Fcreate(d.datafile, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    
    /* Number of options to write */
    j = 0;
    for(i = 0; i < allopts; i++){
      for(k = 0; k < configSpace[i].num; k++){
        j++;
      }
    }

    dim[0] = j;

    /* Define simplified struct */
    simpleopts optstohdf[j];
    
    /* Copy config file to our simplified struct */
    j = 0;
      for(i = 0; i < allopts; i++){
        for(k = 0; k < configSpace[i].num; k++){
          strcpy(optstohdf[j].space,configSpace[i].space);
          strcpy(optstohdf[j].varname,configSpace[i].options[k].name);
          strcpy(optstohdf[j].value,configSpace[i].options[k].value);
          j++;
        }
      }

    /* Config dataspace */
    configspace = H5Screate_simple(rr, dim, NULL);

    /* Create compound data type for handling config struct */
    hid_t hdf_optsarr_dt = H5Tcreate(H5T_COMPOUND, sizeof(simpleopts));
    
      hid_t space_dt = H5Tcopy(H5T_C_S1);
      H5Tset_size(space_dt, MAX_NAME_LENGTH);
    
      hid_t varname_dt = H5Tcopy(H5T_C_S1);
      H5Tset_size(varname_dt, MAX_NAME_LENGTH);
    
      hid_t value_dt = H5Tcopy(H5T_C_S1);
      H5Tset_size(value_dt, MAX_NAME_LENGTH);

    H5Tinsert(hdf_optsarr_dt, "Namespace", HOFFSET(simpleopts, space), space_dt);
    H5Tinsert(hdf_optsarr_dt, "Variable", HOFFSET(simpleopts, varname), varname_dt);
    H5Tinsert(hdf_optsarr_dt, "Value", HOFFSET(simpleopts, value), value_dt);

    dset_config = H5Dcreate(file_id, DATASETCONFIG, hdf_optsarr_dt, configspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    configmemspace = H5Screate_simple(rr, dim, NULL);

    hdf_status = H5Dwrite(dset_config, hdf_optsarr_dt, H5S_ALL, H5S_ALL, H5P_DEFAULT, optstohdf);

    H5Tclose(hdf_optsarr_dt);
    H5Dclose(dset_config);
    H5Sclose(configspace);
    H5Sclose(configmemspace);
    
    /* Control board space */
    dimsf[0] = d.xres;
    dimsf[1] = d.yres;
    mapspace = H5Screate_simple(HDF_RANK, dimsf, NULL);
    
    dset_board = H5Dcreate(file_id, DATABOARD, H5T_NATIVE_INT, mapspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

    /* Master data group */
    data_group = H5Gcreate(file_id, DATAGROUP, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    
    /* Result data space  */
    dimsr[0] = d.xres*d.yres;
    dimsr[1] = MAX_RESULT_LENGTH;
    rawspace = H5Screate_simple(HDF_RANK, dimsr, NULL);

    /* Create master dataset */
    dset_data = H5Dcreate(data_group, DATASETMASTER, H5T_NATIVE_DOUBLE, rawspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  
   /**
    * We write pixels one by one
    */
    co[0] = 1;
    co[1] = 1;
    memmapspace = H5Screate_simple(HDF_RANK, co, NULL);

    rco[0] = 1;
    rco[1] = MAX_RESULT_LENGTH;
    memrawspace = H5Screate_simple(HDF_RANK, rco, NULL);
   
   /**
    * Select hyperslab in the file
    */
    mapspace = H5Dget_space(dset_board);
    rawspace = H5Dget_space(dset_data);
  
   /**
    * FARM STARTS
    */

   /**
    * Security check -- if farm resolution is greater than number of slaves.
    * Needed when we have i.e. 3 slaves and only one pixel to compute.
    */
   if (farm_res > mpi_size){
     nodes = mpi_size;
   }else{
     nodes = farm_res + 1;
   }

   /**
    * Send tasks to all slaves
    */
   for (i = 1; i < nodes; i++){
      userdefined_master_beforeSend(i,&d,&rawdata);
      MPI_Send(map2d(npxc), 3, MPI_INT, i, data_tag, MPI_COMM_WORLD);
      userdefined_master_afterSend(i,&d,&rawdata);
      count++;
      npxc++;
   }

   /**
    * Receive data and send tasks
    */
   while (1) {
    
      userdefined_master_beforeReceive(&d,&rawdata);
      MPI_Recv(buffer, maxsize, MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
      count--;
      
      /**
       * Unpack data sent by the slave
       */
      position = 0;
      MPI_Get_count(&mpi_status, MPI_PACKED, &msgsize);
      MPI_Unpack(buffer, msgsize, &position, rawdata.coords, 3, MPI_INT, MPI_COMM_WORLD);
      MPI_Unpack(buffer, msgsize, &position, rawdata.res, MAX_RESULT_LENGTH, MY_MPI_DATATYPE, MPI_COMM_WORLD);
      userdefined_master_afterReceive(mpi_status.MPI_SOURCE, &d, &rawdata);

      /**
       * Write results to master file
       */
      
      /**
       * Control board -- each computed pixel is marked with 1
       */
      off[0] = rawdata.coords[0];
      off[1] = rawdata.coords[1];
      fdata[0][0] = 1;
      H5Sselect_hyperslab(mapspace, H5S_SELECT_SET, off, NULL, co, NULL);
      hdf_status = H5Dwrite(dset_board, H5T_NATIVE_INT, memmapspace, mapspace, H5P_DEFAULT, fdata);

      /**
       * Data
       */
      off[0] = rawdata.coords[2];
      off[1] = 0;
      for (j = 0; j < MAX_RESULT_LENGTH; j++){
        rdata[j][0] = rawdata.res[j];
      }
      
      H5Sselect_hyperslab(rawspace, H5S_SELECT_SET, off, NULL, rco, NULL);
      hdf_status = H5Dwrite(dset_data, H5T_NATIVE_DOUBLE, memrawspace, rawspace, H5P_DEFAULT, rdata);

      /**
       * Send next task to the slave
       */
      if (npxc < farm_res){
         userdefined_master_beforeSend(mpi_status.MPI_SOURCE, &d, &rawdata);
         MPI_Send(map2d(npxc), 3, MPI_INT, mpi_status.MPI_SOURCE, data_tag, MPI_COMM_WORLD);
         npxc++;
         count++;
         userdefined_master_afterSend(mpi_status.MPI_SOURCE, &d, &rawdata);
      } else {
        break;
      }
    }

    /** 
     * No more work to do, receive the outstanding results from the slaves 
     * We've got exactly 'count' messages to receive 
     */
    for (i = 0; i < count; i++){
        userdefined_master_beforeReceive(&d, &rawdata);
        MPI_Recv(buffer, maxsize, MPI_PACKED, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
        position = 0;
        MPI_Get_count(&mpi_status, MPI_PACKED, &msgsize);
        MPI_Unpack(buffer, msgsize, &position, rawdata.coords, 3, MPI_INT, MPI_COMM_WORLD);
        MPI_Unpack(buffer, msgsize, &position, rawdata.res, MAX_RESULT_LENGTH, MY_MPI_DATATYPE, MPI_COMM_WORLD);
        userdefined_master_afterReceive(mpi_status.MPI_SOURCE, &d, &rawdata);
        
        /**
         * Write outstanding results to file
         */
      
        /**
         * Control board
         */
        off[0] = rawdata.coords[0];
        off[1] = rawdata.coords[1];
        
        fdata[0][0] = 1;
        H5Sselect_hyperslab(mapspace, H5S_SELECT_SET, off, NULL, co, NULL);
        hdf_status = H5Dwrite(dset_board, H5T_NATIVE_INT, memmapspace, mapspace, H5P_DEFAULT, fdata);
        
        /**
         * Data
         */
        off[0] = rawdata.coords[2];
        off[1] = 0;
        
        for (j = 0; j < MAX_RESULT_LENGTH; j++){
          rdata[j][0] = rawdata.res[j];
        }
      
        H5Sselect_hyperslab(rawspace, H5S_SELECT_SET, off, NULL, rco, NULL);
        hdf_status = H5Dwrite(dset_data, H5T_NATIVE_DOUBLE, memrawspace, rawspace, H5P_DEFAULT, rdata);

    }

    /**
     * Now, terminate the slaves 
     */
    for (i = 1; i < mpi_size; i++){
        MPI_Send(map2d(npxc), 3, MPI_INT, i, terminate_tag, MPI_COMM_WORLD);
    }

    /**
     * FARM ENDS
     */
   
    /**
     * CLOSE HDF5 STORAGE
     */
    H5Dclose(dset_board);
    H5Dclose(dset_data);
    H5Sclose(mapspace);
    H5Sclose(memmapspace);
    H5Sclose(rawspace);
    H5Sclose(memrawspace);
    H5Gclose(data_group);
    H5Fclose(file_id);

    /**
     * Master can do something useful after the computations 
     */
    userdefined_masterOUT(mpi_size, &d, &rawdata);
    
    return;
}

/**
 * SLAVE
 *
 */
static void slave(void){

    int *tab=malloc(3*sizeof(*tab));
    int k = 0, i = 0, j = 0;
    
    masterData rawdata;
    slaveData s;
	  MY_DATATYPE rdata[MAX_RESULT_LENGTH][1];
    
    clearArray(rawdata.res,ITEMS_IN_ARRAY(rawdata.res));
   
    /**
     * Slave can do something useful before computations
     */
    userdefined_slaveIN(mpi_rank, &d, &rawdata, &s);

    userdefined_slave_beforeReceive(mpi_rank, &d, &rawdata, &s);
    MPI_Recv(tab, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
    userdefined_slave_afterReceive(mpi_rank, &d, &rawdata, &s);

    while(1){

     if(mpi_status.MPI_TAG == data_tag){ 
      
       /**
        * One pixel per each slave
        */
       if (method == 0){

          rawdata.coords[0] = tab[0];
          rawdata.coords[1] = tab[1];
          rawdata.coords[2] = tab[2];
          
          /**
           * DO SOMETHING HERE
           */
          userdefined_pixelCompute(mpi_rank, &d, &rawdata, &s);

          userdefined_slave_beforeSend(mpi_rank, &d, &rawdata, &s);
          position = 0;
          MPI_Pack(&rawdata.coords, 3, MPI_INT, buffer, maxsize, &position, MPI_COMM_WORLD);
          MPI_Pack(&rawdata.res, MAX_RESULT_LENGTH, MY_MPI_DATATYPE, buffer, maxsize, &position, MPI_COMM_WORLD);
          MPI_Send(buffer, position, MPI_PACKED, mpi_dest, result_tag, MPI_COMM_WORLD);
          userdefined_slave_afterSend(mpi_rank, &d, &rawdata, &s);
        
          userdefined_slave_beforeReceive(mpi_rank, &d, &rawdata, &s);
          MPI_Recv(tab, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
          userdefined_slave_afterReceive(mpi_rank, &d, &rawdata, &s);
        }

       if (method == 6){

          /**
           * Use userdefined pixelCoords method
           * and pixelCompute
           *
           */
          userdefined_pixelCoords(mpi_rank, tab, &d, &rawdata, &s);
          userdefined_pixelCompute(mpi_rank, &d, &rawdata, &s);
          
          userdefined_slave_beforeSend(mpi_rank, &d, &rawdata, &s);
          position = 0;
          MPI_Pack(&rawdata.coords, 3, MPI_INT, buffer, maxsize, &position, MPI_COMM_WORLD);
          MPI_Pack(&rawdata.res, MAX_RESULT_LENGTH, MY_MPI_DATATYPE, buffer, maxsize, &position, MPI_COMM_WORLD);
          MPI_Send(buffer, position, MPI_PACKED, mpi_dest, result_tag, MPI_COMM_WORLD);
          userdefined_slave_afterSend(mpi_rank, &d, &rawdata, &s);
          
          userdefined_slave_beforeReceive(mpi_rank, &d, &rawdata, &s);
          MPI_Recv(tab, 3, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &mpi_status);
          userdefined_slave_afterReceive(mpi_rank, &d, &rawdata, &s);
       }
    }
      
     if(mpi_status.MPI_TAG == terminate_tag) break;     
    }
    
    /**
     * Slave can do something useful after computations
     */
    userdefined_slaveOUT(mpi_rank, &d, &rawdata, &s);

    return;
}

/**
 * HELPER FUNCTION -- MAP 1D INDEX TO 2D ARRAY
 *
 */

int* map2d(int c){
   int *ind = malloc(3*sizeof(*ind));
   int x, y;
   x = d.xres;
   y = d.yres;
  
   ind[2] = c; //we need number of current pixel to store too

   /**
    * Method 0: one pixel per each slave
    */
   if(method == 0){
    if(c < y) ind[0] = c / y; ind[1] = c;
    if(c > y - 1) ind[0] = c / y; ind[1] = c % y;
   }

   /**
    * Method 6: user defined control
    */
   if(method == 6) userdefined_pixelCoordsMap(ind, c, x, y);

   return ind;
}

/**
 * Clears arrays
 */
void clearArray(MY_DATATYPE* array, int no_of_items_in_array){

	int i;
	for(i = 0;i < no_of_items_in_array; i++){
		array[i] = 0.0;
	}

	return;
}