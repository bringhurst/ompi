/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_FILE_GET_ERRHANDLER = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler_ = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler__ = mpi_file_get_errhandler_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_FILE_GET_ERRHANDLER,
                           pmpi_file_get_errhandler,
                           pmpi_file_get_errhandler_,
                           pmpi_file_get_errhandler__,
                           pmpi_file_get_errhandler_f,
                           (MPI_Fint *file, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (file, errhandler, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_FILE_GET_ERRHANDLER = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler_ = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler__ = mpi_file_get_errhandler_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_FILE_GET_ERRHANDLER,
                           mpi_file_get_errhandler,
                           mpi_file_get_errhandler_,
                           mpi_file_get_errhandler__,
                           mpi_file_get_errhandler_f,
                           (MPI_Fint *file, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (file, errhandler, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_file_get_errhandler_f(MPI_Fint *fh, MPI_Fint *errhandler, MPI_Fint *ierr)
{
    MPI_File c_fh;
    MPI_Errhandler c_errhandler;

    c_fh = MPI_File_f2c(*fh);
    
    *ierr = OMPI_INT_2_FINT(MPI_File_get_errhandler(c_fh, &c_errhandler));
    
    *errhandler = MPI_Errhandler_c2f(c_errhandler);
}
