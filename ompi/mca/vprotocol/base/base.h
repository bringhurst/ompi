/*
 * Copyright (c) 2004-2007 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef __INCLUDE_VPROTOCOL_BASE_H_
#define __INCLUDE_VPROTOCOL_BASE_H_

#include "ompi_config.h"
#include "ompi/constants.h"
#include "opal/mca/mca.h"
#include "ompi/mca/vprotocol/vprotocol.h"

BEGIN_C_DECLS

struct mca_pml_v_t {
    int                                 output;
    size_t                              host_pml_req_recv_size;
    size_t                              host_pml_req_send_size;
    mca_pml_base_component_t            host_pml_component;
    mca_pml_base_module_t               host_pml;
    ompi_request_fns_t                  host_request_fns;
};
typedef struct mca_pml_v_t mca_pml_v_t;

OMPI_DECLSPEC extern mca_pml_v_t mca_pml_v;

OMPI_DECLSPEC int mca_vprotocol_base_open(char *vprotocol_include_list);
OMPI_DECLSPEC int mca_vprotocol_base_select(bool enable_progress_threads, 
                                            bool enable_mpi_threads);
OMPI_DECLSPEC int mca_vprotocol_base_parasite(void);
OMPI_DECLSPEC int mca_vprotocol_base_close(void);
    
OMPI_DECLSPEC extern char *mca_vprotocol_base_include_list;
OMPI_DECLSPEC extern opal_list_t mca_vprotocol_base_components_available;
OMPI_DECLSPEC extern mca_vprotocol_base_component_t mca_vprotocol_component;
OMPI_DECLSPEC extern mca_vprotocol_base_module_t mca_vprotocol;
    

/* Macro for use in components that are of type vprotocol
 */
#define MCA_VPROTOCOL_BASE_VERSION_2_0_0 \
    MCA_BASE_VERSION_2_0_0, \
    "vprotocol", 2, 0, 0
        
/* Macro to mark an invalid component version (0.0.0). Any component showing 
 * that version number will be ignored. 
 */
#define MCA_VPROTOCOL_BASE_VERSION_0_0_0 \
    /* vprotocol v0.0 is chained to MCA v2.0 */ \
    MCA_BASE_VERSION_2_0_0, \
    /* vprotocol v0.0 */ \
    "vprotocol", 0, 0, 0
        
#define mca_vprotocol_base_selected() (                                        \
    0 != mca_vprotocol_component.pmlm_version.mca_type_major_version           \
)
        
END_C_DECLS

#endif /* __INCLUDE_VPROTOCOL_BASE_H_ */
