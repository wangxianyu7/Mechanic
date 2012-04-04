/**
 * @file
 * Simple parallel-GA implementation for the Mechanic
 *
 * The algorithm is based on the geneticAlgorithm1.c by John LeFlohic (February 24, 1999)
 * http://www-cs-students.stanford.edu/~jl/Essays/ga.html
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "MMechanic2.h"

#define TRUE 1
#define FALSE 0

#define MAX_GENERATIONS 1000

/**
 * GA functions prototypes
 */
int InitializeOrganisms(pool *p, int genes, int alleles);
int ProduceNextGeneration(pool *p, int genes, int alleles, double mutation_rate);
int EvaluateOrganisms(pool *p, int max_fitness);
int Fitness(pool *p, task *t, int genes);
int SelectOneOrganism(pool *p);

/**
 * @function
 * Implements Init()
 *
 * We overwrite here the default pools number
 */
int Init(init *i) {
  i->pools = MAX_GENERATIONS;
  return TASK_SUCCESS; 
}

/**
 * @function
 * Implements Setup()
 */
int Setup(setup *s) {
  s->options[0] = (LRC_configDefaults) {
    .space="ga", .name="max-generations", .shortName='\0', .value="1000", .type=LRC_INT, .description="Maximum generations (max 1000)"};
  s->options[1] = (LRC_configDefaults) {.space="ga", .name="genes", .shortName='\0', .value="20", .type=LRC_INT, .description="Number of genes"};
  s->options[2] = (LRC_configDefaults) {.space="ga", .name="alleles", .shortName='\0', .value="4", .type=LRC_INT, .description="Number of types of genes"};
  s->options[3] = (LRC_configDefaults) {.space="ga", .name="mutation-rate", .shortName='\0', .value="0.001", .type=LRC_DOUBLE, .description="Mutation rate"};
  s->options[4] = (LRC_configDefaults) {.space="ga", .name="max-fitness", .shortName='\0', .value="20", .type=LRC_INT, .description="Maximum fitness"};
  s->options[5] = (LRC_configDefaults) {LRC_OPTIONS_END};

  return TASK_SUCCESS; 
}

/**
 * @function
 * Implements Storage()
 */
int Storage(pool *p, setup *s) {
  int pool_size;
  int genes;

  pool_size = p->board->layout.dim[0] * p->board->layout.dim[1];
  genes = LRC_option2int("ga", "genes", s->head);

  /** 
   * Path: /Pools/pool-ID/population 
   * Current population
   *
   * The STORAGE_BASIC will store the whole dataset at once
   */
  p->storage[0].layout.path = "population";
  p->storage[0].layout.rank = 2;
  p->storage[0].layout.dim[0] = pool_size;
  p->storage[0].layout.dim[1] = genes; 
  p->storage[0].layout.use_hdf = 1;
  p->storage[0].layout.sync = 1;
  p->storage[0].layout.dataspace_type = H5S_SIMPLE;
  p->storage[0].layout.datatype = H5T_NATIVE_DOUBLE;
  p->storage[0].layout.storage_type = STORAGE_BASIC;

  /** 
   * Path: /Pools/pool-ID/model 
   * The objective -- The model organism
   */
  p->storage[1].layout.path = "model";
  p->storage[1].layout.rank = 2;
  p->storage[1].layout.dim[0] = 1;
  p->storage[1].layout.dim[1] = genes;
  p->storage[1].layout.use_hdf = 1;
  p->storage[1].layout.sync = 1;
  p->storage[1].layout.dataspace_type = H5S_SIMPLE;
  p->storage[1].layout.datatype = H5T_NATIVE_DOUBLE;
  p->storage[1].layout.storage_type = STORAGE_BASIC;

  /**
   * Path: /Pools/pool-ID/tmp-data
   * Temporary space for the pool data, not stored in the master datafile
   */
  p->storage[2].layout.path = "tmp-data";
  p->storage[2].layout.rank = 2;
  p->storage[2].layout.dim[0] = 1;
  p->storage[2].layout.dim[1] = genes;
  p->storage[2].layout.use_hdf = 0;
  p->storage[2].layout.dataspace_type = H5S_SIMPLE;
  p->storage[2].layout.datatype = H5T_NATIVE_DOUBLE;
  p->storage[2].layout.storage_type = STORAGE_BASIC;

  /**
   * Path: /Pools/pool-ID/children
   * The children of the current population
   */
  p->storage[3].layout.path = "children";
  p->storage[3].layout.rank = 2;
  p->storage[3].layout.dim[0] = pool_size;
  p->storage[3].layout.dim[1] = genes; 
  p->storage[3].layout.use_hdf = 0;
  p->storage[3].layout.sync = 1;
  p->storage[3].layout.dataspace_type = H5S_SIMPLE;
  p->storage[3].layout.datatype = H5T_NATIVE_DOUBLE;
  p->storage[3].layout.storage_type = STORAGE_BASIC;

  /**
   * Path: /Pools/pool-ID/Tasks/fitness
   * The fitness of the each organism of the population
   * (How far we are from the model one)
   *
   * The STORAGE_LIST will store the fitness in the same order as the population organisms
   * are stored in the /Pools/pool-ID/population (one-by-one task mapping)
   */
  p->task->storage[0].layout.path = "fitness";
  p->task->storage[0].layout.rank = 2;
  p->task->storage[0].layout.dim[0] = 1;
  p->task->storage[0].layout.dim[1] = 1; // 0 - fitness
  p->task->storage[0].layout.use_hdf = 1;
  p->task->storage[0].layout.dataspace_type = H5S_SIMPLE;
  p->task->storage[0].layout.datatype = H5T_NATIVE_DOUBLE;
  p->task->storage[0].layout.storage_type = STORAGE_LIST;

  return TASK_SUCCESS;
}

/**
 * @function
 * Implements PoolPrepare()
 *
 * If p->pid = 0 we initialize the population (the first generation), otherwise, we copy
 * the children of the previous generation to the population dataset. The previous
 * population is accessed through **all pointer.
 */
int PoolPrepare(pool **all, pool *p, setup *s) {
  int i, j, genes, alleles;  

  genes = LRC_option2int("ga", "genes", s->head);
  alleles = LRC_option2int("ga", "alleles", s->head);

  if (p->pid == 0) {
    InitializeOrganisms(p, genes, alleles);
  } else {
    /* Copy children of previous generation to the current one */
    for (i = 0; i < p->pool_size; i++) {
      for (j = 0; j < genes; j++) {
        p->storage[0].data[i][j] = all[p->pid-1]->storage[3].data[i][j];
      }
    }
    
    /* Copy the model */
    for (j = 0; j < genes; j++) {
      p->storage[1].data[0][j] = all[p->pid-1]->storage[1].data[0][j];
    }
  }
  
  return TASK_SUCCESS;
}

/**
 * @function
 * Implements TaskProcess()
 *
 * We compute here the fitness function (in parallel, each worker receives an organism to
 * check). We return here only the fitness of the specified organism.
 */
int TaskProcess(pool *p, task *t, setup *s) {
  int genes;
  
  genes = LRC_option2int("ga", "genes", s->head);

  t->storage[0].data[0][0] = Fitness(p, t, genes);

  return TASK_SUCCESS;
}

/**
 * @function
 * Implements PoolProcess()
 *
 * We decide here, whether the model has been reached or not. When the model is not
 * reached, we generate the next population (children of the current population). 
 */
int PoolProcess(pool **all, pool *p, setup *s) {
  int perfectGeneration = FALSE;
  int maxgen, max_fitness, genes, alleles;
  double mutation_rate;

  maxgen = LRC_option2int("ga", "max-generations", s->head);
  max_fitness = LRC_option2int("ga", "max-fitness", s->head);
  genes = LRC_option2int("ga", "genes", s->head);
  alleles = LRC_option2int("ga", "alleles", s->head);
  mutation_rate = LRC_option2double("ga", "mutation-rate", s->head);
  
  /* Has the model organism been reached? */
  perfectGeneration = EvaluateOrganisms(p, max_fitness);
  
  if (perfectGeneration == TRUE || p->pid > maxgen || p->pid > MAX_GENERATIONS) {
    printf("Generation %d with fitness %d\n", p->pid+1, (int)p->storage[2].data[0][0]);
    return POOL_FINALIZE;
  }

  /* The model organism has not been reached. We generate the next population */
  ProduceNextGeneration(p, genes, alleles, mutation_rate);

  return POOL_CREATE_NEW;
}

/**
 * geneticAlgorithm1.c based functions
 */

/**
 * @function
 * Initialize the first generation
 */
int InitializeOrganisms(pool *p, int genes, int alleles) {
  int i = 0, j = 0;

  /* Organisms */
  for (i = 0; i < p->pool_size; ++i) {
    for (j = 0; j < genes; ++j) {
      p->storage[0].data[i][j] = rand() % alleles;
    }
  }

  /* Model */
  for (i = 0; i < genes; ++i) {
    p->storage[1].data[0][i] = rand() % alleles;
  }

  return 0;
}

/**
 * @function
 * Compute the fitness of current organism (task)
 */
int Fitness(pool *p, task *t, int genes) {
  int i = 0, fit = 0;
    
  for (i = 0; i < genes; ++i) {
    if ((int)p->storage[0].data[t->tid][i] == (int)p->storage[1].data[0][i]) {
      ++fit;
    }
  }
    
  return fit;
}

/**
 * @function
 * Evaluate organisms in the current generation (pool)
 */
int EvaluateOrganisms(pool *p, int max_fitness) {
  int i;
  
  p->storage[2].data[0][0] = 0;

  /* Total fitness */
  for (i = 0; i < p->pool_size; i++) {
    p->storage[2].data[0][0] += p->task->storage[0].data[i][0];
  }

  for (i = 0; i < p->pool_size; i++) {
    if ((int)p->task->storage[0].data[i][0] == max_fitness) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
 * @function
 * Produce the next generation
 */
int ProduceNextGeneration(pool *p, int genes, int alleles, double mutation_rate) {
  int i, j;
  int parentOne;
  int parentTwo;
  int crossoverPoint;
  int mutateThisGene;

  for (i = 0; i < p->pool_size; ++i) {
    parentOne = SelectOneOrganism(p);
    parentTwo = SelectOneOrganism(p);
    crossoverPoint = rand() % genes;

    for (j = 0; j < genes; ++j) {
      mutateThisGene = rand() % (int)(1.0/mutation_rate);
      if (mutateThisGene == 0) {
        p->storage[3].data[i][j] = rand() % alleles;
      } else {
        if (j < crossoverPoint) {
          p->storage[3].data[i][j] = p->storage[0].data[parentOne][j];
        } else {
          p->storage[3].data[i][j] = p->storage[0].data[parentTwo][j];
        }
      }
    }
  }

  return 0;
}

/**
 * @function
 * Selects the parent
 */
int SelectOneOrganism(pool *p) {
  int i = 0, t = 0, r = 0;
  
  r = rand() % ((int)p->storage[2].data[0][0] + 1);

  for (i = 0; i < p->pool_size; ++i) {
    t += (int)p->task->storage[0].data[i][0];
    if (t >= r) {
      return i;
    }
  }
  return 0;
}

