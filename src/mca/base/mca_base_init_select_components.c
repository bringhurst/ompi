/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "include/constants.h"
#include "class/ompi_list.h"
#include "mca/base/base.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/base.h"
#include "mca/ptl/ptl.h"
#include "mca/ptl/base/base.h"
#include "mca/pml/pml.h"
#include "mca/pml/base/base.h"
#include "mca/mpool/base/base.h"
#include "mca/io/base/base.h"


/*
 * Look at available pml, ptl, and coll modules and find a set that
 * works nicely.  Also set the final MPI thread level.  There are many
 * factors involved here, and this first implementation is rather
 * simplistic.  
 *
 * The contents of this function will likely be replaced 
 */
int mca_base_init_select_components(int requested, 
                                    bool allow_multi_user_threads,
                                    bool have_hidden_threads, int *provided)
{
  bool user_threads, hidden_threads;

  /* Make final lists of available modules (i.e., call the query/init
     functions and see if they return happiness).  For pml, there will
     only be one (because there's only one for the whole process), but
     for ptl and coll, we'll get lists back. */

  if (OMPI_SUCCESS != mca_mpool_base_init(&user_threads)) {
    return OMPI_ERROR;
  }
  allow_multi_user_threads &= user_threads;

  /* JMS: At some point, we'll need to feed it the thread level to
     ensure to pick one high enough (e.g., if we need CR) */

  user_threads = true;
  hidden_threads = false;
  if (OMPI_SUCCESS != mca_pml_base_select(&mca_pml, 
                                         &user_threads, &hidden_threads)) {
    return OMPI_ERROR;
  }
  allow_multi_user_threads &= user_threads;
  have_hidden_threads |= hidden_threads;

  if (OMPI_SUCCESS != mca_ptl_base_select(&user_threads, &hidden_threads)) {
    return OMPI_ERROR;
  }
  allow_multi_user_threads &= user_threads;
  have_hidden_threads |= hidden_threads;

  if (OMPI_SUCCESS != mca_coll_base_find_available(&user_threads, 
                                                   &hidden_threads)) {
    return OMPI_ERROR;
  }
  allow_multi_user_threads &= user_threads;
  have_hidden_threads |= hidden_threads;

  /* io and topo components are selected later, because the io framework is
     opened lazily (at the first MPI_File_* function invocation). */

  /* Now that we have a final list of all available modules, do the
     selection.  pml is already selected. */

  /* JMS ...Do more here with the thread level, etc.... */

  *provided = requested;
  if (have_hidden_threads) {
      ompi_set_using_threads(true);
  }

  /* Tell the selected pml module about all the selected ptl
     modules */

  mca_pml.pml_add_ptls(&mca_ptl_base_modules_initialized);

  /* All done */

  return OMPI_SUCCESS;
}

