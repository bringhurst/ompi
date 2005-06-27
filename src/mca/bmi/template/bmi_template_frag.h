/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
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

#ifndef MCA_BMI_TEMPLATE_FRAG_H
#define MCA_BMI_TEMPLATE_FRAG_H


#define MCA_BMI_TEMPLATE_FRAG_ALIGN (8)
#include "ompi_config.h"
#include "bmi_template.h" 

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
OMPI_DECLSPEC OBJ_CLASS_DECLARATION(mca_bmi_template_frag_t);



/**
 * TEMPLATE send fratemplateent derived type.
 */
struct mca_bmi_template_frag_t {
    mca_bmi_base_descriptor_t base; 
    mca_bmi_base_segment_t segment; 
    struct mca_bmi_base_endpoint_t *endpoint; 
    mca_bmi_base_header_t *hdr;
    size_t size; 
#if MCA_BMI_HAS_MPOOL
    struct mca_mpool_base_registration_t* registration;
#endif
}; 
typedef struct mca_bmi_template_frag_t mca_bmi_template_frag_t; 
OBJ_CLASS_DECLARATION(mca_bmi_template_frag_t); 


typedef struct mca_bmi_template_frag_t mca_bmi_template_frag_eager_t; 
    
OBJ_CLASS_DECLARATION(mca_bmi_template_frag_eager_t); 

typedef struct mca_bmi_template_frag_t mca_bmi_template_frag_max_t; 
    
OBJ_CLASS_DECLARATION(mca_bmi_template_frag_max_t); 

typedef struct mca_bmi_template_frag_t mca_bmi_template_frag_user_t; 
    
OBJ_CLASS_DECLARATION(mca_bmi_template_frag_user_t); 


/*
 * Macros to allocate/return descriptors from module specific
 * free list(s).
 */

#define MCA_BMI_TEMPLATE_FRAG_ALLOC_EAGER(bmi, frag, rc)           \
{                                                                  \
                                                                   \
    ompi_list_item_t *item;                                        \
    OMPI_FREE_LIST_WAIT(&((mca_bmi_template_module_t*)bmi)->template_frag_eager, item, rc); \
    frag = (mca_bmi_template_frag_t*) item;                        \
}

#define MCA_BMI_TEMPLATE_FRAG_RETURN_EAGER(bmi, frag)              \
{                                                                  \
    OMPI_FREE_LIST_RETURN(&((mca_bmi_template_module_t*)bmi)->template_frag_eager, \
        (ompi_list_item_t*)(frag));                                \
}

#define MCA_BMI_TEMPLATE_FRAG_ALLOC_MAX(bmi, frag, rc)             \
{                                                                  \
                                                                   \
    ompi_list_item_t *item;                                        \
    OMPI_FREE_LIST_WAIT(&((mca_bmi_template_module_t*)bmi)->template_frag_max, item, rc); \
    frag = (mca_bmi_template_frag_t*) item;                        \
}

#define MCA_BMI_TEMPLATE_FRAG_RETURN_MAX(bmi, frag)                \
{                                                                  \
    OMPI_FREE_LIST_RETURN(&((mca_bmi_template_module_t*)bmi)->template_frag_max, \
        (ompi_list_item_t*)(frag));                                \
}


#define MCA_BMI_TEMPLATE_FRAG_ALLOC_USER(bmi, frag, rc)            \
{                                                                  \
    ompi_list_item_t *item;                                        \
    OMPI_FREE_LIST_WAIT(&((mca_bmi_template_module_t*)bmi)->template_frag_user, item, rc); \
    frag = (mca_bmi_template_frag_t*) item;                        \
}

#define MCA_BMI_TEMPLATE_FRAG_RETURN_USER(bmi, frag)               \
{                                                                  \
    OMPI_FREE_LIST_RETURN(&((mca_bmi_template_module_t*)bmi)->template_frag_user, \
        (ompi_list_item_t*)(frag)); \
}



#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif
