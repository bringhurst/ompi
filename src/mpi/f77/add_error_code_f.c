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

#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_ADD_ERROR_CODE = mpi_add_error_code_f
#pragma weak pmpi_add_error_code = mpi_add_error_code_f
#pragma weak pmpi_add_error_code_ = mpi_add_error_code_f
#pragma weak pmpi_add_error_code__ = mpi_add_error_code_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_ADD_ERROR_CODE,
                           pmpi_add_error_code,
                           pmpi_add_error_code_,
                           pmpi_add_error_code__,
                           pmpi_add_error_code_f,
                           (MPI_Fint *errorclass, MPI_Fint *errorcode, MPI_Fint *ierr),
                           (errorclass, errorcode, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_ADD_ERROR_CODE = mpi_add_error_code_f
#pragma weak mpi_add_error_code = mpi_add_error_code_f
#pragma weak mpi_add_error_code_ = mpi_add_error_code_f
#pragma weak mpi_add_error_code__ = mpi_add_error_code_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_ADD_ERROR_CODE,
                           mpi_add_error_code,
                           mpi_add_error_code_,
                           mpi_add_error_code__,
                           mpi_add_error_code_f,
                           (MPI_Fint *errorclass, MPI_Fint *errorcode, MPI_Fint *ierr),
                           (errorclass, errorcode, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_add_error_code_f(MPI_Fint *errorclass, MPI_Fint *errorcode, MPI_Fint *ierr)
{
    OMPI_SINGLE_NAME_DECL(errorcode);

    *ierr = OMPI_INT_2_FINT(MPI_Add_error_code(OMPI_FINT_2_INT(*errorclass),
					       OMPI_SINGLE_NAME_CONVERT(errorcode)
					       ));
    
    OMPI_SINGLE_INT_2_FINT(errorcode);

}
