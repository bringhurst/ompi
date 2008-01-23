/*
 * Copyright (c) 2004-2007 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef PML_V_H_HAS_BEEN_INCLUDED
#define PML_V_H_HAS_BEEN_INCLUDED

#include "ompi_config.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/request/request.h"

BEGIN_C_DECLS

struct mca_pml_v_t {
    ompi_request_fns_t                  host_request_fns;
    size_t                              host_pml_req_recv_size;
    size_t                              host_pml_req_send_size;
    mca_pml_base_component_t            host_pml_component;
    mca_pml_base_module_t               host_pml;
};
typedef struct mca_pml_v_t mca_pml_v_t;

OMPI_DECLSPEC extern mca_pml_v_t mca_pml_v;
OMPI_DECLSPEC extern mca_pml_base_component_1_0_0_t mca_pml_v_component;

END_C_DECLS

#include "pml_v_output.h"

#endif  /* PML_V_H_HAS_BEEN_INCLUDED */
