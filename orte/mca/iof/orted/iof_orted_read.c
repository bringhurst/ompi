/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */

#include "orte/util/show_help.h"

#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/iof/iof.h"
#include "orte/mca/iof/base/base.h"

#include "iof_orted.h"

/*
 * Callback when non-blocking RML send completes.
 */
static void send_cb(int status, orte_process_name_t *peer,
                    opal_buffer_t *buf, orte_rml_tag_t tag,
                    void *cbdata)
{
    /* nothing to do here - just release buffer and return */
    OBJ_RELEASE(buf);
}


void orte_iof_orted_read_handler(int fd, short event, void *cbdata)
{
    orte_iof_read_event_t *rev = (orte_iof_read_event_t*)cbdata;
    unsigned char data[ORTE_IOF_BASE_MSG_MAX];
    opal_buffer_t *buf=NULL;
    int rc;
    int32_t numbytes;
    
    OPAL_THREAD_LOCK(&mca_iof_orted_component.lock);
    
    /* read up to the fragment size */
#if !defined(__WINDOWS__)
    numbytes = read(fd, data, sizeof(data));
#else
    {
        DWORD readed;
        HANDLE handle = (HANDLE)_get_osfhandle(fd);
        ReadFile(handle, data, sizeof(data), &readed, NULL);
        numbytes = (int)readed;
    }
#endif  /* !defined(__WINDOWS__) */
    
    if (numbytes < 0) {
        /* either we have a connection error or it was a non-blocking read */
        
        /* non-blocking, retry */
        if (EAGAIN == errno || EINTR == errno) {
            OPAL_THREAD_UNLOCK(&mca_iof_orted_component.lock);
            return;
        } 

        OPAL_OUTPUT_VERBOSE((1, orte_iof_base.iof_output,
                             "%s iof:orted:read handler %s Error on connection:%d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&rev->name), fd));
        
        
        goto CLEAN_RETURN;
    } else if (0 == numbytes) {
        /* child process closed connection - close the fd */
        close(fd);
        goto CLEAN_RETURN;
    }
    
    OPAL_OUTPUT_VERBOSE((1, orte_iof_base.iof_output,
                         "%s iof:orted:read handler %s %d bytes from fd %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&rev->name),
                         numbytes, fd));
    
    /* prep the buffer */
    buf = OBJ_NEW(opal_buffer_t);
    
    /* pack the stream first - we do this so that flow control messages can
     * consist solely of the tag
     */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(buf, &rev->tag, 1, ORTE_IOF_TAG))) {
        ORTE_ERROR_LOG(rc);
        goto CLEAN_RETURN;
    }
    
    /* pack name of process that gave us this data */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(buf, &rev->name, 1, ORTE_NAME))) {
        ORTE_ERROR_LOG(rc);
        goto CLEAN_RETURN;
    }
    
    /* pack the data - only pack the #bytes we read! */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(buf, &data, numbytes, OPAL_BYTE))) {
        ORTE_ERROR_LOG(rc);
        goto CLEAN_RETURN;
    }

    /* start non-blocking RML call to forward received data */
    OPAL_OUTPUT_VERBOSE((1, orte_iof_base.iof_output,
                         "%s iof:orted:read handler sending %d bytes to HNP",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), numbytes));
    
    orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buf, ORTE_RML_TAG_IOF_HNP,
                            0, send_cb, NULL);
    
    OPAL_THREAD_UNLOCK(&mca_iof_orted_component.lock);
    
    /* since the event is persistent, we do not need to re-add it */
    return;
   
CLEAN_RETURN:
    /* delete the event from the event library */
    opal_event_del(&rev->ev);
    if (NULL != buf) {
        OBJ_RELEASE(buf);
    }
    OPAL_THREAD_UNLOCK(&mca_iof_orted_component.lock);
    return;
}