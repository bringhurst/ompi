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
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <stdio.h>

#include "orte/util/output.h"
#include "opal/hash_string.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/plm/base/plm_private.h"

/*
 * attempt to create a globally unique name - do a hash
 * of the hostname plus pid
 */
int orte_plm_base_set_hnp_name(void)
{
    uint16_t jobfam;
    uint32_t hash32;
    uint32_t bias;
    
    /* hash the nodename */
    OPAL_HASH_STR(orte_process_info.nodename, hash32);
    
    bias = (uint32_t)orte_process_info.pid;
    
    ORTE_OUTPUT_VERBOSE((5, orte_plm_globals.output,
                         "plm:base:set_hnp_name: initial bias %ld nodename hash %lu",
                         (long)bias, (unsigned long)hash32));

    /* fold in the bias */
    hash32 = hash32 ^ bias;
    
    /* now compress to 16-bits */
    jobfam = (uint16_t)(((0x0000ffff & (0xffff0000 & hash32) >> 16)) ^ (0x0000ffff & hash32));
    
    ORTE_OUTPUT_VERBOSE((5, orte_plm_globals.output,
                         "plm:base:set_hnp_name: final jobfam %lu",
                         (unsigned long)jobfam));
    
    /* set the name */
    ORTE_PROC_MY_NAME->jobid = 0xffff0000 & ((uint32_t)jobfam << 16);
    ORTE_PROC_MY_NAME->vpid = 0;
    
    orte_plm_globals.next_jobid = ORTE_PROC_MY_NAME->jobid + 1;
    
    /* copy it to the HNP field */
    ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_NAME->jobid;
    ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_NAME->vpid;
    
    /* done */
    return ORTE_SUCCESS;
}

/*
 * Create a jobid
 */
int orte_plm_base_create_jobid(orte_jobid_t *jobid)
{
    if (ORTE_JOBID_MAX-1 <  orte_plm_globals.next_jobid) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        *jobid = ORTE_JOBID_INVALID;
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
    
    *jobid =  orte_plm_globals.next_jobid++;
    return ORTE_SUCCESS;
}
