/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2007 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
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

/** @file 
 * 
 * OMPI Layer Checkpoint/Restart Runtime functions
 *
 */

#include "ompi_config.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>  /* for mkfifo */
#endif  /* HAVE_SYS_STAT_H */

#include "opal/util/output.h"
#include "opal/event/event.h"
#include "opal/mca/crs/crs.h"
#include "opal/mca/crs/base/base.h"
#include "opal/runtime/opal_cr.h"

#include "orte/util/sys_info.h"
#include "orte/util/proc_info.h"
#include "orte/mca/snapc/snapc.h"
#include "orte/mca/snapc/base/base.h"
#include "orte/runtime/runtime.h"

#include "ompi/constants.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/mca/pml/base/base.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "ompi/mca/crcp/crcp.h"
#include "ompi/mca/crcp/base/base.h"
#include "ompi/communicator/communicator.h"
#include "ompi/runtime/ompi_cr.h"

/*************
 * Local functions
 *************/
static int ompi_cr_coord_pre_ckpt(void);
static int ompi_cr_coord_pre_restart(void);
static int ompi_cr_coord_pre_continue(void);

static int ompi_cr_coord_post_ckpt(void);
static int ompi_cr_coord_post_restart(void);
static int ompi_cr_coord_post_continue(void);

/*************
 * Local vars
 *************/
static opal_cr_coord_callback_fn_t  prev_coord_callback = NULL;

int ompi_cr_output = -1;

#define NUM_COLLECTIVES 16

#define SIGNAL(comm, modules, highest_module, msg, ret, func)   \
    do {                                                        \
        bool found = false;                                     \
        int k;                                                  \
        mca_coll_base_module_1_1_0_t *my_module =               \
            comm->c_coll.coll_ ## func ## _module;              \
        if (NULL != my_module) {                                \
            for (k = 0 ; k < highest_module ; ++k) {            \
                if (my_module == modules[k]) found = true;      \
            }                                                   \
            if (!found) {                                       \
                modules[highest_module++] = my_module;          \
                if (NULL != my_module->ft_event) {              \
                    ret = my_module->ft_event(msg);             \
                }                                               \
            }                                                   \
        }                                                       \
    } while (0)


static int
notify_collectives(int msg)
{
    mca_coll_base_module_1_1_0_t *modules[NUM_COLLECTIVES];
    int i, max, ret, highest_module = 0;

    memset(&modules, 0, sizeof(mca_coll_base_module_1_1_0_t*) * NUM_COLLECTIVES);

    max = opal_pointer_array_get_size(&ompi_mpi_communicators);
    for (i = 0 ; i < max ; ++i) {
        ompi_communicator_t *comm =
            (ompi_communicator_t *)opal_pointer_array_get_item(&ompi_mpi_communicators, i);
        if (NULL == comm) continue;

        SIGNAL(comm, modules, highest_module, msg, ret, allgather); 
        SIGNAL(comm, modules, highest_module, msg, ret, allgatherv); 
        SIGNAL(comm, modules, highest_module, msg, ret, allreduce); 
        SIGNAL(comm, modules, highest_module, msg, ret, alltoall); 
        SIGNAL(comm, modules, highest_module, msg, ret, alltoallv); 
        SIGNAL(comm, modules, highest_module, msg, ret, alltoallw); 
        SIGNAL(comm, modules, highest_module, msg, ret, barrier); 
        SIGNAL(comm, modules, highest_module, msg, ret, bcast); 
        SIGNAL(comm, modules, highest_module, msg, ret, exscan); 
        SIGNAL(comm, modules, highest_module, msg, ret, gather); 
        SIGNAL(comm, modules, highest_module, msg, ret, gatherv); 
        SIGNAL(comm, modules, highest_module, msg, ret, reduce); 
        SIGNAL(comm, modules, highest_module, msg, ret, reduce_scatter); 
        SIGNAL(comm, modules, highest_module, msg, ret, scan); 
        SIGNAL(comm, modules, highest_module, msg, ret, scatter); 
        SIGNAL(comm, modules, highest_module, msg, ret, scatterv); 
    }

    return OMPI_SUCCESS;
}


/*
 * CR Init
 */
int ompi_cr_init(void) 
{
    int val;

    /*
     * Register some MCA parameters
     */
    mca_base_param_reg_int_name("ompi_cr", "verbose",
                                "Verbose output for the OMPI Checkpoint/Restart functionality",
                                false, false,
                                0,
                                &val);
    if(0 != val) {
        ompi_cr_output = opal_output_open(NULL);
        opal_output_set_verbosity(ompi_cr_output, val);
    } else {
        ompi_cr_output = opal_cr_output;
    }

    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: init: ompi_cr_init()");
    
    /* Register the OMPI interlevel coordination callback */
    opal_cr_reg_coord_callback(ompi_cr_coord, &prev_coord_callback);
    
    return OMPI_SUCCESS;
}

/*
 * Finalize
 */
int ompi_cr_finalize(void)
{
    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: finalize: ompi_cr_finalize()");
    
    return OMPI_SUCCESS;
}

/*
 * Interlayer coordination callback
 */
int ompi_cr_coord(int state) 
{
    int ret, exit_status = OMPI_SUCCESS;

    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord: ompi_cr_coord(%s)\n",
                        opal_crs_base_state_str((opal_crs_state_type_t)state));

    /*
     * Before calling the previous callback, we have the opportunity to 
     * take action given the state.
     */
    if(OPAL_CRS_CHECKPOINT == state) {
        /* Do Checkpoint Phase work */
        ret = ompi_cr_coord_pre_ckpt();
        if( ret == OMPI_EXISTS) {
            return ret;
        }
        else if( ret != OMPI_SUCCESS) {
            return ret;
        }
    }
    else if (OPAL_CRS_CONTINUE == state ) {
        /* Do Continue Phase work */
        ompi_cr_coord_pre_continue();
    }
    else if (OPAL_CRS_RESTART == state ) {
        /* Do Restart Phase work */
        ompi_cr_coord_pre_restart();
    }
    else if (OPAL_CRS_TERM == state ) {
        /* Do Continue Phase work in prep to terminate the application */
    }
    else {
        /* We must have been in an error state from the checkpoint
         * recreate everything, as in the Continue Phase
         */
    }

    /*
     * Call the previous callback, which should be ORTE [which will handle OPAL]
     */
    if(OMPI_SUCCESS != (ret = prev_coord_callback(state)) ) {
        exit_status = ret;
        goto cleanup;
    }
    
    
    /*
     * After calling the previous callback, we have the opportunity to 
     * take action given the state to tidy up.
     */
    if(OPAL_CRS_CHECKPOINT == state) {
        /* Do Checkpoint Phase work */
        ompi_cr_coord_post_ckpt();
    }
    else if (OPAL_CRS_CONTINUE == state ) {
        /* Do Continue Phase work */
        ompi_cr_coord_post_continue();
    }
    else if (OPAL_CRS_RESTART == state ) {
        /* Do Restart Phase work */
        ompi_cr_coord_post_restart();
    }
    else if (OPAL_CRS_TERM == state ) {
        /* Do Continue Phase work in prep to terminate the application */
    }
    else {
        /* We must have been in an error state from the checkpoint
         * recreate everything, as in the Continue Phase
         */
    }

 cleanup:
    return exit_status;
}

/*************
 * Pre Lower Layer
 *************/
static int ompi_cr_coord_pre_ckpt(void) {
    int ret, exit_status = OMPI_SUCCESS;

    /*
     * All the checkpoint heavey lifting in here...
     */
    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_pre_ckpt: ompi_cr_coord_pre_ckpt()\n");

    /*
     * Notify Collectives
     * - Need to do this on a per communicator basis
     *   Traverse all communicators...
     */
    if (OMPI_SUCCESS != (ret = notify_collectives(OPAL_CR_CHECKPOINT))) {
        goto cleanup;
    }
    
    /*
     * Notify PML
     *  - Will notify BML and BTL's
     */
    if( ORTE_SUCCESS != (ret = mca_pml.pml_ft_event(OPAL_CRS_CHECKPOINT))) {
        exit_status = ret;
        goto cleanup;
    }
    
 cleanup:

    return exit_status;
}

static int ompi_cr_coord_pre_restart(void) {
    /*
     * Can not really do much until ORTE is up and running,
     * so defer action until the post_restart function.
     */
    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_pre_restart: ompi_cr_coord_pre_restart()");
    
    return OMPI_SUCCESS;
}
    
static int ompi_cr_coord_pre_continue(void) {
    /*
     * Can not really do much until ORTE is up and running,
     * so defer action until the post_continue function.
     */
    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_pre_continue: ompi_cr_coord_pre_continue()");

    return OMPI_SUCCESS;
}

/*************
 * Post Lower Layer
 *************/
static int ompi_cr_coord_post_ckpt(void) {
    /*
     * Now that ORTE/OPAL are shutdown, we really can't do much
     * so assume pre_ckpt took care of everything.
     */
    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_post_ckpt: ompi_cr_coord_post_ckpt()");

    return OMPI_SUCCESS;
}

static int ompi_cr_coord_post_restart(void) {
    int ret, exit_status = OMPI_SUCCESS;

    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_post_restart: ompi_cr_coord_post_restart()");

    /*
     * Notify PML
     *  - Will notify BML and BTL's
     */
    if( ORTE_SUCCESS != (ret = mca_pml.pml_ft_event(OPAL_CRS_RESTART))) {
        exit_status = ret;
        goto cleanup;
    }

    /*
     * Notify Collectives
     * - Need to do this on a per communicator basis
     *   Traverse all communicators...
     */
    if (OMPI_SUCCESS != (ret = notify_collectives(OPAL_CRS_RESTART))) {
        goto cleanup;
    }
    
 cleanup:

    return exit_status;
}

static int ompi_cr_coord_post_continue(void) {
    int ret, exit_status = OMPI_SUCCESS;

    opal_output_verbose(10, ompi_cr_output,
                        "ompi_cr: coord_post_continue: ompi_cr_coord_post_continue()");

    /*
     * Notify PML
     *  - Will notify BML and BTL's
     */
    if( ORTE_SUCCESS != (ret = mca_pml.pml_ft_event(OPAL_CRS_CONTINUE))) {
        exit_status = ret;
        goto cleanup;
    }

    /*
     * Notify Collectives
     * - Need to do this on a per communicator basis
     *   Traverse all communicators...
     */
    if (OMPI_SUCCESS != (ret = notify_collectives(OPAL_CRS_CONTINUE))) {
        goto cleanup;
    }

 cleanup:

    return exit_status;
}
