/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008-2011 University of Houston. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "ompi_config.h"
#include "fs_pvfs2.h"
#include "mpi.h"

/*
 * Public string showing the fs pvfs2 component version number
 */
const char *mca_fs_pvfs2_component_version_string =
  "OMPI/MPI pvfs2 FS MCA component version " OMPI_VERSION;

static int pvfs2_register(void);

int mca_fs_pvfs2_priority = 0;
int mca_fs_pvfs2_stripe_size = -1;
int mca_fs_pvfs2_stripe_width = -1;
int mca_fs_pvfs2_IS_INITIALIZED = 0;

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */
mca_fs_base_component_2_0_0_t mca_fs_pvfs2_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    {
        MCA_FS_BASE_VERSION_2_0_0,

        /* Component name and version */
        "pvfs2",
        OMPI_MAJOR_VERSION,
        OMPI_MINOR_VERSION,
        OMPI_RELEASE_VERSION,
        pvfs2_register,
        NULL
    },
    {
        /* This component is checkpointable */
      MCA_BASE_METADATA_PARAM_CHECKPOINT
    },
    mca_fs_pvfs2_component_init_query,      /* get thread level */
    mca_fs_pvfs2_component_file_query,      /* get priority and actions */
    mca_fs_pvfs2_component_file_unquery     /* undo what was done by previous function */
};

static int
pvfs2_register(void)
{
    int param;

    param = mca_base_param_find ("fs", NULL, "pvfs2_stripe_size");
    if (param >= 0)
    {
        mca_base_param_lookup_int (param, &mca_fs_pvfs2_stripe_size);
    }
    param = mca_base_param_find ("fs", NULL, "pvfs2_stripe_width");
    if (param >= 0)
    {
        mca_base_param_lookup_int (param, &mca_fs_pvfs2_stripe_width);
    }

    mca_base_param_reg_int (&mca_fs_pvfs2_component.fsm_version,
                            "priority",
                            "Priority of the pvfs2 fs component",
                            false, false, mca_fs_pvfs2_priority,
                            &mca_fs_pvfs2_priority);
    mca_base_param_reg_int (&mca_fs_pvfs2_component.fsm_version,
                            "stripe_size",
                            "stripe size of a file over pvfs2",
                            false, false, mca_fs_pvfs2_stripe_size,
                            &mca_fs_pvfs2_stripe_size);
    mca_base_param_reg_int (&mca_fs_pvfs2_component.fsm_version,
                            "stripe_width",
                            "stripe width of a file over pvfs2",
                            false, false, mca_fs_pvfs2_stripe_width,
                            &mca_fs_pvfs2_stripe_width);

    return OMPI_SUCCESS;
}
