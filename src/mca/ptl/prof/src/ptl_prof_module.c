/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 * $HEADER$
 */
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "constants.h"
#include "event/event.h"
#include "util/if.h"
#include "util/argv.h"
#include "util/output.h"
#include "mca/pml/pml.h"
#include "mca/ptl/ptl.h"
#include "mca/pml/base/pml_base_sendreq.h"
#include "mca/ptl/base/ptl_base_recvfrag.h"
#include "mca/base/mca_base_param.h"
#include "mca/base/mca_base_module_exchange.h"
#include "ptl_prof.h"

/**
 * This is the moment to grab all existing modules, and then replace their
 * functions with my own. In same time the ptl_stack will be initialized
 * with the pointer to a ptl automatically generate, which will contain
 * the correct pointers.
 */
static int ptl_prof_module_control_fn( int param, void* value, size_t size )
{
    /* check in mca_ptl_base_modules_initialized */
    return 0;
}

/* We have to create at least one PTL, just to allow the PML to call the control
 * function associated with this PTL.
 */
extern mca_ptl_prof_t mca_ptl_prof;
static struct mca_ptl_t** ptl_prof_module_init_fn( int *num_ptls,
                                                   bool *allow_multi_user_threads,
                                                   bool *have_hidden_threads )
{
    *num_ptls = 1;
    *allow_multi_user_threads = true;
    *have_hidden_threads = false;
    return (struct mca_ptl_t**)&mca_ptl_prof;
}

static int mca_ptl_prof_module_open_fn( void )
{
    return OMPI_SUCCESS;
}

static int mca_ptl_prof_module_close_fn( void )
{
    return OMPI_SUCCESS;
}

mca_ptl_prof_module_1_0_0_t mca_ptl_prof_module = {
    {
        /* First, the mca_base_module_t struct containing meta information
           about the module itself */

        {
            /* Indicate that we are a pml v1.0.0 module (which also implies a
               specific MCA version) */

            MCA_PTL_BASE_VERSION_1_0_0,

            "prof", /* MCA module name */
            1,  /* MCA module major version */
            0,  /* MCA module minor version */
            0,  /* MCA module release version */
            mca_ptl_prof_module_open_fn,  /* module open */
            mca_ptl_prof_module_close_fn  /* module close */
        },

        /* Next the MCA v1.0.0 module meta data */

        {
            /* Whether the module is checkpointable or not */
            true
        },

        ptl_prof_module_init_fn,
        ptl_prof_module_control_fn,
        NULL,
    }
};
