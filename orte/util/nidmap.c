/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
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
#include "orte/types.h"
#include "orte/constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/dss/dss.h"
#include "opal/runtime/opal.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/mca/hwloc/hwloc.h"
#include "opal/util/output.h"
#include "opal/util/opal_sos.h"
#include "opal/util/argv.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/odls/base/odls_private.h"
#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/util/regex.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/rml/base/rml_contact.h"

#include "orte/util/nidmap.h"

static bool initialized = false;

int orte_util_nidmap_init(opal_buffer_t *buffer)
{
    int32_t cnt;
    int rc;
    opal_byte_object_t *bo;
    
    if (!initialized) {
        /* need to construct the global arrays */
        /* setup the nidmap array */
        OBJ_CONSTRUCT(&orte_nidmap, opal_pointer_array_t);
        opal_pointer_array_init(&orte_nidmap, 8, INT32_MAX, 8);
        
        /* setup array of jmaps */
        OBJ_CONSTRUCT(&orte_jobmap, opal_pointer_array_t);
        opal_pointer_array_init(&orte_jobmap, 1, INT32_MAX, 1);
        
        /* make sure we don't do this twice */
        initialized = true;
    }
    
    /* it is okay if the buffer is empty */
    if (NULL == buffer || 0 == buffer->bytes_used) {
        return ORTE_SUCCESS;
    }
    
    /* extract the byte object holding the daemonmap */
    cnt=1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &bo, &cnt, OPAL_BYTE_OBJECT))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* unpack the node map */
    if (ORTE_SUCCESS != (rc = orte_util_decode_nodemap(bo))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* the bytes in the object were free'd by the decode */
    
    /* extract the byte object holding the process map */
    cnt=1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &bo, &cnt, OPAL_BYTE_OBJECT))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* unpack the process map */
    if (ORTE_SUCCESS != (rc = orte_util_decode_pidmap(bo))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    /* the bytes in the object were free'd by the decode */
#if OPAL_HAVE_HWLOC
    /* extract the topology */
    if (NULL == opal_hwloc_topology) {
        cnt=1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &opal_hwloc_topology, &cnt, OPAL_HWLOC_TOPO))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
#endif

    return ORTE_SUCCESS;
}

void orte_util_nidmap_finalize(void)
{
    orte_nid_t *nid;
    orte_jmap_t *jmap;
    int32_t i;
    
    if (!initialized) {
        /* nothing to do */
        return;
    }
    
    /* deconstruct the global nidmap and jobmap arrays */
    for (i=0; i < orte_nidmap.size; i++) {
        if (NULL == (nid = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
            continue;
        }
        OBJ_RELEASE(nid);
    }
    OBJ_DESTRUCT(&orte_nidmap);
    for (i=0; i < orte_jobmap.size; i++) {
        if (NULL == (jmap = (orte_jmap_t*)opal_pointer_array_get_item(&orte_jobmap, i))) {
            continue;
        }
        OBJ_RELEASE(jmap);
    }
    OBJ_DESTRUCT(&orte_jobmap);
    
#if OPAL_HAVE_HWLOC
    /* destroy the topology */
    if (NULL != opal_hwloc_topology) {
        hwloc_topology_destroy(opal_hwloc_topology);
        opal_hwloc_topology = NULL;
    }
#endif

    /* flag that these are no longer initialized */
    initialized = false;
}

int orte_util_setup_local_nidmap_entries(void)
{
    orte_nid_t *node;
    orte_jmap_t *jmap;
    orte_pmap_t *pmap;

    /* add a jmap entry for myself */
    jmap = OBJ_NEW(orte_jmap_t);
    jmap->job = ORTE_PROC_MY_NAME->jobid;
    opal_pointer_array_add(&orte_jobmap, jmap);
    jmap->num_procs = 1;
    
    /* create a nidmap entry for this node */
    node = OBJ_NEW(orte_nid_t);
    node->name = strdup(orte_process_info.nodename);
    node->daemon = ORTE_PROC_MY_DAEMON->vpid;
    pmap = OBJ_NEW(orte_pmap_t);
    pmap->local_rank = 0;
    pmap->node_rank = 0;
    node->index = opal_pointer_array_add(&orte_nidmap, node);
    /* value array copies values, so everything must be set before
     * calling the set_item function
     */
    pmap->node = node->index;
    opal_pointer_array_set_item(&jmap->pmap, ORTE_PROC_MY_NAME->vpid, pmap);
    
    /* all done */
    return ORTE_SUCCESS;
}

int orte_util_build_daemon_nidmap(char **nodes)
{
    orte_nid_t *node;
    int i, num_nodes;
    int rc;
    struct hostent *h;
    opal_buffer_t buf;
    orte_process_name_t proc;
    char *uri, *addr;
    char *proc_name;
    
    num_nodes = opal_argv_count(nodes);
    
    OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                         "%s orte:util:build:daemon:nidmap found %d nodes",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), num_nodes));
    
    if (0 == num_nodes) {
        /* nothing to do */
        return ORTE_SUCCESS;
    }
    
    /* set the size of the nidmap storage so we minimize realloc's */
    if (ORTE_SUCCESS != (rc = opal_pointer_array_set_size(&orte_nidmap, num_nodes+1))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* install the entry for the HNP */
    node = OBJ_NEW(orte_nid_t);
    node->name = strdup("HNP");
    node->daemon = 0;
    /* the arch defaults to our arch so that non-hetero
     * case will yield correct behavior
     */
    opal_pointer_array_set_item(&orte_nidmap, 0, node);        
    
    /* the daemon vpids will be assigned in order,
     * starting with vpid=1 for the first node in
     * the list
     */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    proc.jobid = ORTE_PROC_MY_NAME->jobid;
    for (i=0; i < num_nodes; i++) {
        node = OBJ_NEW(orte_nid_t);
        node->name = strdup(nodes[i]);
        node->daemon = i+1;
        /* the arch defaults to our arch so that non-hetero
         * case will yield correct behavior
         */
        opal_pointer_array_set_item(&orte_nidmap, node->daemon, node);        
        
        /* lookup the address of this node */
        if (NULL == (h = gethostbyname(node->name))) {
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return ORTE_ERR_NOT_FOUND;
        }
        addr = inet_ntoa(*(struct in_addr*)h->h_addr_list[0]);
        
        /* since we are using static ports, all my fellow daemons will be on my
         * port. Setup the contact info for each daemon in my hash tables. Note
         * that this will -not- open a port to those daemons, but will only
         * define the info necessary for opening such a port if/when I communicate
         * to them
         */
        /* construct the URI */
        proc.vpid = node->daemon;
        ORTE_EPOCH_SET(proc.epoch,ORTE_EPOCH_MIN);

        orte_util_convert_process_name_to_string(&proc_name, &proc);
        asprintf(&uri, "%s;tcp://%s:%d", proc_name, addr, (int)orte_process_info.my_port);
        OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                             "%s orte:util:build:daemon:nidmap node %s daemon %d addr %s uri %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             node->name, (int)node->daemon, addr, uri));
        opal_dss.pack(&buf, &uri, 1, OPAL_STRING);
        free(proc_name);
        free(uri);
    }
    
    /* load the hash tables */
    if (ORTE_SUCCESS != (rc = orte_rml_base_update_contact_info(&buf))) {
        ORTE_ERROR_LOG(rc);
    }
    OBJ_DESTRUCT(&buf);

    return rc;
}

int orte_util_encode_nodemap(opal_byte_object_t *boptr)
{
    orte_vpid_t *vpids;
    orte_node_t *node, *hnp;
    int32_t i, num_nodes;
    int rc;
    char *nodename;
    opal_buffer_t buf;
    char *ptr;
    uint8_t *oversub=NULL;

    /* setup a buffer for tmp use */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);

    /* determine the number of nodes in the global node array */
    num_nodes = 0;
    for (i=0; i < orte_node_pool->size; i++) {
        if (NULL == opal_pointer_array_get_item(orte_node_pool, i)) {
            continue;
        }
        ++num_nodes;
    }

    /* pack number of nodes */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, &num_nodes, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* the HNP always has an entry at posn 0 - get its pointer as
     * we will need it later
     */
    hnp = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, 0);

    /* pack every nodename individually */
    for (i=0; i < orte_node_pool->size; i++) {
        if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, i))) {
            continue;
        }
        if (!orte_keep_fqdn_hostnames) {
            nodename = strdup(node->name);
            if (NULL != (ptr = strchr(nodename, '.'))) {
                *ptr = '\0';
            }
            if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, &nodename, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            free(nodename);
        } else {
            if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, &node->name, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
        }
    }
    
    /* since the daemon vpids may not correspond to the node
     * index, we need to also pack the vpid array for all
     * daemons. This scenario can happen when the user is
     * employing a mapping algo that doesn't use all allocated
     * nodes, and sprinkles procs across them in some non-contig
     * manner. For example, use of the seq mapper where only
     * some nodes are used, and where the usage leaves "holes"
     * in the node array, will cause the daemon vpids to not
     * match their node array index
     */
    
    /* allocate space for the daemon vpids and oversubscribed flags */
    vpids = (orte_vpid_t*)malloc(num_nodes * sizeof(orte_vpid_t));
    oversub = (uint8_t*)malloc(num_nodes * sizeof(uint8_t));
    for (i=0; i < orte_node_pool->size; i++) {
        if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(orte_node_pool, i))) {
            continue;
        }
        if (NULL == node->daemon) {
            /* some nodes may not have daemons on them */
            vpids[i] = ORTE_VPID_INVALID;
            continue;
        }
        vpids[i] = node->daemon->name.vpid;
        oversub[i] = node->oversubscribed;
    }
    if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, vpids, num_nodes, ORTE_VPID))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    free(vpids);
    if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, oversub, num_nodes, OPAL_UINT8))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    free(oversub);
        
    /* transfer the payload to the byte object */
    opal_dss.unload(&buf, (void**)&boptr->bytes, &boptr->size);
    OBJ_DESTRUCT(&buf);
    
    return ORTE_SUCCESS;
}

int orte_util_decode_nodemap(opal_byte_object_t *bo)
{
    int n;
    int32_t num_nodes, i, num_daemons;
    orte_nid_t *node;
    orte_vpid_t *vpids;
    orte_nid_t *nd, *ndptr;
    opal_buffer_t buf;
    int rc;
    uint8_t *oversub;

    OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                         "%s decode:nidmap decoding nodemap",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* if there are any entries already in the node array, clear it out */
    if (0 < orte_nidmap.size) {
        /* unfortunately, the opal function "remove_all" doesn't release
         * the memory pointed to by the elements in the array, so we need
         * to release those first
         */
        for (i=0; i < orte_nidmap.size; i++) {
            if (NULL != (ndptr = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
                OBJ_RELEASE(ndptr);
            }
        }
        /* now use the opal function to reset the internal pointers */
        opal_pointer_array_remove_all(&orte_nidmap);
    }
    
    /* xfer the byte object to a buffer for unpacking */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    opal_dss.load(&buf, bo->bytes, bo->size);
    
    /* unpack number of nodes */
    n=1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, &num_nodes, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
 
    OPAL_OUTPUT_VERBOSE((2, orte_debug_output,
                         "%s decode:nidmap decoding %d nodes",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), num_nodes));
    
    /* set the size of the nidmap storage so we minimize realloc's */
    if (ORTE_SUCCESS != (rc = opal_pointer_array_set_size(&orte_nidmap, num_nodes))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* loop over nodes and unpack the raw nodename */
    for (i=0; i < num_nodes; i++) {
        node = OBJ_NEW(orte_nid_t);
        /* the arch defaults to our arch so that non-hetero
         * case will yield correct behavior
         */
        opal_pointer_array_set_item(&orte_nidmap, i, node);
        
        /* unpack the node's name */
        n=1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, &(node->name), &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    
    /* unpack the daemon vpids */
    vpids = (orte_vpid_t*)malloc(num_nodes * sizeof(orte_vpid_t));
    n=num_nodes;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, vpids, &n, ORTE_VPID))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* unpack the oversubscribed flags */
    oversub = (uint8_t*)malloc(num_nodes * sizeof(uint8_t));
    n=num_nodes;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, oversub, &n, OPAL_UINT8))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* transfer the data to the nidmap, counting the number of
     * daemons in the system
     */
    num_daemons = 0;
    for (i=0; i < num_nodes; i++) {
        if (NULL != (ndptr = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
            ndptr->daemon = vpids[i];
            if (0 == oversub[i]) {
                ndptr->oversubscribed = false;
            } else {
                ndptr->oversubscribed = true;
            }
            if (ORTE_VPID_INVALID != vpids[i]) {
                ++num_daemons;
            }
        }
    }
    free(vpids);
    free(oversub);

    /* if we are a daemon or the HNP, update our num_procs */
    if (ORTE_PROC_IS_HNP || ORTE_PROC_IS_DAEMON) {
        orte_process_info.num_procs = num_daemons;

        if (orte_process_info.max_procs < orte_process_info.num_procs) {
            orte_process_info.max_procs = orte_process_info.num_procs;
        }
    }
    /* update num_daemons */
    orte_process_info.num_daemons = num_daemons;
    
    if (0 < opal_output_get_verbosity(orte_debug_output)) {
        for (i=0; i < num_nodes; i++) {
            if (NULL == (nd = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
                continue;
            }
            opal_output(0, "%s node[%d].name %s daemon %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), i,
                        (NULL == nd->name) ? "NULL" : nd->name,
                        ORTE_VPID_PRINT(nd->daemon));
        }
    }

    OBJ_DESTRUCT(&buf);
    return ORTE_SUCCESS;
}

int orte_util_encode_pidmap(opal_byte_object_t *boptr)
{
    orte_proc_t *proc;
    opal_buffer_t buf;
    orte_local_rank_t *lrank = NULL;
    orte_node_rank_t *nrank = NULL;
    orte_job_t *jdata = NULL;
    int32_t *nodes = NULL;
    int i, j, k, rc = ORTE_SUCCESS;

    /* setup the working buffer */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    
    for (j=1; j < orte_job_data->size; j++) {
        /* the job array is no longer left-justified and may
         * have holes in it as we recover resources at job
         * completion
         */
        if (NULL == (jdata = (orte_job_t*)opal_pointer_array_get_item(orte_job_data, j))) {
            continue;
        }
        /* if this job doesn't have a map, then it is a tool
         * and doesn't need to be included
         */
        if (NULL == jdata->map) {
            continue;
        }
        /* pack the jobid */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, &jdata->jobid, 1, ORTE_JOBID))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup_and_return;
        }
        /* pack the number of procs */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, &jdata->num_procs, 1, ORTE_VPID))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup_and_return;
        }
        
        /* allocate memory for the nodes, local ranks and node ranks */
        nodes = (int32_t*)malloc(jdata->num_procs * sizeof(int32_t));
        lrank = (orte_local_rank_t*)malloc(jdata->num_procs*sizeof(orte_local_rank_t));
        nrank = (orte_node_rank_t*)malloc(jdata->num_procs*sizeof(orte_node_rank_t));
        
        /* transfer and pack the node info in one pack */
        for (i=0, k=0; i < jdata->procs->size; i++) {
            if (NULL == (proc = (orte_proc_t *) opal_pointer_array_get_item(jdata->procs, i))) {
                continue;
            }
            if( k >= (int)jdata->num_procs ) {
                orte_show_help("help-orte-runtime.txt", "orte_nidmap:too_many_nodes",
                               true, jdata->num_procs);
                break;
            }
            nodes[k] = proc->node->index;
            lrank[k] = proc->local_rank;
            nrank[k] = proc->node_rank;
            ++k;
        }
        if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, nodes, jdata->num_procs, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup_and_return;
        }
        /* transfer and pack the local_ranks in one pack */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, lrank, jdata->num_procs, ORTE_LOCAL_RANK))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup_and_return;
        }
        /* transfer and pack the node ranks in one pack */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(&buf, nrank, jdata->num_procs, ORTE_NODE_RANK))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup_and_return;
        }
    }
    
    /* transfer the payload to the byte object */
    opal_dss.unload(&buf, (void**)&boptr->bytes, &boptr->size);

 cleanup_and_return:

    if( NULL != lrank ) {
        free(lrank);
    }
    if( NULL != nrank ) {
        free(nrank);
    }
    if( NULL != nodes ) {
        free(nodes);
    }
    OBJ_DESTRUCT(&buf);
    
    return rc;
}


int orte_util_decode_pidmap(opal_byte_object_t *bo)
{
    orte_jobid_t jobid;
    orte_vpid_t i, num_procs;
    orte_pmap_t *pmap;
    int32_t *nodes;
    orte_local_rank_t *local_rank;
    orte_node_rank_t *node_rank;
    orte_std_cntr_t n;
    opal_buffer_t buf;
    orte_jmap_t *jmap;
    bool already_present;
    int j;
    int rc;
    
    /* xfer the byte object to a buffer for unpacking */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    if (ORTE_SUCCESS != (rc = opal_dss.load(&buf, bo->bytes, bo->size))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    
    n = 1;
    /* cycle through the buffer */
    while (ORTE_SUCCESS == (rc = opal_dss.unpack(&buf, &jobid, &n, ORTE_JOBID))) {
        /* unfortunately, job objects cannot be stored
         * by index number as the jobid is a constructed
         * value. So we have no choice but to cycle through
         * the jobmap pointer array and look for this entry. Since
         * jobs are cleaned up as they complete, check the
         * entire array
         */
        jmap = NULL;
        already_present = false;
        for (j=0; j < orte_jobmap.size; j++) {
            if (NULL == (jmap = (orte_jmap_t*)opal_pointer_array_get_item(&orte_jobmap, j))) {
                continue;
            }
            if (jobid == jmap->job) {
                already_present = true;
                break;
            }
        }
        
        /* unpack the number of procs */
        n=1;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, &num_procs, &n, ORTE_VPID))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }

        /* allocate memory for the node info */
        nodes = (int32_t*)malloc(num_procs * 4);
        /* unpack it in one shot */
        n=num_procs;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, nodes, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        
        /* allocate memory for local ranks */
        local_rank = (orte_local_rank_t*)malloc(num_procs*sizeof(orte_local_rank_t));
        /* unpack them in one shot */
        n=num_procs;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, local_rank, &n, ORTE_LOCAL_RANK))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        
        /* allocate memory for node ranks */
        node_rank = (orte_node_rank_t*)malloc(num_procs*sizeof(orte_node_rank_t));
        /* unpack node ranks in one shot */
        n=num_procs;
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(&buf, node_rank, &n, ORTE_NODE_RANK))) {
            ORTE_ERROR_LOG(rc);
            goto cleanup;
        }
        
        /* if we already know about this job, we need to check the data to see
         * if something has changed - e.g., a proc that is being restarted somewhere
         * other than where it previously was
         */
        if (already_present) {
            /* we already have the jmap object, so let's refresh its pidmap
             * using the new data - start by cleaning out the old array
             */
            for (j=0; j < jmap->pmap.size; j++) {
                if (NULL == (pmap = (orte_pmap_t*)opal_pointer_array_get_item(&jmap->pmap, j))) {
                    continue;
                }
                OBJ_RELEASE(pmap);
            }
            /* now use the opal function to reset the internal pointers */
            opal_pointer_array_remove_all(&jmap->pmap);
            /* set the size of the storage so we minimize realloc's */
            if (ORTE_SUCCESS != (rc = opal_pointer_array_set_size(&jmap->pmap, num_procs))) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            /* add in the updated array */
            for (i=0; i < num_procs; i++) {
                pmap = OBJ_NEW(orte_pmap_t);
                /* add the pidmap entry at the specific site corresponding
                 * to the proc's vpid
                 */
                if (ORTE_SUCCESS != (rc = opal_pointer_array_set_item(&jmap->pmap, i, pmap))) {
                    ORTE_ERROR_LOG(rc);
                    goto cleanup;
                }
                /* add/update the data */
                pmap->node = nodes[i];
                pmap->local_rank = local_rank[i];
                pmap->node_rank = node_rank[i];
            }
            /* update the #procs */
            jmap->num_procs = num_procs;
        } else {
            /* if we don't already have this data, store it
             * unfortunately, job objects cannot be stored
             * by index number as the jobid is a constructed
             * value. So we have to just add it to the end
             * of the array
             */
            jmap = OBJ_NEW(orte_jmap_t);
            jmap->job = jobid;
            jmap->num_procs = num_procs;
            if (0 > (j = opal_pointer_array_add(&orte_jobmap, jmap))) {
                ORTE_ERROR_LOG(j);
                rc = j;
                goto cleanup;
            }
            /* allocate memory for the procs array */
            opal_pointer_array_set_size(&jmap->pmap, num_procs);
            /* xfer the data */
            for (i=0; i < num_procs; i++) {
                pmap = OBJ_NEW(orte_pmap_t);
                pmap->node = nodes[i];
                pmap->local_rank = local_rank[i];
                pmap->node_rank = node_rank[i];
                /* add the pidmap entry at the specific site corresponding
                 * to the proc's vpid
                 */
                if (ORTE_SUCCESS != (rc = opal_pointer_array_set_item(&jmap->pmap, i, pmap))) {
                    ORTE_ERROR_LOG(rc);
                    goto cleanup;
                }
            }
        }
        
        /* release data */
        free(nodes);
        free(local_rank);
        free(node_rank);
        /* setup for next cycle */
        n = 1;
    }
    if (ORTE_ERR_UNPACK_READ_PAST_END_OF_BUFFER == OPAL_SOS_GET_ERROR_CODE(rc)) {
        rc = ORTE_SUCCESS;
    }
    
cleanup:
    OBJ_DESTRUCT(&buf);
    return rc;
}


/***   NIDMAP UTILITIES   ***/
orte_jmap_t* orte_util_lookup_jmap(orte_jobid_t job)
{
    int i;
    orte_jmap_t *jmap;
    
    /* unfortunately, job objects cannot be stored
     * by index number as the jobid is a constructed
     * value. So we have no choice but to cycle through
     * the jobmap pointer array and look for the entry
     * we want. We also cannot trust that the array is
     * left-justified as cleanup is done - and array
     * entries set to NULL - upon job completion.
     */
    for (i=0; i < orte_jobmap.size; i++) {
        if (NULL == (jmap = (orte_jmap_t*)opal_pointer_array_get_item(&orte_jobmap, i))) {
            continue;
        }
        OPAL_OUTPUT_VERBOSE((10, orte_debug_output,
                             "%s lookup:pmap: checking job %s for job %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_JOBID_PRINT(jmap->job), ORTE_JOBID_PRINT(job)));
        if (job == jmap->job) {
            return jmap;
        }
    }
    
    /* if we didn't find it, return NULL */
    return NULL;
}

orte_pmap_t* orte_util_lookup_pmap(orte_process_name_t *proc)
{
    orte_jmap_t *jmap;
    
    if (NULL == (jmap = orte_util_lookup_jmap(proc->jobid))) {
        return NULL;
    }
    
    /* the get_item function will check the array index range,
     * so we can just access it here
     */
    return (orte_pmap_t *) opal_pointer_array_get_item(&jmap->pmap, proc->vpid);
}

/* the daemon's vpid does not necessarily correlate
 * to the node's index in the node array since
 * some nodes may not have a daemon on them. Thus,
 * we have to search for the daemon in the array.
 * Fortunately, this is rarely done
 */
static orte_nid_t* find_daemon_node(orte_process_name_t *proc)
{
    int32_t i;
    orte_nid_t *nid;
    
    for (i=0; i < orte_nidmap.size; i++) {
        if (NULL == (nid = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
            continue;
        }
        OPAL_OUTPUT_VERBOSE((10, orte_debug_output,
                             "%s find:daemon:node: checking daemon %s for %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_VPID_PRINT(nid->daemon), ORTE_VPID_PRINT(proc->vpid)));
        if (nid->daemon == proc->vpid) {
            return nid;
        }
    }
    
    /* if we didn't find it, return NULL */
    return NULL;
}

orte_nid_t* orte_util_lookup_nid(orte_process_name_t *proc)
{
    orte_pmap_t *pmap;
    
    OPAL_OUTPUT_VERBOSE((5, orte_debug_output,
                         "%s lookup:nid: looking for proc %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc)));
    
    if (ORTE_JOBID_IS_DAEMON(proc->jobid)) {
        /* looking for a daemon */
        return find_daemon_node(proc);
    }
    
    /* looking for an application proc */
    if (NULL == (pmap = orte_util_lookup_pmap(proc))) {
        return NULL;
    }
    
    /* the get_item function will check the array index range,
     * so we can just access it here
     */
    return (orte_nid_t *) opal_pointer_array_get_item(&orte_nidmap, pmap->node);
}

void orte_nidmap_dump(void)
{
    int i;
    orte_nid_t *nid;

    opal_output(orte_clean_output, "***   DUMP OF NIDMAP   ***");
    for (i=0; i < orte_nidmap.size; i++) {
        if (NULL == (nid = (orte_nid_t*)opal_pointer_array_get_item(&orte_nidmap, i))) {
            continue;
        }
        opal_output(orte_clean_output, "%s node[%d].name %s daemon %s",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), i,
                    (NULL == nid->name) ? "NULL" : nid->name,
                    ORTE_VPID_PRINT(nid->daemon));
    }
    opal_output(orte_clean_output, "\n\n");
}

void orte_jmap_dump(orte_jmap_t *jmap)
{
    int i;
    orte_pmap_t *pmap;
    
    opal_output(orte_clean_output, "****   DUMP OF JOB %s (%s procs)   ***",
                ORTE_JOBID_PRINT(jmap->job), ORTE_VPID_PRINT(jmap->num_procs));
    
    for (i=0; i < jmap->pmap.size; i++) {
        if (NULL == (pmap = (orte_pmap_t*)opal_pointer_array_get_item(&jmap->pmap, i))) {
            continue;
        }
        opal_output(orte_clean_output, "\tnode %d local_rank %d node_rank %d",
                    pmap->node, (int)pmap->local_rank, (int)pmap->node_rank);
    }
    opal_output(orte_clean_output, "\n");
}

void orte_jobmap_dump(void)
{
    int i;
    orte_jmap_t *jmap;
    
    opal_output(orte_clean_output, "***   DUMP OF JOBMAP   ***");
    for (i=0; i < orte_jobmap.size; i++) {
        if (NULL == (jmap = (orte_jmap_t*)opal_pointer_array_get_item(&orte_jobmap, i))) {
            continue;
        }
        orte_jmap_dump(jmap);
    }
    opal_output(orte_clean_output, "\n\n");
}

#if 0
/* Useful for debugging. Not used otherwise. */
void print_orte_job_data() {
    orte_job_t *jdata;
    orte_proc_t *pdata;
    int i, j;

    if (NULL == orte_job_data) {
        opal_output(0, "ORTE_JOB_DATA == NULL");
        return;
    }

    for (i = 0; i < orte_job_data->size; i++) {
        if (NULL == (jdata = (orte_job_t *) opal_pointer_array_get_item(orte_job_data, i))) {
            continue;
        }
        opal_output(0, "JOB: %s", ORTE_JOBID_PRINT(jdata->jobid));

        for (j = 0; j < jdata->num_procs; j++) {
            if (NULL == (pdata = (orte_proc_t *) opal_pointer_array_get_item(jdata->procs, j))) {
                continue;
            }
            opal_output(0, "    PROC: %s", ORTE_NAME_PRINT(&(pdata->name)));
        }
    }
}
#endif

#if ORTE_ENABLE_EPOCH
/* Look up the current epoch value that we have stored locally.
 *
 * Note that this will not ping the HNP to get the most up to date epoch stored
 * there, but it assumes that when it needs to know that the epoch has changed,
 * someone will tell it.  If you need the most up to date epoch, you should
 * tell ask the hnp to refresh our information.
 */
orte_epoch_t orte_util_lookup_epoch(orte_process_name_t *proc)
{
    return get_epoch_from_orte_job_data(proc, ORTE_EPOCH_INVALID);
}

/* Set the current epoch value that we have stored locally.
 *
 * This will update the currently stored local value for the epoch.
 */
orte_epoch_t orte_util_set_epoch(orte_process_name_t *proc, orte_epoch_t epoch)
{
    orte_epoch_t e = get_epoch_from_orte_job_data(proc, epoch);
    /*print_orte_job_data();*/
    return e;
}
#endif

#if ORTE_RESIL_ORTE
bool orte_util_proc_is_running(orte_process_name_t *proc) {
    int i;
    unsigned int j;
    orte_job_t *jdata;
    orte_proc_t *pdata;

    if (NULL == orte_job_data) {
        return false;
    }

    for (i = 0; i < orte_job_data->size; i++) {
        if (NULL == (jdata = (orte_job_t *) opal_pointer_array_get_item(orte_job_data, i))) {
            continue;
        } else if (proc->jobid == jdata->jobid) {
            for (j = 0; j < jdata->num_procs; j++) {
                if (NULL == (pdata = (orte_proc_t *) opal_pointer_array_get_item(jdata->procs, j))) {
                    continue;
                } else if (proc->vpid == pdata->name.vpid) {
                    return ORTE_PROC_STATE_TERMINATED > pdata->state;
                }
            }
        }
    }

    return true;
}

int orte_util_set_proc_state(orte_process_name_t *proc, orte_proc_state_t state) {
    int i;
    unsigned int j;
    orte_job_t *jdata;
    orte_proc_t *pdata;

    if (NULL == orte_job_data) {
        return ORTE_ERROR;
    }

    for (i = 0; i < orte_job_data->size; i++) {
        if (NULL == (jdata = (orte_job_t *) opal_pointer_array_get_item(orte_job_data, i))) {
            continue;
        } else if (proc->jobid == jdata->jobid) {
            for (j = 0; j < jdata->num_procs; j++) {
                if (NULL == (pdata = (orte_proc_t *) opal_pointer_array_get_item(jdata->procs, j))) {
                    continue;
                } else if (proc->vpid == pdata->name.vpid) {
                    pdata->state = state;
                    return ORTE_SUCCESS;
                }
            }
        }
    }

    return ORTE_ERROR;
}
#endif

#if ORTE_ENABLE_EPOCH
/*
 * This function performs both the get and set operations on the epoch for a
 * sepcific process name. If the epoch passed into the function is
 * ORTE_EPOCH_INVALID, then we are performing a get operation. If the epoch is
 * anything else, we are performing a set operation.
 */
orte_epoch_t get_epoch_from_orte_job_data(orte_process_name_t *proc, orte_epoch_t epoch) {
    int ret, i;
    unsigned int j;
    orte_job_t *jdata;
    orte_proc_t *pdata;

    if (ORTE_JOBID_INVALID == proc->jobid || 
        ORTE_VPID_INVALID  == proc->vpid) {
        return ORTE_EPOCH_INVALID;
    }

    /* Sanity check just to make sure we don't overwrite our existing
     * orte_job_data.
     */
    if (NULL == orte_job_data) {
        orte_job_data = OBJ_NEW(opal_pointer_array_t);
        if (ORTE_SUCCESS != (ret = opal_pointer_array_init(orte_job_data,
                                        1,
                                        ORTE_GLOBAL_ARRAY_MAX_SIZE,
                                        1))) {
            ORTE_ERROR_LOG(ret);
            return ORTE_EPOCH_INVALID;
        }
    }

    /* Look to see if the job is in the orte_job_data. */
    for (i = 0; i < orte_job_data->size; i++) {
        if (NULL == (jdata = (orte_job_t *) opal_pointer_array_get_item(orte_job_data, i))) {
            continue;
        } else if (proc->jobid == jdata->jobid) {
            /* Found the right job, now look for the process. */
            for (j = 0; j < jdata->num_procs; j++) {
                if (NULL == (pdata = (orte_proc_t *) opal_pointer_array_get_item(jdata->procs, j))) {
                    continue;
                } else if (proc->vpid == pdata->name.vpid) {
                    if (ORTE_EPOCH_INVALID != epoch) {
                        pdata->name.epoch = epoch;
                    }
                    return pdata->name.epoch;
                }
            }

            /* Found the right job but didn't find the process in it. Create the
             * process if necessary.
             */
            if (ORTE_EPOCH_INVALID != epoch) {
                pdata = OBJ_NEW(orte_proc_t);
                pdata->name.jobid = proc->jobid;
                pdata->name.vpid = proc->vpid;
                pdata->name.epoch = epoch;

                pdata->state = ORTE_PROC_STATE_TERMINATED;

                opal_pointer_array_add(jdata->procs, pdata);
                jdata->num_procs++;

                return pdata->name.epoch;
            } else {
                return ORTE_EPOCH_MIN;
            }
        }
    }

    /* Didn't find the right job, add a new job structure and a new process. */
    if (ORTE_EPOCH_INVALID != epoch) {
        jdata = OBJ_NEW(orte_job_t);
        jdata->jobid = proc->jobid;

        pdata = OBJ_NEW(orte_proc_t);
        pdata->name.jobid = proc->jobid;
        pdata->name.vpid = proc->vpid;
        pdata->name.epoch = epoch;

        pdata->state = ORTE_PROC_STATE_TERMINATED;

        opal_pointer_array_add(jdata->procs, pdata);
        jdata->num_procs++;

        opal_pointer_array_add(orte_job_data, jdata);

        return pdata->name.epoch;
    } else {
        return ORTE_EPOCH_MIN;
    }
}
#endif

