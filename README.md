Mechanic
--------

### Overview

The Mechanic is a task management system and host software framework developed to help in
massive numerical simulations. It provides powerful and flexible user API with unified
data storage and management. It relies on the core-module approach, which allows to
separate the numerical problem from the common programming tasks, such as setup, storage,
task management, splitting the workload, checkpointing etc. From this point of view it
allows the user to focus on the numerical part of the scientific problem only, without
digging into MPI or advanced data storage. Since the user API is written in plain C, it
allows to easily adapt any code developed with a C-interoperable programming language,
such as C++, Fortran2003+, OpenCL, CUDA etc.

The core code is based on the MPI Task Farm model and the HDF5 data storage specification.
It may be installed system-wide and become a helper tools for users, who need to perform a
large number of computations. The code has been tested on large CPU-clusters, as well as
desktop computers and works equally well (the Linux and MAC OS X operating systems are
actively maintained).

Mechanic is BSD-licensed. The source code package comes with few example
modules and is freely available at http://git.astri.umk.pl/projects/mechanic

### How does it work?

Consider an example of generation of an image (sets of points). In a single-threaded software,
it requires a loop over all points:

    for (i = 0; i < xdim; i++) {
      for (j = 0; j < ydim; j++) {
        result = PixelCompute(i, j);
      }
    }

It works well, however, the problem arises, when the `PixelCompute()` function takes a long
time to finish (especially in dynamical astronomy, the field the Mechanic came from).
Since each `PixelCompute()` call is independent, we may try to split the workload by using
some parallel techniques. In such a case, we will loop over all tasks to do, this time,
however, each parallel thread receives a single task, and return the result to the master thread. 
This is what Mechanic does. It sends a single `PixelCompute()` task to each worker node in
the computing pool (say CPU cluster) and combine the results (this is the so-called _MPI
Task Farm model_). Under the hood the Mechanic is independent from the numerical problem
itself -- it provides a unified way to create and access data files, setup,
node-communication etc.

### Key features

- **The numerical part of the code is fully separated from the setup and storage phase.** You may
  use the user API to adjust the storage layout and configuration options
- **Storage of data and configuration.** The core code takes care on the data storage and
  configuration
- **The pool-based simulations.** Each pool may have different number of task to compute
  (the task loop follows the MPI Task Farm model), and data from all pools may be used at
  the every stage of the simulation
- **MPI Task Farm** model (master -- worker relationships between nodes in a computing
  pool)
- **MPI non-blocking communication**
- **HDF5 data storage layout**
- **Automatic backup of data files and restart mode**
- **Configuration command line.** All configuration options defined through API are
  automatically available in the command line
- **Different storage modes** which allows to design the storage layout that fits best
  the user needs (i.e. for processing with Gnuplot or Matplotlib)
- **Linux and MAC OS X** supported

### Current limitations

- The data is stored with H5T_NATIVE_DOUBLE datatype, and datasets are H5S_SIMPLE-type
- Only rank 2 datasets are supported right now

### Example usage

- **Efficient creation of dynamical maps.** Each pixel of the map is mapped into
  a standalone numerical task
- **Genetic algorithms.** Each pool of tasks may be treated as a generation in the
  language of GAs
- **Data processing.** Think about processing huge number of astronomical observations

### Quick start

As an quick and simple example consider creating a dynamical map. A dynamical map helps to
obtain global information of the dynamics of the dynamical system. We would like to know
the state of the system when we move from the nominal solution (i.e. in the range of
uncertainity of observations). To do so, we create a grid of initial conditions (a map),
close to the nominal solution. Each point of the map is a standalone numerical simulation,
and we may speed the computations by splitting the workload in a farm model.  

First, we would like to define some sensible storage layout for the result. We will store
the location of the point of the map and the state of the system (1x3 array). Let us
create a `mechanic_module_map.c` file and put in it the following code:

    #include "MMechanic2.h"

    int Storage(pool *p, setup *s) {
      p->task->storage[0].layout = (schema) {
        .path = "result", // the name of the output dataset
        .rank = 2, // the rank of the dataset (only rank = 2 is currently supported)
        .dim[0] = 1, // the vertical dimension of the result array (not the dataset)
        .dim[1] = 3, // the horizontal dimension of the result array (not the dataset)
        .use_hdf = 1, // whether to store the result in the master data file
        .storage_type = STORAGE_PM3D, // storage type, which is suitable to process with Gnuplot PM3D
      };

      return SUCCESS;
    }

Each worker node will return such 1x3 array. The master node will receive these arrays and
combine them into one result dataset. We tell the Mechanic, that the output dataset should
be suitable to be processed with Gnuplot PM3D (STORAGE_PM3D flag). 

The second step is to create the TaskProcess() function, in which we will compute the
state of the system:

    int TaskProcess(pool *p, task *t, setup *s) {
      t->storage[0].data[0][0] = t->location[1]; // the horizontal position of the current task
      t->storage[0].data[0][1] = t->location[0]; // the vertical position of the current task
      t->storage[0].data[0][2] = t->tid; // task id represents the state of the system

      return SUCCESS;
    }

The `data` array is automatically allocated using the information provided in the
`Storage()` hook.

Our state of the system is represented here by the unique task ID (you may use of course
any numerical function here to get the result). The task location is filled up
automatically during the task preparation phase. Each worker node receives a unique task
to compute. 

We should now compile our code to a shared library:

    mpicc -fPIC -Dpic -shared mechanic_module_map.c -o libmechanic_module_map.so

After all, we may run the code for a 10x10px map, using four MPI threads, one master and three workers:

    mpirun -np 4 mechanic2 -p map -x 10 -y 10

The result is stored in the `mechanic-master-00.h5` file and may be accessed i.e. through
`h5dump` utility. The list the contents of the file, try

    # h5ls -r mechanic-master-00.h5

    /                        Group
    /LRC_Config              Type
    /Pools                   Group
    /Pools/last              Group
    /Pools/last/Tasks        Group
    /Pools/last/Tasks/result Dataset {100, 3}
    /Pools/last/board        Dataset {10, 10}
    /Pools/pool-0000         Group, same as /Pools/last
    /config                  Group
    /config/core             Group
    /config/core/core        Dataset {13}
    /config/module           Group
    /config/module/core      Dataset {13}

And our result:

    # h5dump -d/Pools/pool-0000/Tasks/result mechanic-master-00.h5

    DATASET "/Pools/pool-0000/Tasks/result" {
       DATATYPE  H5T_IEEE_F64LE
       DATASPACE  SIMPLE { ( 100, 3 ) / ( 100, 3 ) }
       DATA {
       (0,0): 0, 0, 0,
       (1,0): 1, 0, 10,
       (2,0): 2, 0, 20,
       (3,0): 3, 0, 30,
       (4,0): 4, 0, 40,
       (5,0): 5, 0, 50,
       (6,0): 6, 0, 60,
       (7,0): 7, 0, 70,
       (8,0): 8, 0, 80,
       (9,0): 9, 0, 90,

       (... the output cutted off)
    }

### Differences to other task management systems

The Mechanic differs significantly from other task management systems, such as Condor or
Workqueue, in terms of user API: the code does not use the executable of user's serial
code. Instead, the user's code should be rewritten within the provided API. In such a way, we
focus only on the numerical part of the task, and not its setup or storage. The HDF5
storage layer provides unified way to access the data by a number of different
applications (not to mention C/Fortran codes only, but also Python software)

### Short history of Mechanic

The idea of creating the Mechanic came from the problem of efficient computing of dynamical maps of
planetary systems. Such maps consist of many many independent numerical simulations which
may take a long time (from seconds to weeks). Efficient splitting of such workload was
neccessary to determine the global dynamics of exo-planets. The MPI-software-skeleton-kind idea
that came once to K. Goździewski mind was expanded and developed by his PhD student, M.
Słonina. The very first branch of the Mechanic, proof-of-concept 0.12 was successfully used on several
clusters and became a good starting point for the full featured software.

### Publications

- Slonina M., Gozdziewski K., Migaszewski C., 2012arXiv1202.6513S
- Migaszewski C., Slonina M., Gozdziewski K., 2012arXiv1205.0822M
- Gozdziewski K. et al, 2012arXiv1205.4164G
- Slonina M., Gozdziewski K., Migaszewski C., Rozenkiewicz A., 2012arXiv1205.1341S
- Slonina M., Gozdziewski K., Migaszewski C., Astrophysics Source Code Library, record
ascl:1205.001

### Posters

- Slonina M., Gozdziewski K., Migaszewski C., Simtech2011 (Stuttgart, June 2011)
- Slonina M., Gozdziewski K., Migaszewski C., Orbital Couples: "Pas de Deux" in the Solar
  System and the Milky Way (Paris, October 2011)

### Acknowledgments

This project is supported by the Polish Ministry of Science and Higher Education through
the grant N/N203/402739. This work is conducted within the POWIEW project of the European
Regional Development Fund in Innovative Economy Programme POIG.02.03.00-00-018/08.
