/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal_config.h"

#include "opal/constants.h"
#include "opal/mca/compress/compress.h"
#include "opal/mca/compress/base/base.h"
#include "compress_bzip.h"

/*
 * Public string for version number
 */
const char *opal_compress_bzip_component_version_string = 
"OPAL COMPRESS bzip MCA component version " OPAL_VERSION;

/*
 * Local functionality
 */
static int compress_bzip_open(void);
static int compress_bzip_close(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointer to our public functions in it
 */
opal_compress_bzip_component_t mca_compress_bzip_component = {
    /* First do the base component stuff */
    {
        /* Handle the general mca_component_t struct containing 
         *  meta information about the component itbzip
         */
        {
            OPAL_COMPRESS_BASE_VERSION_2_0_0,

            /* Component name and version */
            "bzip",
            OPAL_MAJOR_VERSION,
            OPAL_MINOR_VERSION,
            OPAL_RELEASE_VERSION,
            
            /* Component open and close functions */
            compress_bzip_open,
            compress_bzip_close,
            opal_compress_bzip_component_query
        },
        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },
        
        /* Verbosity level */
        0,
        /* opal_output handler */
        -1,
        /* Default priority */
        10
    }
};

/*
 * Bzip module
 */
static opal_compress_base_module_t loc_module = {
    /** Initialization Function */
    opal_compress_bzip_module_init,
    /** Finalization Function */
    opal_compress_bzip_module_finalize,

    /** Compress Function */
    opal_compress_bzip_compress,
    opal_compress_bzip_compress_nb,

    /** Decompress Function */
    opal_compress_bzip_decompress,
    opal_compress_bzip_decompress_nb
};

static int compress_bzip_open(void) 
{
    mca_base_param_reg_int(&mca_compress_bzip_component.super.base_version,
                           "priority",
                           "Priority of the COMPRESS bzip component",
                           false, false,
                           mca_compress_bzip_component.super.priority, 
                           &mca_compress_bzip_component.super.priority);

    mca_base_param_reg_int(&mca_compress_bzip_component.super.base_version,
                           "verbose",
                           "Verbose level for the COMPRESS bzip component",
                           false, false,
                           mca_compress_bzip_component.super.verbose,
                           &mca_compress_bzip_component.super.verbose);
    /* If there is a custom verbose level for this component than use it
     * otherwise take our parents level and output channel
     */
    if ( 0 != mca_compress_bzip_component.super.verbose) {
        mca_compress_bzip_component.super.output_handle = opal_output_open(NULL);
        opal_output_set_verbosity(mca_compress_bzip_component.super.output_handle, 
                                  mca_compress_bzip_component.super.verbose);
    } else {
        mca_compress_bzip_component.super.output_handle = opal_compress_base_output;
    }

    /*
     * Debug output
     */
    opal_output_verbose(10, mca_compress_bzip_component.super.output_handle,
                        "compress:bzip: open()");
    opal_output_verbose(20, mca_compress_bzip_component.super.output_handle,
                        "compress:bzip: open: priority = %d", 
                        mca_compress_bzip_component.super.priority);
    opal_output_verbose(20, mca_compress_bzip_component.super.output_handle,
                        "compress:bzip: open: verbosity = %d", 
                        mca_compress_bzip_component.super.verbose);
    return OPAL_SUCCESS;
}

static int compress_bzip_close(void)
{
    return OPAL_SUCCESS;
}

int opal_compress_bzip_component_query(mca_base_module_t **module, int *priority)
{
    *module   = (mca_base_module_t *)&loc_module;
    *priority = mca_compress_bzip_component.super.priority;

    return OPAL_SUCCESS;
}

