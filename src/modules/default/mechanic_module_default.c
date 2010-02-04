/**
 * MECHANIC MODULE DEFAULT
 *
 * Functions provided here are called during farm operations. 
 * We provide You with simple examples of using them -- 
 * the only limit is You imagination.
 *
 * Master Data struct is defined as follows:
 * typedef struct{
 *   int count[3]; <-- handles x,y coords and number of the pixel
 *   MY_DATATYPE res[1]; <-- handles result vector, resizable with mrl variable
 * }
 *
 * @section Case studies
 * @subsection Each slave does the same
 * This is the simplest case of using Mechanic. The only thing to do is to define 
 * pixelCompute function and return some data to master node with masterData struct.
 * You can also do something in functions IN/OUT, but in that case it is not really necessary.
 *
 * @subsection Each slave has different config file
 * This time You need to read config file for each slave separately. This can be done with 
 * LibReadConfig in slaveIN function and config files named after slave number, i.e. slave22.
 *
 * @subsection Each slave has different pixelCompute function.
 * At this point You need to create some subfunctions of pixelCompute and choose them
 * accordingly to number of the slave, i.e. in the switch routine.
 *
 * @subsection Each slave has both different config file and different pixelCompute
 * Just combining two cases in simple switch routines and it should work too.
 *
 */

#include "mechanic.h"
#include "mechanic_module_default.h"

/*
int mechanic_module_init(struct yourdata *pointer){
  //printf("Module DEFAULT INIT\n");
  //pointer->aa = 1.1;
  //pointer->bb = 2.2;
  return 0;
}

int mechanic_module_query(struct yourdata *pointer){
  //printf("Module DEFAULT QUERY\n");
  return 0;
}

int mechanic_module_cleanup(struct yourdata *pointer){
  //printf("Module DEFAULT CLEANUP: %f\n", pointer->aa + pointer->bb);

  free(pointer);
  return 0;
}
*/
int default_init(moduleInfo *md){

  md->name = "default";
  md->author = "MSlonina";
  md->date = "2010";
  md->version = "1.0";

//  sd.test = 21;
  return 0;
}
int default_query(){
//  printf("TEST = %d\n", s->test);
  return 0;
}
int default_cleanup(){
//  printf("SD.TEST = %d\n", sd.test);
  return 0;
}

/**
 * @fn int default_farmResolution(int x, int y)
 * @brief Defines the resolution of the farm.
 *
 * @param x
 * @param y
 *
 * @return 
 * Farm resolution
 *
 */
int default_farmResolution(int x, int y){
  
  int farm;

  farm = x*y;

  return farm;
}

/**
 * @fn int default_pixelCoordsMap(int t[], int p, int x, int y)
 * @brief Defines pixel mapping in the farm.
 *
 * You can overwrite default pixel coords alignment here.
 * Used only when method = 6.
 *
 * @param t
 *   Coords array, sent to each slave.
 * @param p
 * @param x
 * @param y
 *
 */
int default_pixelCoordsMap(int t[], int p, int x, int y){
  
    if(p < y) t[0] = p / y; t[1] = p;
    if(p > y - 1) t[0] = p / y; t[1] = p % y;
  return 0;
}

/**
 * @fn int default_pixelCoords(int slave, int t[], configData* d, masterData* r)
 * @brief Pixel coords mapping.
 *
 * Each slave takes the pixel coordinates and then do its work.
 * Here You can change pixel assignment to output masterData r.
 *
 * Used only when method = 6.
 *
 * @param slave
 * @param t
 * @param d
 * @param r
 *
 */
int default_pixelCoords(int slave, int t[], configData* d, masterData* r){
          
  r->coords[0] = t[0]; //x 
  r->coords[1] = t[1]; //y
  r->coords[2] = t[2]; //number of the pixel
  
  return 0;
}

/**
 * @fn int default_pixelCompute(int slave, configData* d, masterData* r)
 * @brief Pixel compute routine.
 * 
 * The heart. Here You can compute your pixels. Possible extenstions:
 * - Each slave has its own pixelCompute routine. You can use them accordingly to
 *   slave number in the switch loop.
 * - Each slave has the same pixelCompute routine. 
 *
 * Example:
 * We assign some values to the result array of masterData r and 
 * do some weird computations.
 *
 * @param slave
 * @param d
 * @param r
 *
 *
 */
int default_pixelCompute(int slave, configData* d, masterData* r){

  int i = 0;

   //  printf("D->MRL = %d\n",d->mrl);

   for(i = 0; i < d->mrl; i++){
      r->res[i] = pow(sin(i), 2.0) + pow(cos(i), 2.0) + pow(r->coords[0], 8.0) - pow(r->coords[1], 7.0);
      r->res[i] = (double)r->coords[2]*(double)i;
   }
  
   return 0;
}

/**
 * @fn int default_masterIN(int mpi_size, configData* d)
 * @brief This function is called before any farm operations.
 *
 * You can do something before computations starts.
 *
 * @param mpi_size
 * @param d
 *
 */
int default_masterIN(int mpi_size, configData* d){
  return 0;
}

/**
 * @fn int default_masterOUT(int nodes, configData* d, masterData* r)
 * @brief This function is called after all operations are performed.
 * 
 * Example:
 * Here, we just copy slave data files into one master file.
 *
 * @param nodes
 * @param d
 * @param r
 */
int default_masterOUT(int nodes, configData* d, masterData* r){
  
  int i = 0;
  hid_t fname, masterfile, masterdatagroup;
  herr_t stat;
  char groupname[512];
  char filename[512];

  stat = H5open();
  masterfile = H5Fopen(d->datafile,H5F_ACC_RDWR,H5P_DEFAULT);
  masterdatagroup = H5Gopen(masterfile, "data", H5P_DEFAULT);
  
  // Copy data from slaves to one master file
  for(i = 1; i < nodes; i++){
    sprintf(groupname,"slave%d", i);
    sprintf(filename,"%s-slave%d.h5", d->name,i);
   
    fname = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
    stat = H5Ocopy(fname, groupname, masterdatagroup, groupname, H5P_DEFAULT, H5P_DEFAULT);
    if(stat < 0) printf("copy error\n");
    H5Fclose(fname);

  }

  H5Gclose(masterdatagroup);
  H5Fclose(masterfile);
  stat = H5close();
  
  printf("Master process OVER & OUT.\n");
  return 0;
}

/**
 * 
 * @fn int default_master_beforeSend(int slave, configData* d, masterData* r)
 * @brief Called before send data to slaves.
 *
 * @fn int default_master_afterSend(int slave, configData* d, masterData* r)
 * @brief Called after data was send to slaves.
 *
 * @fn int default_master_beforeReceive(configData* d, masterData* r)
 * @brief Called before data receive from the slave.
 *
 * @fn int default_master_afterReceive(int slave, configData* d, masterData* r)
 * @brief Called after data is received.
 * 
 */
int default_master_beforeSend(int slave, configData* d, masterData* r){
  return 0;
}
int default_master_afterSend(int slave, configData* d, masterData* r){
  return 0;
}
int default_master_beforeReceive(configData* d, masterData* r){
  return 0;
}
int default_master_afterReceive(int slave, configData* d, masterData* r){
  return 0;
}

/**
 * USER DEFINED SLAVE_IN FUNCTION
 * 
 * Called before slave starts its work.
 *
 * Do some preparation here, i.e. 
 * -- clear proper arrays in slaveData_t s
 * -- read data to struct s, even from different files
 * -- create group/dataset for the slave etc.
 *
 * Example:
 * Here we create slave specific data file.
 * You can handle here any type of datasets etc.
 *
 * Data group is incorporated in MASTER_OUT function to one master data file.
 *
 */
int default_slaveIN(int slave, configData* d, masterData* r){

  hid_t sfile_id, sdatagroup, gid, string_type;
  hid_t dataset, dataspace;
  hid_t rank = 1;
  hsize_t dimens_1d;
  herr_t serr;
  char sbase[] = "slave";
  char node[512];
  char gbase[] = "slave";
  char group[512];
  char oldfile[1028];

  char cbase[] = "Hello from slave ";
  char comment[1024];

  struct stat st;

  sprintf(node, "%s-%s%d.h5", d->name, sbase, slave);
  sprintf(group, "%s%d", gbase, slave);

  /**
   * Imagine this:
   * each slave can create different dataspaces and datasets here, 
   * perform different computations, even read different config file!
   */
  if(stat(node,&st) == 0){
      sprintf(oldfile,"old-%s",node);
      /*printf("-> File %s exists!\n", cd.datafile);
      printf("-> I will back it up for You now\n");
      printf("-> Backuped file: %s\n",oldfile);*/
      rename(node,oldfile);
  }

  sfile_id = H5Fcreate(node, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  gid = H5Gcreate(sfile_id, group, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
 
  sprintf(comment, "%s%d. ",cbase,slave); 
  string_type = H5Tcopy(H5T_C_S1);
  H5Tset_size(string_type, strlen(comment));

  rank = 1;
  dimens_1d = 1;

  dataspace = H5Screate_simple(rank, &dimens_1d, NULL);
  
  dataset = H5Dcreate(gid, "comment", string_type, dataspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  serr = H5Dwrite(dataset, string_type, H5S_ALL, dataspace, H5P_DEFAULT, comment);

  H5Sclose(dataspace);
  H5Dclose(dataset);
  H5Gclose(gid);
  H5Fclose(sfile_id);

  return 0;
}

/**
 * USER DEFINED SLAVE_OUT FUNCTION
 *
 * Called after slave done its work.
 *
 * Example:
 * Just prints a message from the slave.
 */
int default_slaveOUT(int slave, configData* d, masterData* r){
  
  printf("SLAVE[%d] OVER & OUT\n",slave);

  return 0;
}

/**
 * USER DEFINED SLAVE HELPER FUNCTIONS
 * 
 * Called before/after send/receive
 * 
 */
int default_slave_beforeSend(int slave, configData* d, masterData* r){
  
  int i = 0;
  //printf("S[%d] px[%3d, %3d]: ", slave, r->coords[0], r->coords[1]);
  
  //for(i = 0; i < d->mrl; i++) printf("%3.0f", r->res[i]);
  //printf("\n");
  
  return 0;
}
int default_slave_afterSend(int slave, configData* d, masterData* r){
  return 0;
}
int default_slave_beforeReceive(int slave, configData* d, masterData* r){
  return 0;
}
int default_slave_afterReceive(int slave, configData* d, masterData* r){
  return 0;
}

