/*
 * $HEADER$
 */

#include "ompi_config.h"

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "communicator/communicator.h"
#include "errhandler/errhandler.h"
#include "file/file.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_File_set_errhandler = PMPI_File_set_errhandler
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_File_set_errhandler";


int MPI_File_set_errhandler( MPI_File file, MPI_Errhandler errhandler) 
{
  /* Error checking */

  if (MPI_PARAM_CHECK) {
    OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
    if (NULL == file || 
        MPI_FILE_NULL == file) {
      return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_FILE,
                                    FUNC_NAME);
    } else if (NULL == errhandler ||
               MPI_ERRHANDLER_NULL == errhandler ||
               OMPI_ERRHANDLER_TYPE_FILE != errhandler->eh_mpi_object_type) {
      return OMPI_ERRHANDLER_INVOKE(file, MPI_ERR_ARG, FUNC_NAME);
    }
  }

  /* Ditch the old errhandler, and decrement its refcount */

  OBJ_RELEASE(file->error_handler);

  /* We have a valid comm and errhandler, so increment its refcount */

  file->error_handler = errhandler;
  OBJ_RETAIN(file->error_handler);

  /* All done */

  return MPI_SUCCESS;
}
