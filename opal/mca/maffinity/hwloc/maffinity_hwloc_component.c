/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2011 Cisco Systems, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal_config.h"

#include <string.h>

#include "opal/constants.h"
#include "opal/util/show_help.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/mca/common/hwloc/common_hwloc.h"
#include "opal/mca/maffinity/maffinity.h"
#include "maffinity_hwloc.h"

/*
 * Public string showing the maffinity ompi_hwloc component version number
 */
const char *opal_maffinity_hwloc_component_version_string =
    "OPAL hwloc maffinity MCA component version " OPAL_VERSION;

/*
 * Local function
 */
static int hwloc_open(void);
static int hwloc_close(void);
static int hwloc_register(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

opal_maffinity_hwloc_component_2_0_0_t mca_maffinity_hwloc_component = {
    {
        /* First, the mca_component_t struct containing meta information
           about the component itself */
        {
            OPAL_MAFFINITY_BASE_VERSION_2_0_0,
            
            /* Component name and version */
            "hwloc",
            OPAL_MAJOR_VERSION,
            OPAL_MINOR_VERSION,
            OPAL_RELEASE_VERSION,
            
            /* Component open and close functions */
            hwloc_open,
            hwloc_close,
            opal_maffinity_hwloc_component_query,
            hwloc_register,
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        }
    },

    /* Priority */
    40,
    /* Default binding policy */
    HWLOC_MEMBIND_STRICT,

    /* NULL fill the rest of the component data */
};


static int hwloc_register(void)
{
    int i;
    char *val, *mca_policy;

    /* Call the registration function of common hwloc */
    opal_common_hwloc_register();

    i = mca_base_param_find("common", NULL, "hwloc_version");
    if (OPAL_ERROR != i) {
        mca_base_param_reg_syn(i, 
                               &mca_maffinity_hwloc_component.base.base_version,
                               "hwloc_version", false);
    }

    mca_base_param_reg_int(&mca_maffinity_hwloc_component.base.base_version,
                           "priority",
                           "Priority of the hwloc maffinity component",
                           false, false, 40, 
                           &mca_maffinity_hwloc_component.priority);

    /* Default memory binding policy */
    val = (HWLOC_MEMBIND_STRICT == mca_maffinity_hwloc_component.bind_policy ?
           "strict" : "loose");
    mca_base_param_reg_string(&mca_maffinity_hwloc_component.base.base_version,
                              "policy", 
                              "Binding policy that determines what happens if memory is unavailable on the local NUMA node.  A value of \"strict\" means that the memory allocation will fail; a value of \"loose\" means that the memory allocation will spill over to another NUMA node.",
                              false, false, val, &mca_policy);

    if (strcasecmp(mca_policy, "loose") == 0) {
        mca_maffinity_hwloc_component.bind_policy = 0;
    } else if (strcasecmp(mca_policy, "strict") == 0) {
        mca_maffinity_hwloc_component.bind_policy = HWLOC_MEMBIND_STRICT;
    } else {
        opal_show_help("help-opal-maffinity-hwloc.txt", "invalid policy",
                       true, mca_policy, getpid());
        mca_maffinity_hwloc_component.bind_policy = HWLOC_MEMBIND_STRICT;
        return OPAL_ERR_BAD_PARAM;
    }

    return OPAL_SUCCESS;
}


static int hwloc_open(void)
{
    /* Initialize hwloc */
    if (0 != hwloc_topology_init(&(mca_maffinity_hwloc_component.topology)) ||
        0 != hwloc_topology_load(mca_maffinity_hwloc_component.topology)) {
        mca_maffinity_hwloc_component.topology = NULL;
        return OPAL_ERR_NOT_AVAILABLE;
    }
    mca_maffinity_hwloc_component.topology_need_destroy = true;

    return OPAL_SUCCESS;
}

static int hwloc_close(void)
{
    /* If we set up hwloc, tear it down */
    if (mca_maffinity_hwloc_component.topology_need_destroy) {
        hwloc_topology_destroy(mca_maffinity_hwloc_component.topology);
        mca_maffinity_hwloc_component.topology_need_destroy = false;
    }

    return OPAL_SUCCESS;
}
