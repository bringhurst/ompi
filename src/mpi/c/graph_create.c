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

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "communicator/communicator.h"
#include "errhandler/errhandler.h"
#include "mca/topo/topo.h"
#include "mca/topo/base/base.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Graph_create = PMPI_Graph_create
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Graph_create";


int MPI_Graph_create(MPI_Comm old_comm, int nnodes, int *index,
                     int *edges, int reorder, MPI_Comm *comm_graph) 
{

    int err;
    bool re_order = false;

    /* check the arguments */
    if (MPI_PARAM_CHECK) {
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (MPI_COMM_NULL == old_comm) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_COMM,
                                           FUNC_NAME);
        }
        if (OMPI_COMM_IS_INTER(old_comm)) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_COMM,
                                           FUNC_NAME);
        }
        if ( (1 > nnodes) || (NULL == index) || (NULL == edges) ) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_ARG,
                                           FUNC_NAME);
        }

        if (nnodes > ompi_comm_size(old_comm)) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_ARG,
                                           FUNC_NAME);
        }

        if ((0 > reorder) || (1 < reorder)) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_ARG,
                                           FUNC_NAME);
        }
    }

    /*
     * Now we have to check if the topo module exists or not. This has been
     * removed from initialization since most of the MPI calls do not use 
     * this module 
     */
    if (!(mca_topo_base_components_opened_valid ||
          mca_topo_base_components_available_valid)) {
        bool user_threads = true;
        bool hidden_threads = true;
        if (OMPI_SUCCESS != (err = mca_topo_base_open())) {
            return OMPI_ERRHANDLER_INVOKE(old_comm, err, FUNC_NAME);
        }
        if (OMPI_SUCCESS != (err = mca_topo_base_find_available (&user_threads,
                                                                 &hidden_threads))) {
            return OMPI_ERRHANDLER_INVOKE(old_comm, err, FUNC_NAME);
        }
    }

    /* 
     * everything seems to be alright with the communicator, we can go 
     * ahead and select a topology module for this purpose and create 
     * the new graph communicator
     */

    re_order = (1 == reorder) ? true:false;

    err = ompi_topo_create ((struct ompi_communicator_t *)old_comm,
                            nnodes,
                            index,
                            edges,
                            re_order,
                            (struct ompi_communicator_t **)comm_graph,
                            OMPI_COMM_GRAPH);

    /* check the error status */
    if (MPI_SUCCESS != err) {
        return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, err, FUNC_NAME);
    }
    
    /* All done */
    return MPI_SUCCESS;
}
