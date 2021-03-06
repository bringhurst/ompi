/*
 * Copyright (c) 2007      Los Alamos National Security, LLC.
 *                         All rights reserved. 
 * Copyright (c) 2009-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <stddef.h>

#include "opal/threads/condition.h"
#include "opal/dss/dss.h"
#include "opal/class/opal_hash_table.h"
#include "opal/class/opal_bitmap.h"
#include "opal/util/bit_ops.h"
#include "opal/util/output.h"
#include "opal/util/opal_sos.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/ess/ess.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/runtime.h"
#include "orte/runtime/data_type_support/orte_dt_support.h"

#include "orte/mca/rml/base/rml_contact.h"

#include "orte/mca/routed/base/base.h"
#include "routed_cm.h"

static int init(void);
static int finalize(void);
static int delete_route(orte_process_name_t *proc);
static int update_route(orte_process_name_t *target,
                        orte_process_name_t *route);
static orte_process_name_t get_route(orte_process_name_t *target);
static int init_routes(orte_jobid_t job, opal_buffer_t *ndat);
static int route_lost(const orte_process_name_t *route);
static bool route_is_defined(const orte_process_name_t *target);
static int update_routing_tree(orte_jobid_t jobid);
static orte_vpid_t get_routing_tree(opal_list_t *children);
static int get_wireup_info(opal_buffer_t *buf);
static int set_lifeline(orte_process_name_t *proc);
static size_t num_routes(void);

#if OPAL_ENABLE_FT_CR == 1
static int cm_ft_event(int state);
#endif

orte_routed_module_t orte_routed_cm_module = {
    init,
    finalize,
    delete_route,
    update_route,
    get_route,
    init_routes,
    route_lost,
    route_is_defined,
    set_lifeline,
    update_routing_tree,
    get_routing_tree,
    get_wireup_info,
    num_routes,
#if OPAL_ENABLE_FT_CR == 1
    cm_ft_event
#else
    NULL
#endif
};

/* local globals */
static opal_condition_t         cond;
static opal_mutex_t             lock;
static orte_process_name_t      *lifeline=NULL;
static orte_process_name_t      local_lifeline;
static bool                     ack_recvd;


static int init(void)
{
    /* setup the global condition and lock */
    OBJ_CONSTRUCT(&cond, opal_condition_t);
    OBJ_CONSTRUCT(&lock, opal_mutex_t);

    lifeline = NULL;
    
    return ORTE_SUCCESS;
}

static int finalize(void)
{
    int rc;
    
    /* if I am a tool without a daemon, just cleanout
     * the basics and leave
     */
    if (ORTE_PROC_IS_TOOL && NULL == orte_process_info.my_daemon_uri) {
        goto cleanup;
    }
    
    /* if I am an application process, indicate that I am
     * truly finalizing prior to departure
     */
    if (ORTE_PROC_IS_APP) {
        if (ORTE_SUCCESS != (rc = orte_routed_base_register_sync(false))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    
cleanup:
    /* destruct the global condition and lock */
    OBJ_DESTRUCT(&cond);
    OBJ_DESTRUCT(&lock);

    lifeline = NULL;

    return ORTE_SUCCESS;
}

static int delete_route(orte_process_name_t *proc)
{
    int i;
    orte_routed_jobfam_t *jfam;
    uint16_t jfamily;
    
#if ORTE_ENABLE_EPOCH
    if (proc->jobid == ORTE_JOBID_INVALID ||
        proc->vpid == ORTE_VPID_INVALID ||
        0 == ORTE_EPOCH_CMP(target->epoch,ORTE_EPOCH_INVALID)) {
#else
    if (proc->jobid == ORTE_JOBID_INVALID ||
        proc->vpid == ORTE_VPID_INVALID) {
#endif
        return ORTE_ERR_BAD_PARAM;
    }

    /* if I am anything other than the HNP, I don't have any routes
     * so there is nothing for me to do
     */
    if (!ORTE_PROC_IS_HNP) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                         "%s routed_cm_delete_route for %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc)));
    
    
    /* if this is from a different job family, then I need to
     * look it up appropriately
     */
    if (ORTE_JOB_FAMILY(proc->jobid) != ORTE_JOB_FAMILY(ORTE_PROC_MY_NAME->jobid)) {
        
        /* see if this job family is present */
        jfamily = ORTE_JOB_FAMILY(proc->jobid);
        for (i=0; i < orte_routed_jobfams.size; i++) {
            if (NULL == (jfam = (orte_routed_jobfam_t*)opal_pointer_array_get_item(&orte_routed_jobfams, i))) {
                continue;
            }
            if (jfam->job_family == jfamily) {
                OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                                     "%s routed_binomial: deleting route to %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_JOB_FAMILY_PRINT(proc->jobid)));
                opal_pointer_array_set_item(&orte_routed_jobfams, i, NULL);
                OBJ_RELEASE(jfam);
                return ORTE_SUCCESS;
            }
        }
        /* not present - nothing to do */
        return ORTE_SUCCESS;
    }
    
    /* THIS CAME FROM OUR OWN JOB FAMILY...there is nothing
     * to do here. The routes will be redefined when we update
     * the routing tree
     */
    
    /* remove any entries in the RML for this process */
    orte_rml.purge(proc);
    return ORTE_SUCCESS;
}

static int update_route(orte_process_name_t *target,
                        orte_process_name_t *route)
{ 
    int i;
    orte_routed_jobfam_t *jfam;
    uint16_t jfamily;
    
#if ORTE_ENABLE_EPOCH
    if (target->jobid == ORTE_JOBID_INVALID ||
        target->vpid == ORTE_VPID_INVALID ||
        0 == ORTE_EPOCH_CMP(target->epoch,ORTE_EPOCH_INVALID)) {
#else
    if (target->jobid == ORTE_JOBID_INVALID ||
        target->vpid == ORTE_VPID_INVALID) {
#endif
        return ORTE_ERR_BAD_PARAM;
    }

    /* if I am an application process, we don't update the route since
     * we automatically route everything through the local daemon
     */
    if (ORTE_PROC_IS_APP) {
        return ORTE_SUCCESS;
    }

    /* if I am a daemon, there is nothing to do as all comm is routed
     * through the HNP to minimize sockets
     */
    if (ORTE_PROC_IS_DAEMON) {
        return ORTE_SUCCESS;
    }
    
    /* if I am a tool, we don't update the route since
     * we automatically route everything direct
     */
    if (ORTE_PROC_IS_TOOL) {
        return ORTE_SUCCESS;
    }
    
    OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                         "%s routed_cm_update: %s --> %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(target), 
                         ORTE_NAME_PRINT(route)));


    /* if this is from a different job family, then I need to
     * track how to send messages to it
     */
    if (ORTE_JOB_FAMILY(target->jobid) != ORTE_JOB_FAMILY(ORTE_PROC_MY_NAME->jobid)) {
        
        OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                             "%s routed_cm_update: diff job family routing job %s --> %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOBID_PRINT(target->jobid), 
                             ORTE_NAME_PRINT(route)));
        
        /* see if this target is already present */
        jfamily = ORTE_JOB_FAMILY(target->jobid);
        for (i=0; i < orte_routed_jobfams.size; i++) {
            if (NULL == (jfam = (orte_routed_jobfam_t*)opal_pointer_array_get_item(&orte_routed_jobfams, i))) {
                continue;
            }
            if (jfam->job_family == jfamily) {
                OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                                     "%s routed_cm: updating route to %s via %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_JOB_FAMILY_PRINT(target->jobid),
                                     ORTE_NAME_PRINT(route)));
                jfam->route.jobid = route->jobid;
                jfam->route.vpid = route->vpid;
                ORTE_EPOCH_SET(jfam->route.epoch,orte_ess.proc_get_epoch(&jfam->route));
                
                return ORTE_SUCCESS;
            }
        }
        
        /* not there, so add the route FOR THE JOB FAMILY*/
        OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                             "%s routed_cm: adding route to %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOB_FAMILY_PRINT(target->jobid)));
        jfam = OBJ_NEW(orte_routed_jobfam_t);
        jfam->job_family = jfamily;
        jfam->route.jobid = route->jobid;
        jfam->route.vpid = route->vpid;
        ORTE_EPOCH_SET(jfam->route.epoch,orte_ess.proc_get_epoch(&jfam->route));
        
        opal_pointer_array_add(&orte_routed_jobfams, jfam);
        return ORTE_SUCCESS;
    }
    
    /* THIS CAME FROM OUR OWN JOB FAMILY... */
    
    opal_output(0, "%s CALL TO UPDATE ROUTE FOR OWN JOB FAMILY", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    return ORTE_ERR_NOT_SUPPORTED;
}


static orte_process_name_t get_route(orte_process_name_t *target)
{
    orte_process_name_t *ret = target, daemon;
    int32_t i;
    orte_job_t *jdata;
    orte_proc_t *proc;
    orte_routed_jobfam_t *jfam;
    uint16_t jfamily;

#if ORTE_ENABLE_EPOCH
    if (target->jobid == ORTE_JOBID_INVALID ||
        target->vpid == ORTE_VPID_INVALID ||
        0 == ORTE_EPOCH_CMP(proc->epoch,ORTE_EPOCH_INVALID)) {
#else
    if (target->jobid == ORTE_JOBID_INVALID ||
        target->vpid == ORTE_VPID_INVALID) {
#endif
        ret = ORTE_NAME_INVALID;
        goto found;
    }

    if (0 > ORTE_EPOCH_CMP(target->epoch, orte_ess.proc_get_epoch(target))) {
        ret = ORTE_NAME_INVALID;
        goto found;
    }

    /* if it is me, then the route is just direct */
    if (OPAL_EQUAL == opal_dss.compare(ORTE_PROC_MY_NAME, target, ORTE_NAME)) {
        ret = target;
        goto found;
    }
    
    /* if I am a tool */
    if (ORTE_PROC_IS_TOOL) {
        /* without a daemon, the route is direct */
        if (NULL == orte_process_info.my_daemon_uri) {
            ret = target;
        } else {
            /* otherwise, we go thru the local daemon */
            ret = ORTE_PROC_MY_DAEMON;
        }
        goto found;
    }
    
    /* if I am an application process, always route via my local daemon */
    if (ORTE_PROC_IS_APP) {
        ret = ORTE_PROC_MY_DAEMON;
        goto found;
    }

    /* IF THIS IS FOR A DIFFERENT JOB FAMILY... */
    if (ORTE_JOB_FAMILY(target->jobid) != ORTE_JOB_FAMILY(ORTE_PROC_MY_NAME->jobid)) {
        /* if I am a daemon, send it to the HNP */
        if (ORTE_PROC_IS_DAEMON) {
            ret = ORTE_PROC_MY_HNP;
            goto found;
        }
        /* if I am the HNP, then I stored a route to
         * this job family, so look it up
         */
        jfamily = ORTE_JOB_FAMILY(target->jobid);
        for (i=0; i < orte_routed_jobfams.size; i++) {
            if (NULL == (jfam = (orte_routed_jobfam_t*)opal_pointer_array_get_item(&orte_routed_jobfams, i))) {
                continue;
            }
            if (jfam->job_family == jfamily) {
                OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                                     "%s routed_cm: route to %s found",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_JOB_FAMILY_PRINT(target->jobid)));
                ret = &jfam->route;
                goto found;
            }
        }
        /* not found - so we have no route */
        ret = ORTE_NAME_INVALID;
        goto found;
    }
     
    /* THIS CAME FROM OUR OWN JOB FAMILY... */
    daemon.jobid = ORTE_PROC_MY_NAME->jobid;
    /* find out what daemon hosts this proc */
    if (ORTE_VPID_INVALID == (daemon.vpid = orte_ess.proc_get_daemon(target))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        ret = ORTE_NAME_INVALID;
        goto found;
    }
  
    /* Initialize daemon's epoch, based on its current vpid/jobid */
    ORTE_EPOCH_SET(daemon.epoch,orte_ess.proc_get_epoch(&daemon));

    /* if the daemon is me, then send direct to the target! */
    if (ORTE_PROC_MY_NAME->vpid == daemon.vpid) {
        ret = target;
        goto found;
    } else {
        /* otherwise, if I am the HNP, send to the daemon */
        if (ORTE_PROC_IS_HNP) {
            /*
             * Check to make sure the daemon is active, if not then return an INVALID name
             * JJH: There should be a faster way to do this check, but for now just iterate...
             */
            if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
                ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
                ret = ORTE_NAME_INVALID;
                goto found;
            }

            for(i = 0; i < jdata->procs->size; ++i) {
                if (NULL == (proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, i))) {
                    continue;
                }
                if( proc->name.vpid != daemon.vpid) {
                    continue;
                }

                OPAL_OUTPUT_VERBOSE((5, orte_routed_base_output,
                                     "%s routed_cm_get: Checking process %15s state 0x%x",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&(proc->name)),
                                     proc->state));

                if( proc->state <= ORTE_PROC_STATE_UNTERMINATED ) {
                    ret = &daemon;
                } else {
                    ret = ORTE_NAME_INVALID;
                }
                goto found;
            }
        } else {
            /* send to the HNP for routing */
            ret = ORTE_PROC_MY_HNP;
        }
        goto found;
    }
    
 found:
    OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                         "%s routed_cm_get(%s) --> %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(target), 
                         ORTE_NAME_PRINT(ret)));
    
    return *ret;
}

/* HANDLE ACK MESSAGES FROM AN HNP */
static void release_ack(int fd, short event, void *data)
{
    orte_message_event_t *mev = (orte_message_event_t*)data;
    ack_recvd = true;
    OBJ_RELEASE(mev);
}

static void recv_ack(int status, orte_process_name_t* sender,
                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                     void* cbdata)
{
    /* don't process this right away - we need to get out of the recv before
     * we process the message as it may ask us to do something that involves
     * more messaging! Instead, setup an event so that the message gets processed
     * as soon as we leave the recv.
     *
     * The macro makes a copy of the buffer, which we release above - the incoming
     * buffer, however, is NOT released here, although its payload IS transferred
     * to the message buffer for later processing
     */
    ORTE_MESSAGE_EVENT(sender, buffer, tag, release_ack);    
}


static int init_routes(orte_jobid_t job, opal_buffer_t *ndat)
{
    /* the cm module routes all proc communications through
     * the local daemon. Daemons must identify which of their
     * daemon-peers is "hosting" the specified recipient and
     * route the message to that daemon. Daemon contact info
     * is handled elsewhere, so all we need to do here is
     * ensure that the procs are told to route through their
     * local daemon, and that daemons are told how to route
     * for each proc
     */
    int rc;

    /* if I am a tool, then see if I stand alone - otherwise,
     * setup the HNP info
     */
    if (ORTE_PROC_IS_TOOL) {
        OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                             "%s routed_cm: init routes for TOOL job %s\n\thnp_uri %s\n\tdaemon uri %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_JOBID_PRINT(job),
                             (NULL == orte_process_info.my_hnp_uri) ? "NULL" : orte_process_info.my_hnp_uri,
                             (NULL == orte_process_info.my_daemon_uri) ? "NULL" : orte_process_info.my_daemon_uri));
        

        if (NULL != orte_process_info.my_daemon_uri) {
            /* set the contact info into the hash table */
            if (ORTE_SUCCESS != (rc = orte_rml.set_contact_info(orte_process_info.my_daemon_uri))) {
                ORTE_ERROR_LOG(rc);
                return(rc);
            }
            
            /* extract the hnp name and store it */
            if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_daemon_uri,
                                                               ORTE_PROC_MY_DAEMON, NULL))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            
            /* set our lifeline to the HNP - we will abort if that connection is lost */
            lifeline = ORTE_PROC_MY_DAEMON;
            
            if (NULL != orte_process_info.my_hnp_uri) {
                /* extract the hnp name and store it */
                if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_hnp_uri,
                                                                   ORTE_PROC_MY_HNP, NULL))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            }
            return ORTE_SUCCESS;
        }
        
        if (NULL == orte_process_info.my_hnp_uri) {
            return ORTE_SUCCESS;
        }
        
        /* set the contact info into the hash table */
        if (ORTE_SUCCESS != (rc = orte_rml.set_contact_info(orte_process_info.my_hnp_uri))) {
            ORTE_ERROR_LOG(rc);
            return(rc);
        }
        
        /* extract the hnp name and store it */
        if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_hnp_uri,
                                                           ORTE_PROC_MY_HNP, NULL))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        
        /* set our lifeline to the HNP - we will abort if that connection is lost */
        lifeline = ORTE_PROC_MY_HNP;
        
        return ORTE_SUCCESS;
    }
    
    /* if I am a daemon or HNP, then I have to extract the routing info for this job
     * from the data sent to me for launch and update the routing tables to
     * point at the daemon for each proc
     */
    if (ORTE_PROC_IS_DAEMON) {
        
        OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                             "%s routed_cm: init routes for daemon job %s\n\thnp_uri %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOBID_PRINT(job),
                             (NULL == orte_process_info.my_hnp_uri) ? "NULL" : orte_process_info.my_hnp_uri));
        
        if (NULL == ndat) {
            /* indicates this is being called during orte_init.
             * Get the HNP's name for possible later use
             */
            if (NULL == orte_process_info.my_hnp_uri) {
                /* fatal error */
                ORTE_ERROR_LOG(ORTE_ERR_FATAL);
                return ORTE_ERR_FATAL;
            }
            /* set the contact info into the hash table */
            if (ORTE_SUCCESS != (rc = orte_rml.set_contact_info(orte_process_info.my_hnp_uri))) {
                ORTE_ERROR_LOG(rc);
                return(rc);
            }
            
            /* extract the hnp name and store it */
            if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_hnp_uri,
                                                               ORTE_PROC_MY_HNP, NULL))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            
            /* set our lifeline to the HNP - we will abort if that connection is lost */
            lifeline = ORTE_PROC_MY_HNP;
            
            /* daemons will send their contact info back to the HNP as
             * part of the message confirming they are read to go. HNP's
             * load their contact info during orte_init
             */
        }
        
        /* ignore any other call as we only talk to the HNP */
        
        OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                             "%s routed_cm: completed init routes",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        return ORTE_SUCCESS;
    }
    
    
    if (ORTE_PROC_IS_HNP) {
        
        OPAL_OUTPUT_VERBOSE((2, orte_routed_base_output,
                             "%s routed_cm: init routes for HNP job %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOBID_PRINT(job)));
        
        if (NULL == ndat) {
            /* the HNP has no lifeline */
            lifeline = NULL;
        } else {
            /* if this is for my own jobid, then I am getting an update of RML info
             * for the daemons - so update our contact info and routes
             */
            if (ORTE_PROC_MY_NAME->jobid == job) {
                if (ORTE_SUCCESS != (rc = orte_rml_base_update_contact_info(ndat))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            } else {
                /* if not, then I need to process the callback */
                if (ORTE_SUCCESS != (rc = orte_routed_base_process_callback(job, ndat))) {
                    ORTE_ERROR_LOG(rc);
                    return rc;
                }
            }
        }

        return ORTE_SUCCESS;
    }

    {  /* MUST BE A PROC */
        /* if ndat != NULL, then this is being invoked by the proc to
         * init a route to a specified process that is outside of our
         * job family. We want that route to go through our HNP, routed via
         * out local daemon - however, we cannot know for
         * certain that the HNP already knows how to talk to the specified
         * procs. For example, in OMPI's publish/subscribe procedures, the
         * DPM framework looks for an mca param containing the global ompi-server's
         * uri. This info will come here so the proc can setup a route to
         * the server - we need to pass the routing info to our HNP
         */
        if (NULL != ndat) {
            int rc;
            opal_buffer_t xfer;
            orte_rml_cmd_flag_t cmd=ORTE_RML_UPDATE_CMD;
            
            OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                                 "%s routed_cm: init routes w/non-NULL data",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            
            /* if this is for a job family of zero, then we know that the enclosed
             * procs are local slaves to our daemon. In that case, we can just ignore this
             * as our daemon - given that it had to spawn the local slave - already
             * knows how to talk to them
             */
            if (0 == ORTE_JOB_FAMILY(job)) {
                return ORTE_SUCCESS;
            }
            
            if (ORTE_JOB_FAMILY(ORTE_PROC_MY_NAME->jobid) != ORTE_JOB_FAMILY(job)) {
                /* if this is for a different job family, then we route via our HNP
                 * to minimize connection counts to entities such as ompi-server, so
                 * start by sending the contact info to the HNP for update
                 */
                OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                                     "%s routed_cm_init_routes: diff job family - sending update to %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_HNP)));
                
                /* prep the buffer for transmission to the HNP */
                OBJ_CONSTRUCT(&xfer, opal_buffer_t);
                opal_dss.pack(&xfer, &cmd, 1, ORTE_RML_CMD);
                opal_dss.copy_payload(&xfer, ndat);

                /* save any new connections for use in subsequent connect_accept calls */
                orte_routed_base_update_hnps(ndat);

                if (0 > (rc = orte_rml.send_buffer(ORTE_PROC_MY_HNP, &xfer,
                                                   ORTE_RML_TAG_RML_INFO_UPDATE, 0))) {
                    ORTE_ERROR_LOG(rc);
                    OBJ_DESTRUCT(&xfer);
                    return rc;
                }
                OBJ_DESTRUCT(&xfer);
                
                /* wait right here until the HNP acks the update to ensure that
                 * any subsequent messaging can succeed
                 */
                ack_recvd = false;
                rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_UPDATE_ROUTE_ACK,
                                             ORTE_RML_NON_PERSISTENT, recv_ack, NULL);
                
                ORTE_PROGRESSED_WAIT(ack_recvd, 0, 1);
                
                OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                                     "%s routed_cm_init_routes: ack recvd",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
                
                /* our get_route function automatically routes all messages for
                 * other job families via the HNP, so nothing more to do here
                 */
            }
            return ORTE_SUCCESS;
        }
        
        /* if ndat=NULL, then we are being called during orte_init. In this
         * case, we need to setup a few critical pieces of info
         */
        
        OPAL_OUTPUT_VERBOSE((1, orte_routed_base_output,
                             "%s routed_cm: init routes for proc job %s\n\thnp_uri %s\n\tdaemon uri %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_JOBID_PRINT(job),
                             (NULL == orte_process_info.my_hnp_uri) ? "NULL" : orte_process_info.my_hnp_uri,
                             (NULL == orte_process_info.my_daemon_uri) ? "NULL" : orte_process_info.my_daemon_uri));
                
        if (NULL == orte_process_info.my_daemon_uri) {
            /* in this module, we absolutely MUST have this information - if
             * we didn't get it, then error out
             */
            opal_output(0, "%s ERROR: Failed to identify the local daemon's URI",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_output(0, "%s ERROR: This is a fatal condition when the cm router",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_output(0, "%s ERROR: has been selected - either select the unity router",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            opal_output(0, "%s ERROR: or ensure that the local daemon info is provided",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            return ORTE_ERR_FATAL;
        }
            
        /* Set the contact info in the RML - this won't actually establish
         * the connection, but just tells the RML how to reach the HNP
         * if/when we attempt to send to it
         */
        if (ORTE_SUCCESS != (rc = orte_rml.set_contact_info(orte_process_info.my_hnp_uri))) {
            ORTE_ERROR_LOG(rc);
            return(rc);
        }
        /* we have to set the HNP's name, even though we won't route messages directly
         * to it. This is required to ensure that we -do- send messages to the correct
         * HNP name
         */
        if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_hnp_uri,
                                                           ORTE_PROC_MY_HNP, NULL))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        
        /* Set the contact info in the RML - this won't actually establish
         * the connection, but just tells the RML how to reach the daemon
         * if/when we attempt to send to it
         */
        if (ORTE_SUCCESS != (rc = orte_rml.set_contact_info(orte_process_info.my_daemon_uri))) {
            ORTE_ERROR_LOG(rc);
            return(rc);
        }
        /* extract the daemon's name so we can update the routing table */
        if (ORTE_SUCCESS != (rc = orte_rml_base_parse_uris(orte_process_info.my_daemon_uri,
                                                           ORTE_PROC_MY_DAEMON, NULL))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        
        /* set our lifeline to the local daemon - we will abort if this connection is lost */
        lifeline = ORTE_PROC_MY_DAEMON;
        
        /* register ourselves -this sends a message to the daemon (warming up that connection)
         * and sends our contact info to the HNP when all local procs have reported
         *
         * NOTE: it may seem odd that we send our contact info to the HNP - after all,
         * the HNP doesn't really need to know how to talk to us directly if we are
         * using this routing method. However, this is good for two reasons:
         *
         * (1) some debuggers and/or tools may need RML contact
         *     info to set themselves up
         *
         * (2) doing so allows the HNP to "block" in a dynamic launch
         *     until all procs are reported running, thus ensuring that no communication
         *     is attempted until the overall ORTE system knows how to talk to everyone -
         *     otherwise, the system can just hang.
         */
        if (ORTE_SUCCESS != (rc = orte_routed_base_register_sync(true))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        /* no answer is expected or coming */
        
        return ORTE_SUCCESS;
    }
}

static int route_lost(const orte_process_name_t *route)
{
    if (ORTE_PROC_IS_HNP) {
        /* take no further action */
        return ORTE_SUCCESS;
    }

    /* if we are not the HNP and lose the connection to the
     * lifeline, and we are NOT already,
     * in finalize, tell the OOB to abort.
     * NOTE: we cannot call abort from here as the OOB needs to first
     * release a thread-lock - otherwise, we will hang!!
     */
    if (!orte_finalizing &&
        NULL != lifeline &&
        OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, route, lifeline)) {
        opal_output(0, "%s routed:cm: Connection to lifeline %s lost",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    ORTE_NAME_PRINT(lifeline));
        return ORTE_ERR_FATAL;
    }

    /* we don't care about this one, so return success */
    return ORTE_SUCCESS;
}


static bool route_is_defined(const orte_process_name_t *target)
{
    /* find out what daemon hosts this proc */
    if (ORTE_VPID_INVALID == orte_ess.proc_get_daemon((orte_process_name_t*)target)) {
        return false;
    }
    
    return true;
}

static int set_lifeline(orte_process_name_t *proc)
{
    /* we have to copy the proc data because there is no
     * guarantee that it will be preserved
     */
    local_lifeline.jobid = proc->jobid;
    local_lifeline.vpid = proc->vpid;
    ORTE_EPOCH_SET(local_lifeline.epoch,orte_ess.proc_get_epoch(&local_lifeline));
    
    lifeline = &local_lifeline;
    
    return ORTE_SUCCESS;
}

static int update_routing_tree(orte_jobid_t jobid)
{
    /* nothing to do here */
    return ORTE_SUCCESS;
}

static orte_vpid_t get_routing_tree(opal_list_t *children)
{
    orte_routed_tree_t *nm;
    int32_t i;
    orte_job_t *jdata;
    orte_proc_t *proc;
    
    /* if I am anything other than a daemon or the HNP, this
     * is a meaningless command as I am not allowed to route
     */
    if (!ORTE_PROC_IS_DAEMON && !ORTE_PROC_IS_HNP) {
        return ORTE_VPID_INVALID;
    }
    
    /* if I am a daemon, I do not have any children */
    if (ORTE_PROC_IS_DAEMON) {
        return ORTE_PROC_MY_HNP->vpid;
    }
    
    /* for the HNP, the cm routing tree is direct to all known alive daemons */
    if (NULL != children) {
        if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return ORTE_ERR_NOT_FOUND;
        }

        for(i = 0; i < jdata->procs->size; ++i) {
            if (NULL == (proc = (orte_proc_t*)opal_pointer_array_get_item(jdata->procs, i))) {
                continue;
            }
            if( proc->name.vpid == 0) {
                continue;
            }

            if( proc->state <= ORTE_PROC_STATE_UNTERMINATED &&
                NULL != proc->rml_uri ) {
                OPAL_OUTPUT_VERBOSE((5, orte_routed_base_output,
                                     "%s get_routing_tree: Adding process %15s state 0x%x",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&(proc->name)),
                                     proc->state));

                nm = OBJ_NEW(orte_routed_tree_t);
                nm->vpid = proc->name.vpid;
                opal_bitmap_clear_all_bits(&nm->relatives);
                opal_list_append(children, &nm->super);
            }
            else {
                OPAL_OUTPUT_VERBOSE((5, orte_routed_base_output,
                                     "%s get_routing_tree: Skipped process %15s state 0x%x (non functional daemon)",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     ORTE_NAME_PRINT(&(proc->name)),
                                     proc->state));
            }
        }
    }
    
    /* I have no parent */
    return ORTE_VPID_INVALID;
}

static int get_wireup_info(opal_buffer_t *buf)
{
    int rc;
    int i;
    orte_routed_jobfam_t *jfam;

    if (ORTE_PROC_IS_HNP) {
        /* if we are not using static ports, then we need to share the
         * comm info - otherwise, just return
         */
        if (orte_static_ports) {
            return ORTE_SUCCESS;
        }
    
        if (ORTE_SUCCESS != (rc = orte_rml_base_get_contact_info(ORTE_PROC_MY_NAME->jobid, buf))) {
            ORTE_ERROR_LOG(rc);
        }
        return rc;
    }
    
    /* if I am an application, this is occurring during connect_accept.
     * We need to return the stored information of other HNPs we
     * know about, if any
     */
    if (ORTE_PROC_IS_APP) {
        for (i=0; i < orte_routed_jobfams.size; i++) {
            if (NULL != (jfam = (orte_routed_jobfam_t*)opal_pointer_array_get_item(&orte_routed_jobfams, i))) {
                opal_dss.pack(buf, &(jfam->hnp_uri), 1, OPAL_STRING);
            }
        }
        return ORTE_SUCCESS;
    }

    return ORTE_SUCCESS;
}

static size_t num_routes(void)
{
    orte_job_t *jdata;

    if (!ORTE_PROC_IS_HNP) {
        return 0;
    }

    /* if I am the HNP, then the number of routes is
     * the number of daemons (other than me) still alive
     */
    if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        return 0;
    }
    return (jdata->num_procs - jdata->num_terminated - 1);
}

#if OPAL_ENABLE_FT_CR == 1
static int cm_ft_event(int state)
{
    int ret, exit_status = ORTE_SUCCESS;

    /******** Checkpoint Prep ********/
    if(OPAL_CRS_CHECKPOINT == state) {
    }
    /******** Continue Recovery ********/
    else if (OPAL_CRS_CONTINUE == state ) {
    }
    /******** Restart Recovery ********/
    else if (OPAL_CRS_RESTART == state ) {
        /*
         * Re-exchange the routes
         */
        if (ORTE_SUCCESS != (ret = orte_routed.init_routes(ORTE_PROC_MY_NAME->jobid, NULL))) {
            exit_status = ret;
            goto cleanup;
        }
    }
    else if (OPAL_CRS_TERM == state ) {
        /* Nothing */
    }
    else {
        /* Error state = Nothing */
    }

 cleanup:
    return exit_status;
}
#endif

