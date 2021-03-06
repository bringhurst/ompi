/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University.
 *                         All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal_config.h"

#ifdef HAVE_UNISTD_H
#include "unistd.h"
#endif

#include "opal/include/opal/constants.h"
#include "opal/util/output.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/mca/compress/compress.h"
#include "opal/mca/compress/base/base.h"

int opal_compress_base_select(void)
{
    int ret, exit_status = OPAL_SUCCESS;
    opal_compress_base_component_t *best_component = NULL;
    opal_compress_base_module_t *best_module = NULL;

    /* Compression currently only used with C/R */
    if( !opal_cr_is_enabled ) {
        opal_output_verbose(10, opal_compress_base_output,
                            "compress:open: FT is not enabled, skipping!");
        return OPAL_SUCCESS;
    }

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("compress", opal_compress_base_output,
                                        &opal_compress_base_components_available,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        exit_status = OPAL_ERROR;
        goto cleanup;
    }

    /* Save the winner */
    opal_compress_base_selected_component = *best_component;
    opal_compress = *best_module;

    /* Initialize the winner */
    if (NULL != best_module) {
        if (OPAL_SUCCESS != (ret = opal_compress.init()) ) {
            exit_status = ret;
            goto cleanup;
        }
    }

 cleanup:
    return exit_status;
}
