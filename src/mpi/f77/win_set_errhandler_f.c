/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "errhandler/errhandler.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_WIN_SET_ERRHANDLER = mpi_win_set_errhandler_f
#pragma weak pmpi_win_set_errhandler = mpi_win_set_errhandler_f
#pragma weak pmpi_win_set_errhandler_ = mpi_win_set_errhandler_f
#pragma weak pmpi_win_set_errhandler__ = mpi_win_set_errhandler_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_WIN_SET_ERRHANDLER,
                           pmpi_win_set_errhandler,
                           pmpi_win_set_errhandler_,
                           pmpi_win_set_errhandler__,
                           pmpi_win_set_errhandler_f,
                           (MPI_Fint *win, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (win, errhandler, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_WIN_SET_ERRHANDLER = mpi_win_set_errhandler_f
#pragma weak mpi_win_set_errhandler = mpi_win_set_errhandler_f
#pragma weak mpi_win_set_errhandler_ = mpi_win_set_errhandler_f
#pragma weak mpi_win_set_errhandler__ = mpi_win_set_errhandler_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_WIN_SET_ERRHANDLER,
                           mpi_win_set_errhandler,
                           mpi_win_set_errhandler_,
                           mpi_win_set_errhandler__,
                           mpi_win_set_errhandler_f,
                           (MPI_Fint *win, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (win, errhandler, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_win_set_errhandler_f(MPI_Fint *win, MPI_Fint *errhandler,
			      MPI_Fint *ierr)
{
    MPI_Win c_win = MPI_Win_f2c(*win);
    MPI_Errhandler c_err = MPI_Errhandler_f2c(*errhandler);

    *ierr = OMPI_INT_2_FINT(MPI_Win_set_errhandler(c_win, c_err));
    if ( MPI_SUCCESS == *ierr &&
	 OMPI_ERRHANDLER_TYPE_PREDEFINED != c_err->eh_mpi_object_type ) {
	c_err->eh_fortran_function = true ;
    }
}
