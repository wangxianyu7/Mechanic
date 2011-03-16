/*
 * MECHANIC
 *
 * Copyright (c) 2010-2011, Mariusz Slonina (Nicolaus Copernicus University)
 * All rights reserved.
 *
 * This file is part of MECHANIC code.
 *
 * MECHANIC was created to help solving many numerical problems by providing
 * tools for improving scalability and functionality of the code. MECHANIC was
 * released in belief it will be useful. If you are going to use this code, or
 * its parts, please consider referring to the authors either by the website
 * or the user guide reference.
 *
 * http://git.astri.umk.pl/projects/mechanic
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Nicolaus Copernicus University nor the names of
 *   its contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "mechanic.h"
#include "mechanic_internals.h"
#include "mechanic_mode_masteralone.h"

int mechanic_mode_masteralone(int mpi_size, int node, mechanic_internals handler,
    moduleInfo* md, configData* d){

  int j = 0, farm_res = 0, mstat = 0, tab[3], check = 0;
  int npxc = 0;

  masterData result;
  masterData inidata;

  /* Checkpoint storage */
  int vecsize;
  int* coordsvec;
  int** coordsarr;
  MECHANIC_DATATYPE *resultsvec;
  MECHANIC_DATATYPE **resultarr;

  /* Restart mode board */
  int** board;
  int computed = 0;
  int pixeldiff = 0;
  int totalnumofpx = 0;

  module_query_int_f query;

  /* Allocate memory */
  result.res = AllocateDoubleVec(md->mrl);
  inidata.res = AllocateDoubleVec(md->irl);

  coordsarr = AllocateInt2D(d->checkpoint,3);
  resultarr = AllocateDouble2D(d->checkpoint,md->mrl);

  /* Allocate memory for board */
  board = AllocateInt2D(d->xres,d->yres);

<<<<<<< HEAD
  vecsize = 3;

  /* For the sake of simplicity we read board everytime,
   * both in restart and clean simulation mode */
  computed = H5readBoard(handler->config, board);
  mechanic_message(MECHANIC_MESSAGE_INFO, "Num of computed pixels = %d\n", computed);
=======
  computed = H5readBoard(d, board);
>>>>>>> parent of 7783d3f... API change: ModuleInfo and ConfigData merged. Masteralone broken.

  /* Master can do something useful before computations,
   * even in masteralone mode */
  query = mechanic_load_sym(handler, "in", MECHANIC_MODULE_SILENT);
  if (query) mstat = query(mpi_size, node, md, d, &inidata);
  mechanic_check_mstat(mstat);

  /* Align farm resolution for given method. */
  query = mechanic_load_sym(handler, "farmResolution", MECHANIC_MODULE_ERROR);
<<<<<<< HEAD
  if (query) farm_res = query(handler);
  if (farm_res > (handler->config->xres * handler->config->yres)) {
    mechanic_message(MECHANIC_MESSAGE_ERR,
      "Farm resolution should not exceed x*y!\n");
    mechanic_error(MECHANIC_ERR_SETUP);
  }
=======
  if (query) farm_res = query(d->xres, d->yres, md, d);
>>>>>>> parent of 7783d3f... API change: ModuleInfo and ConfigData merged. Masteralone broken.

  pixeldiff = farm_res - computed;

  if (pixeldiff == 0) goto finalize;

  /* Perform farm operations */
  while (1) {

    npxc = map2d(npxc, handler, md, d, tab, board);
    totalnumofpx++;

    inidata.coords[0] = tab[0];
    inidata.coords[1] = tab[1];
    inidata.coords[2] = tab[2];
		
    result.coords[0] = tab[0];
    result.coords[1] = tab[1];
    result.coords[2] = tab[2];
    
    mechanic_message(MECHANIC_MESSAGE_DEBUG, "TAB [%d, %d, %d]\t",
      tab[0], tab[1], tab[2]);

    query = mechanic_load_sym(handler, "pixelCoords", MECHANIC_MODULE_ERROR);
    if (query) mstat = query(node, tab, md, d, &inidata, &result);
    mechanic_check_mstat(mstat);

    query = mechanic_load_sym(handler, "preparePixel", MECHANIC_MODULE_SILENT);
    if (query) mstat = query(node, md, d, &inidata, &result);
    mechanic_check_mstat(mstat);

    query = mechanic_load_sym(handler, "beforeProcessPixel", MECHANIC_MODULE_SILENT);
    if (query) mstat = query(node, md, d, &inidata, &result);
    mechanic_check_mstat(mstat);

    /* PIXEL COMPUTATION */
    query = mechanic_load_sym(handler, "processPixel", MECHANIC_MODULE_ERROR);
    if (query) mstat = query(node, md, d, &inidata, &result);
    mechanic_check_mstat(mstat);

    query = mechanic_load_sym(handler, "afterProcessPixel", MECHANIC_MODULE_SILENT);
    if (query) mstat = query(node, md, d, &inidata, &result);
    mechanic_check_mstat(mstat);

    /* Copy data to checkpoint arrays */
    coordsarr[check][0] = result.coords[0];
    coordsarr[check][1] = result.coords[1];
    coordsarr[check][2] = result.coords[2];

		mechanic_message(MECHANIC_MESSAGE_DEBUG, "MASTER [%d, %d, %d]\t",
        result.coords[0], result.coords[1], result.coords[2]);

    for (j = 0; j < md->mrl; j++) {
      resultarr[check][j] = result.res[j];
      mechanic_message(MECHANIC_MESSAGE_DEBUG, "%2.2f\t", result.res[j]);
    }

		mechanic_message(MECHANIC_MESSAGE_DEBUG, "\n");

<<<<<<< HEAD
    if (((check+1) % handler->config->checkpoint) == 0 || mechanic_ups() < 0) {
      
      /* Fortran interoperability:
       * Convert 2D coordinates array to 1D vector, as well as
       * results array */
      coordsvec = IntArrayToVec(coordsarr, handler->config->checkpoint, vecsize);
      resultsvec = DoubleArrayToVec(resultarr, handler->config->checkpoint, handler->info->mrl);
      
      query = mechanic_load_sym(handler, "beforeCheckpoint", MECHANIC_MODULE_SILENT);
      if (query) mstat = query(handler, coordsvec, resultsvec);
      mechanic_check_mstat(mstat);

      mstat = atCheckPoint(handler, check+1, coordsarr, board, resultarr);
=======
    check++;

    if (check % d->checkpoint == 0 || mechanic_ups() < 0) {
      query = mechanic_load_sym(handler, "beforeCheckpoint", MECHANIC_MODULE_SILENT);
      if (query) mstat = query(mpi_size, md, d, coordsarr, resultarr);
      mechanic_check_mstat(mstat);

      mstat = atCheckPoint(check, coordsarr, board, resultarr, md, d);
>>>>>>> parent of 7783d3f... API change: ModuleInfo and ConfigData merged. Masteralone broken.
      mechanic_check_mstat(mstat);
      check = 0;

      query = mechanic_load_sym(handler, "afterCheckpoint", MECHANIC_MODULE_SILENT);
<<<<<<< HEAD
      if (query) mstat = query(handler, coordsvec, resultsvec);
=======
      if (query) mstat = query(mpi_size, md, d, coordsarr, resultarr);
>>>>>>> parent of 7783d3f... API change: ModuleInfo and ConfigData merged. Masteralone broken.
      mechanic_check_mstat(mstat);
      
      FreeIntVec(coordsvec);
      FreeDoubleVec(resultsvec);
    }

    check++;

    mechanic_message(MECHANIC_MESSAGE_CONT,
        "[%04d / %04d] Pixel [%04d, %04d, %04d] computed\n",
        totalnumofpx, pixeldiff,  result.coords[0], result.coords[1],
        result.coords[2]);

    if (npxc >= farm_res) break;
  }

  /* Write outstanding results */
  if (check > 0) {
      
      /* Fortran interoperability:
       * Convert 2D coordinates array to 1D vector, as well as
       * results array */
      coordsvec = IntArrayToVec(coordsarr, handler->config->checkpoint, vecsize);
      resultsvec = DoubleArrayToVec(resultarr, handler->config->checkpoint, handler->info->mrl);
    
      query = mechanic_load_sym(handler, "beforeCheckpoint", MECHANIC_MODULE_SILENT);
<<<<<<< HEAD
      if (query) mstat = query(handler, coordsvec, resultsvec);

      mstat = atCheckPoint(handler, check+1, coordsarr, board, resultarr);
      mechanic_check_mstat(mstat);

      query = mechanic_load_sym(handler, "afterCheckpoint", MECHANIC_MODULE_SILENT);
      if (query) mstat = query(handler, coordsvec, resultsvec);
=======
      if (query) mstat = query(mpi_size, md, d, coordsarr, resultarr);
      mstat = atCheckPoint(check, coordsarr, board, resultarr, md, d);
      mechanic_check_mstat(mstat);

      query = mechanic_load_sym(handler, "afterCheckpoint", MECHANIC_MODULE_SILENT);
      if (query) mstat = query(mpi_size, md, d, coordsarr, resultarr);
>>>>>>> parent of 7783d3f... API change: ModuleInfo and ConfigData merged. Masteralone broken.
      mechanic_check_mstat(mstat);
      
      FreeIntVec(coordsvec);
      FreeDoubleVec(resultsvec);
  }

  /* FARM ENDS */

finalize:

  /* Master can do something useful after the computations. */
  query = mechanic_load_sym(handler, "out", MECHANIC_MODULE_SILENT);
  if (query) mstat = query(1, node, md, d, &inidata, &result);
  mechanic_check_mstat(mstat);

  FreeDoubleVec(result.res);
  FreeDoubleVec(inidata.res);
  FreeInt2D(coordsarr,d->checkpoint);
  FreeDouble2D(resultarr,d->checkpoint);
  FreeInt2D(board,d->xres);

  return mstat;
}

