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
#include "coll_basic.h"

#include <stdio.h>

#include "mpi.h"
#include "include/constants.h"
#include "op/op.h"
#include "datatype/datatype.h"
#include "mca/pml/pml.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/coll_tags.h"
#include "coll_basic.h"


/*
 *	exscan_intra
 *
 *	Function:	- basic exscan operation
 *	Accepts:	- same arguments as MPI_Exccan()
 *	Returns:	- MPI_SUCCESS or error code
 */
int mca_coll_basic_exscan_intra(void *sbuf, void *rbuf, int count,
                                struct ompi_datatype_t *dtype, 
                                struct ompi_op_t *op, 
                                struct ompi_communicator_t *comm)
{
  int size;
  int rank;
  int err;
  long true_lb, true_extent, lb, extent;
  char *free_buffer = NULL;
  char *reduce_buffer = NULL;
  char *source;
  MPI_Request req = MPI_REQUEST_NULL;

  /* Initialize. */

  rank = ompi_comm_rank(comm);
  size = ompi_comm_size(comm);

  /* If we're rank 0, then we send our sbuf to the next rank */

  if (0 == rank) {
    return mca_pml.pml_send(sbuf, count, dtype, rank + 1, 
                            MCA_COLL_BASE_TAG_EXSCAN,
                            MCA_PML_BASE_SEND_STANDARD, comm);
  }

  /* If we're the last rank, then just receive the result from the
     prior rank */

  else if ((size - 1) == rank) {
    return mca_pml.pml_recv(rbuf, count, dtype, rank - 1, 
                            MCA_COLL_BASE_TAG_EXSCAN, comm, MPI_STATUS_IGNORE);
  }

  /* Otherwise, get the result from the prior rank, combine it with my
     data, and send it to the next rank */
  
  /* Start the receive for the prior rank's answer */
      
  err = mca_pml.pml_irecv(rbuf, count, dtype, rank - 1,
                          MCA_COLL_BASE_TAG_EXSCAN, comm, &req);
  if (MPI_SUCCESS != err) {
    goto error;
  }

  /* Get a temporary buffer to perform the reduction into.  Rationale
     for malloc'ing this size is provided in coll_basic_reduce.c. */
  
  ompi_ddt_get_extent(dtype, &lb, &extent);
  ompi_ddt_get_true_extent(dtype, &true_lb, &true_extent);
    
  free_buffer = malloc(true_extent + (count - 1) * extent);
  if (NULL == free_buffer) {
    return OMPI_ERR_OUT_OF_RESOURCE;
  }
  reduce_buffer = free_buffer - lb;

  if (ompi_op_is_commute(op)) {
      
    /* If we're commutative, we can copy my sbuf into the reduction
       buffer before the receive completes */
    
    err = ompi_ddt_sndrcv(sbuf, count, dtype, reduce_buffer, count, dtype,
                          MCA_COLL_BASE_TAG_EXSCAN, comm);
    if (MPI_SUCCESS != err) {
      goto error;
    }

    /* Now setup the reduction */

    source = rbuf;
    
    /* Finally, wait for the receive to complete (so that we can do
       the reduction).  */

    err = ompi_request_wait(&req, MPI_STATUS_IGNORE);
    if (MPI_SUCCESS != err) {
      goto error;
    }
  } else {

    /* Setup the reduction */

    source = sbuf;

    /* If we're not commutative, we have to wait for the receive to
       complete and then copy it into the reduce buffer */

    err = ompi_request_wait(&req, MPI_STATUS_IGNORE);
    if (MPI_SUCCESS != err) {
      goto error;
    }

    err = ompi_ddt_sndrcv(rbuf, count, dtype, reduce_buffer, count, dtype,
                          MCA_COLL_BASE_TAG_EXSCAN, comm);
    if (MPI_SUCCESS != err) {
      goto error;
    }
  }

  /* Now reduce the received answer with my source into the answer
     that we send off to the next rank */

  ompi_op_reduce(op, source, reduce_buffer, count, dtype);

  /* Send my result off to the next rank */

  err = mca_pml.pml_send(reduce_buffer, count, dtype, rank + 1, 
                         MCA_COLL_BASE_TAG_EXSCAN, 
                         MCA_PML_BASE_SEND_STANDARD, comm);

  /* Error */

error:
  free(free_buffer);
  if (MPI_REQUEST_NULL != req) {
    ompi_request_cancel(req);
    ompi_request_wait(&req, MPI_STATUS_IGNORE);
  }

  /* All done */

  return err;
}


/*
 *	exscan_inter
 *
 *	Function:	- basic exscan operation
 *	Accepts:	- same arguments as MPI_Exccan()
 *	Returns:	- MPI_SUCCESS or error code
 */
int mca_coll_basic_exscan_inter(void *sbuf, void *rbuf, int count,
                                struct ompi_datatype_t *dtype, 
                                struct ompi_op_t *op, 
                                struct ompi_communicator_t *comm)
{
  return OMPI_ERR_NOT_IMPLEMENTED;
}
