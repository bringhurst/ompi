/*
 * Copyright (c) 2011      Cisco Systems, Inc.  All rights reserved. 
 *
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

#include "opal_config.h"
#include "opal/constants.h"

#include "opal/mca/hwloc/hwloc.h"
#include "hwloc122ompi.h"

/*
 * Public string showing the sysinfo ompi_linux component version number
 */
const char *opal_hwloc_hwloc122ompi_component_version_string =
    "OPAL hwloc122ompi hwloc MCA component version " OPAL_VERSION;

/*
 * Local function
 */
static int hwloc122ompi_open(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

const opal_hwloc_component_t mca_hwloc_hwloc122ompi_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    {
        OPAL_HWLOC_BASE_VERSION_2_0_0,

        /* Component name and version */
        "hwloc122ompi",
        OPAL_MAJOR_VERSION,
        OPAL_MINOR_VERSION,
        OPAL_RELEASE_VERSION,

        /* Component open and close functions */
        hwloc122ompi_open,
        NULL
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
};


static int hwloc122ompi_open(void)
{    
    return OPAL_SUCCESS;
}

