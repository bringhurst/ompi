/*
 * $HEADER$
 */

#include "ompi_config.h"
#include "coll_basic.h"

#include "mpi.h"
#include "include/constants.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/coll_tags.h"
#include "coll_basic.h"


/*
 *	gatherv_intra
 *
 *	Function:	- basic gatherv operation
 *	Accepts:	- same arguments as MPI_Bcast()
 *	Returns:	- MPI_SUCCESS or error code
 */
int mca_coll_basic_gatherv_intra(void *sbuf, int scount, 
                                 struct ompi_datatype_t *sdtype,
                                 void *rbuf, int *rcounts, int *disps,
                                 struct ompi_datatype_t *rdtype, int root,
                                 struct ompi_communicator_t *comm)
{
  int i;
  int rank;
  int size;
  int err;
  char *ptmp;
  long lb;
  long extent;

  size = ompi_comm_size(comm);
  rank = ompi_comm_rank(comm);

  /* Everyone but root sends data and returns. */

  if (rank != root) {
    err = mca_pml.pml_send(sbuf, scount, sdtype, root,
                           MCA_COLL_BASE_TAG_GATHERV, 
                           MCA_PML_BASE_SEND_STANDARD, comm);
    return err;
  }

  /* I am the root, loop receiving data. */

  err = ompi_ddt_get_extent(rdtype, &lb, &extent);
  if (OMPI_SUCCESS != err) {
    return OMPI_ERROR;
  }

  for (i = 0; i < size; ++i) {
    ptmp = ((char *) rbuf) + (extent * disps[i]);

    /* simple optimization */

    if (i == rank) {
      err = ompi_ddt_sndrcv(sbuf, scount, sdtype,
                           ptmp, rcounts[i], rdtype, 
                           MCA_COLL_BASE_TAG_GATHERV, comm);
    } else {
      err = mca_pml.pml_recv(ptmp, rcounts[i], rdtype, i,
                             MCA_COLL_BASE_TAG_GATHERV, 
                             comm, MPI_STATUS_IGNORE);
    }

    if (MPI_SUCCESS != err) {
      return err;
    }
  }

  /* All done */

  return MPI_SUCCESS;
}


/*
 *	gatherv_inter
 *
 *	Function:	- basic gatherv operation
 *	Accepts:	- same arguments as MPI_Bcast()
 *	Returns:	- MPI_SUCCESS or error code
 */
int mca_coll_basic_gatherv_inter(void *sbuf, int scount,
                                 struct ompi_datatype_t *sdtype,
                                 void *rbuf, int *rcounts, int *disps,
                                 struct ompi_datatype_t *rdtype, int root,
                                 struct ompi_communicator_t *comm)
{
  return OMPI_ERR_NOT_IMPLEMENTED;
}
