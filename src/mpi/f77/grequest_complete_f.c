/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_GREQUEST_COMPLETE = mpi_grequest_complete_f
#pragma weak pmpi_grequest_complete = mpi_grequest_complete_f
#pragma weak pmpi_grequest_complete_ = mpi_grequest_complete_f
#pragma weak pmpi_grequest_complete__ = mpi_grequest_complete_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_GREQUEST_COMPLETE,
                           pmpi_grequest_complete,
                           pmpi_grequest_complete_,
                           pmpi_grequest_complete__,
                           pmpi_grequest_complete_f,
                           (MPI_Fint *request, MPI_Fint *ierr),
                           (request, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_GREQUEST_COMPLETE = mpi_grequest_complete_f
#pragma weak mpi_grequest_complete = mpi_grequest_complete_f
#pragma weak mpi_grequest_complete_ = mpi_grequest_complete_f
#pragma weak mpi_grequest_complete__ = mpi_grequest_complete_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_GREQUEST_COMPLETE,
                           mpi_grequest_complete,
                           mpi_grequest_complete_,
                           mpi_grequest_complete__,
                           mpi_grequest_complete_f,
                           (MPI_Fint *request, MPI_Fint *ierr),
                           (request, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_grequest_complete_f(MPI_Fint *request, MPI_Fint *ierr)
{
  /* This function not yet implemented */
}
