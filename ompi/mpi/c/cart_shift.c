/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "ompi_config.h"
#include <stdio.h>

#include "mpi/c/bindings.h"
#include "ompi/mca/topo/topo.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Cart_shift = PMPI_Cart_shift
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Cart_shift";


int MPI_Cart_shift(MPI_Comm comm, int direction, int disp,
                   int *rank_source, int *rank_dest) 
{
    int err;
    mca_topo_base_module_cart_shift_fn_t func;

    /* check the arguments */
    if (MPI_PARAM_CHECK) {
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (MPI_COMM_NULL == comm) {
            return OMPI_ERRHANDLER_INVOKE (MPI_COMM_WORLD, MPI_ERR_COMM,
                                          FUNC_NAME);
        }
        if (OMPI_COMM_IS_INTER(comm)) { 
            return OMPI_ERRHANDLER_INVOKE (comm, MPI_ERR_COMM,
                                          FUNC_NAME);
        }
        if (!OMPI_COMM_IS_CART(comm)) {
            return OMPI_ERRHANDLER_INVOKE (comm, MPI_ERR_TOPOLOGY,
                                          FUNC_NAME);
        }
        if (0 > direction) { /* yet to detect direction >= comm->c_topo_ndims */
            return OMPI_ERRHANDLER_INVOKE (comm, MPI_ERR_DIMS,
                                          FUNC_NAME);
        }
        if (NULL == rank_source || NULL == rank_dest) {
            return OMPI_ERRHANDLER_INVOKE (comm, MPI_ERR_ARG,
                                          FUNC_NAME);
        }
    }

    /* get the function pointer on this communicator */
    func = comm->c_topo->topo_cart_shift;

    /* call the function */
    if ( MPI_SUCCESS != 
            (err = func(comm, direction, disp, rank_source, rank_dest))) {
        return OMPI_ERRHANDLER_INVOKE(comm, err, FUNC_NAME);
    }

    /* all done */
    return MPI_SUCCESS;
}
