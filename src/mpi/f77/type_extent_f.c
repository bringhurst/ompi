/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_TYPE_EXTENT = mpi_type_extent_f
#pragma weak pmpi_type_extent = mpi_type_extent_f
#pragma weak pmpi_type_extent_ = mpi_type_extent_f
#pragma weak pmpi_type_extent__ = mpi_type_extent_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_TYPE_EXTENT,
                           pmpi_type_extent,
                           pmpi_type_extent_,
                           pmpi_type_extent__,
                           pmpi_type_extent_f,
                           (MPI_Fint *type, MPI_Fint *extent, MPI_Fint *ierr),
                           (type, extent, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_TYPE_EXTENT = mpi_type_extent_f
#pragma weak mpi_type_extent = mpi_type_extent_f
#pragma weak mpi_type_extent_ = mpi_type_extent_f
#pragma weak mpi_type_extent__ = mpi_type_extent_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_TYPE_EXTENT,
                           mpi_type_extent,
                           mpi_type_extent_,
                           mpi_type_extent__,
                           mpi_type_extent_f,
                           (MPI_Fint *type, MPI_Fint *extent, MPI_Fint *ierr),
                           (type, extent, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_type_extent_f(MPI_Fint *type, MPI_Fint *extent, MPI_Fint *ierr)
{
    MPI_Datatype c_type = MPI_Type_f2c(*type);
    MPI_Aint c_extent;

    *ierr = MPI_Type_extent(c_type, &c_extent);

    if (*ierr == MPI_SUCCESS)
      *extent = (MPI_Fint)c_extent;
}
