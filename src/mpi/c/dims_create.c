/*
 * $HEADER$
 */

#include "ompi_config.h"

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "communicator/communicator.h"
#include "errhandler/errhandler.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Dims_create = PMPI_Dims_create
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Dims_create";


int MPI_Dims_create(int nnodes, int ndims, int *dims) 
{
  if (MPI_PARAM_CHECK) {
    OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
  }

  /* This function is not yet implemented */

  return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_INTERN, FUNC_NAME);
}
