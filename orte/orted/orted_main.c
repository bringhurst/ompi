/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2009      Institut National de Recherche en Informatique
 *                         et Automatique. All rights reserved.
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved. 
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */


#include "opal/mca/event/event.h"
#include "opal/mca/base/base.h"
#include "opal/util/output.h"
#include "opal/util/cmd_line.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_path.h"
#include "opal/util/printf.h"
#include "opal/util/argv.h"
#include "opal/runtime/opal.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/util/daemon_init.h"
#include "opal/dss/dss.h"
#include "opal/mca/hwloc/hwloc.h"

#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/mca/rml/base/rml_contact.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/odls/odls.h"
#include "orte/mca/odls/base/odls_private.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/ras/ras.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/rmaps/rmaps_types.h"

/* need access to the create_jobid fn used by plm components
* so we can set singleton name, if necessary
*/
#include "orte/mca/plm/base/plm_private.h"

#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_locks.h"
#include "orte/runtime/orte_quit.h"

#include "orte/orted/orted.h"

/*
 * Globals
 */
static opal_event_t pipe_handler;
static void shutdown_callback(int fd, short flags, void *arg);

static struct {
    bool debug;
    bool help;
    bool set_sid;
    bool hnp;
    bool daemonize;
    char* name;
    char* vpid_start;
    char* num_procs;
    int uri_pipe;
    int singleton_died_pipe;
    int fail;
    int fail_delay;
    bool abort;
} orted_globals;

/*
 * define the orted context table for obtaining parameters
 */
opal_cmd_line_init_t orte_cmd_line_opts[] = {
    /* Various "obvious" options */
    { NULL, NULL, NULL, 'h', NULL, "help", 0,
      &orted_globals.help, OPAL_CMD_LINE_TYPE_BOOL,
      "This help message" },

    { "orte", "daemon_spin", NULL, 's', NULL, "spin", 0,
      &orted_spin_flag, OPAL_CMD_LINE_TYPE_BOOL,
      "Have the orted spin until we can connect a debugger to it" },

    { NULL, NULL, NULL, '\0', NULL, "debug-failure", 1,
      &orted_globals.fail, OPAL_CMD_LINE_TYPE_INT,
      "Have the specified orted fail after init for debugging purposes" },
    
    { NULL, NULL, NULL, '\0', NULL, "debug-failure-delay", 1,
      &orted_globals.fail_delay, OPAL_CMD_LINE_TYPE_INT,
      "Have the orted specified for failure delay for the provided number of seconds before failing" },
    
    { "orte", "debug", NULL, 'd', NULL, "debug", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Debug the OpenRTE" },
        
    { "orte", "daemonize", NULL, '\0', NULL, "daemonize", 0,
      &orted_globals.daemonize, OPAL_CMD_LINE_TYPE_BOOL,
      "Daemonize the orted into the background" },

    { "orte", "debug", "daemons", '\0', NULL, "debug-daemons", 0,
      &orted_globals.debug, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE daemons" },

    { "orte", "debug", "daemons_file", '\0', NULL, "debug-daemons-file", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Enable debugging of OpenRTE daemons, storing output in files" },

    { NULL, NULL, NULL, '\0', NULL, "hnp", 0,
      &orted_globals.hnp, OPAL_CMD_LINE_TYPE_BOOL,
      "Direct the orted to act as the HNP"},

    { "orte", "hnp", "uri", '\0', NULL, "hnp-uri", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the HNP"},
    
    { "orte", "parent", "uri", '\0', NULL, "parent-uri", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "URI for the parent if tree launch is enabled."},
    
    { NULL, NULL, NULL, '\0', NULL, "set-sid", 0,
      &orted_globals.set_sid, OPAL_CMD_LINE_TYPE_BOOL,
      "Direct the orted to separate from the current session"},
    
    { "tmpdir", "base", NULL, '\0', NULL, "tmpdir", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Set the root for the session directory tree" },

    { NULL, NULL, NULL, '\0', NULL, "report-uri", 1,
      &orted_globals.uri_pipe, OPAL_CMD_LINE_TYPE_INT,
      "Report this process' uri on indicated pipe"},

    { NULL, NULL, NULL, '\0', NULL, "singleton-died-pipe", 1,
      &orted_globals.singleton_died_pipe, OPAL_CMD_LINE_TYPE_INT,
      "Watch on indicated pipe for singleton termination"},
    
    { "orte", "output", "filename", '\0', "output-filename", "output-filename", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Redirect output from application processes into filename.rank" },
    
    { "orte", "xterm", NULL, '\0', "xterm", "xterm", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Create a new xterm window and display output from the specified ranks there" },

    { "orte", "report", "bindings", '\0', "report-bindings", "report-bindings", 0,
      NULL, OPAL_CMD_LINE_TYPE_BOOL,
      "Whether to report process bindings to stderr" },

    { "orte", "node", "regex", '\0', "nodes", "nodes", 1,
      NULL, OPAL_CMD_LINE_TYPE_STRING,
      "Regular expression defining nodes in system" },

    /* End of list */
    { NULL, NULL, NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int orte_daemon(int argc, char *argv[])
{
    int ret = 0;
    opal_cmd_line_t *cmd_line = NULL;
    char *rml_uri;
    int i;
    opal_buffer_t *buffer;
    char hostname[100];
    char *tmp_env_var = NULL;
    struct timeval starttime, setuptime;
    
    /* get our time for first executable */
    gettimeofday(&starttime, NULL);
    
    /* initialize the globals */
    memset(&orted_globals, 0, sizeof(orted_globals));
    /* initialize the singleton died pipe to an illegal value so we can detect it was set */
    orted_globals.singleton_died_pipe = -1;
    /* init the failure orted vpid to an invalid value */
    orted_globals.fail = ORTE_VPID_INVALID;
    
    /* setup to check common command line options that just report and die */
    cmd_line = OBJ_NEW(opal_cmd_line_t);
    if (OPAL_SUCCESS != opal_cmd_line_create(cmd_line, orte_cmd_line_opts)) {
        OBJ_RELEASE(cmd_line);
        exit(1);
    }
    mca_base_cmd_line_setup(cmd_line);
    if (ORTE_SUCCESS != (ret = opal_cmd_line_parse(cmd_line, false,
                                                   argc, argv))) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        fprintf(stderr, "Usage: %s [OPTION]...\n%s\n", argv[0], args);
        free(args);
        OBJ_RELEASE(cmd_line);
        return ret;
    }
    
    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(cmd_line, &environ, &environ);
    
    /* Ensure that enough of OPAL is setup for us to be able to run */
    /*
     * NOTE: (JJH)
     *  We need to allow 'mca_base_cmd_line_process_args()' to process command
     *  line arguments *before* calling opal_init_util() since the command
     *  line could contain MCA parameters that affect the way opal_init_util()
     *  functions. AMCA parameters are one such option normally received on the
     *  command line that affect the way opal_init_util() behaves.
     *  It is "safe" to call mca_base_cmd_line_process_args() before 
     *  opal_init_util() since mca_base_cmd_line_process_args() does *not*
     *  depend upon opal_init_util() functionality.
     */
    if (OPAL_SUCCESS != opal_init_util(&argc, &argv)) {
        fprintf(stderr, "OPAL failed to initialize -- orted aborting\n");
        exit(1);
    }

    /* save the environment for launch purposes. This MUST be
     * done so that we can pass it to any local procs we
     * spawn - otherwise, those local procs won't see any
     * non-MCA envars that were set in the enviro when the
     * orted was executed - e.g., by .csh
     */
    orte_launch_environ = opal_argv_copy(environ);
    
    /* purge any ess flag set in the environ when we were launched */
    opal_unsetenv("OMPI_MCA_ess", &orte_launch_environ);
    
    /* if orte_daemon_debug is set, let someone know we are alive right
     * away just in case we have a problem along the way
     */
    if (orted_globals.debug) {
        gethostname(hostname, 100);
        fprintf(stderr, "Daemon was launched on %s - beginning to initialize\n", hostname);
    }
    
    /* check for help request */
    if (orted_globals.help) {
        char *args = NULL;
        args = opal_cmd_line_get_usage_msg(cmd_line);
        orte_show_help("help-orted.txt", "orted:usage", false,
                       argv[0], args);
        free(args);
        return 1;
    }
#if defined(HAVE_SETSID) && !defined(__WINDOWS__)
    /* see if we were directed to separate from current session */
    if (orted_globals.set_sid) {
        setsid();
    }
#endif  /* !defined(__WINDOWS__) */
    /* see if they want us to spin until they can connect a debugger to us */
    i=0;
    while (orted_spin_flag) {
        i++;
        if (1000 < i) i=0;        
    }

#if OPAL_ENABLE_FT_CR == 1
    /* Mark as a tool program */
    tmp_env_var = mca_base_param_env_var("opal_cr_is_tool");
    opal_setenv(tmp_env_var,
                "1",
                true, &environ);
    free(tmp_env_var);
#endif
    tmp_env_var = NULL; /* Silence compiler warning */

    /* Set the flag telling OpenRTE that I am NOT a
     * singleton, but am "infrastructure" - prevents setting
     * up incorrect infrastructure that only a singleton would
     * require.
     */
    if (orted_globals.hnp) {
        if (ORTE_SUCCESS != (ret = orte_init(&argc, &argv, ORTE_PROC_HNP))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
    } else {
        if (ORTE_SUCCESS != (ret = orte_init(&argc, &argv, ORTE_PROC_DAEMON))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
    }
    /* finalize the OPAL utils. As they are opened again from orte_init->opal_init
     * we continue to have a reference count on them. So we have to finalize them twice...
     */
    opal_finalize_util();

    if ((int)ORTE_VPID_INVALID != orted_globals.fail) {
        orted_globals.abort=false;
        /* some vpid was ordered to fail. The value can be positive
         * or negative, depending upon the desired method for failure,
         * so need to check both here
         */
        if (0 > orted_globals.fail) {
            orted_globals.fail = -1*orted_globals.fail;
            orted_globals.abort = true;
        }
        /* are we the specified vpid? */
        if ((int)ORTE_PROC_MY_NAME->vpid == orted_globals.fail) {
            /* if the user specified we delay, then setup a timer
             * and have it kill us
             */
            if (0 < orted_globals.fail_delay) {
                ORTE_TIMER_EVENT(orted_globals.fail_delay, 0, shutdown_callback);
                
            } else {
                opal_output(0, "%s is executing clean %s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            orted_globals.abort ? "abort" : "abnormal termination");

                /* do -not- call finalize as this will send a message to the HNP
                 * indicating clean termination! Instead, just forcibly cleanup
                 * the local session_dir tree and exit
                 */
                orte_session_dir_cleanup(ORTE_JOBID_WILDCARD);
                
                /* if we were ordered to abort, do so */
                if (orted_globals.abort) {
                    abort();
                }
                
                /* otherwise, return with non-zero status */
                ret = ORTE_ERROR_DEFAULT_EXIT_CODE;
                goto DONE;
            }
        }
    }

    /* detach from controlling terminal
     * otherwise, remain attached so output can get to us
     */
    if(!orte_debug_flag &&
       !orte_debug_daemons_flag &&
       orted_globals.daemonize) {
        opal_daemon_init(NULL);
    }
    
    /* insert our contact info into our process_info struct so we
     * have it for later use and set the local daemon field to our name
     */
    orte_process_info.my_daemon_uri = orte_rml.get_contact_info();
    ORTE_PROC_MY_DAEMON->jobid = ORTE_PROC_MY_NAME->jobid;
    ORTE_PROC_MY_DAEMON->vpid = ORTE_PROC_MY_NAME->vpid;
    ORTE_EPOCH_SET(ORTE_PROC_MY_DAEMON->epoch,ORTE_EPOCH_MIN);
    
    /* if I am also the hnp, then update that contact info field too */
    if (ORTE_PROC_IS_HNP) {
        orte_process_info.my_hnp_uri = orte_rml.get_contact_info();
        ORTE_PROC_MY_HNP->jobid = ORTE_PROC_MY_NAME->jobid;
        ORTE_PROC_MY_HNP->vpid = ORTE_PROC_MY_NAME->vpid;
        ORTE_EPOCH_SET(ORTE_PROC_MY_HNP->epoch,ORTE_EPOCH_MIN);
    }
    
    /* setup the primary daemon command receive function */
    ret = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_DAEMON,
                                  ORTE_RML_NON_PERSISTENT, orte_daemon_recv, NULL);
    if (ret != ORTE_SUCCESS && ret != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(ret);
        goto DONE;
    }
    
    /* output a message indicating we are alive, our name, and our pid
     * for debugging purposes
     */
    if (orte_debug_daemons_flag) {
        fprintf(stderr, "Daemon %s checking in as pid %ld on host %s\n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (long)orte_process_info.pid,
                orte_process_info.nodename);
    }

    /* We actually do *not* want the orted to voluntarily yield() the
       processor more than necessary.  The orted already blocks when
       it is doing nothing, so it doesn't use any more CPU cycles than
       it should; but when it *is* doing something, we do not want it
       to be unnecessarily delayed because it voluntarily yielded the
       processor in the middle of its work.

       For example: when a message arrives at the orted, we want the
       OS to wake up the orted in a timely fashion (which most OS's
       seem good about doing) and then we want the orted to process
       the message as fast as possible.  If the orted yields and lets
       aggressive MPI applications get the processor back, it may be a
       long time before the OS schedules the orted to run again
       (particularly if there is no IO event to wake it up).  Hence,
       routed OOB messages (for example) may be significantly delayed
       before being delivered to MPI processes, which can be
       problematic in some scenarios (e.g., COMM_SPAWN, BTL's that
       require OOB messages for wireup, etc.). */
    opal_progress_set_yield_when_idle(false);

    /* Change the default behavior of libevent such that we want to
       continually block rather than blocking for the default timeout
       and then looping around the progress engine again.  There
       should be nothing in the orted that cannot block in libevent
       until "something" happens (i.e., there's no need to keep
       cycling through progress because the only things that should
       happen will happen in libevent).  This is a minor optimization,
       but what the heck... :-) */
    opal_progress_set_event_flag(OPAL_EVLOOP_ONCE);

    /* if requested, obtain and report a new process name and my uri to the indicated pipe */
    if (orted_globals.uri_pipe > 0) {
        orte_job_t *jdata;
        orte_proc_t *proc;
        orte_node_t **nodes;
        orte_app_context_t *app;
        char *tmp, *nptr, *sysinfo;
        int rc;
        int32_t ljob;

        /* setup the singleton's job */
        jdata = OBJ_NEW(orte_job_t);
        orte_plm_base_create_jobid(jdata);
        ljob = ORTE_LOCAL_JOBID(jdata->jobid);
        opal_pointer_array_set_item(orte_job_data, ljob, jdata);
        
        /* must create a map for it (even though it has no
         * info in it) so that the job info will be picked
         * up in subsequent pidmaps or other daemons won't
         * know how to route
         */
        jdata->map = OBJ_NEW(orte_job_map_t);

        /* setup an app_context for the singleton */
        app = OBJ_NEW(orte_app_context_t);
        app->app = strdup("singleton");
        app->num_procs = 1;
        opal_pointer_array_add(jdata->apps, app);
        
        /* run our local allocator to read the available
         * allocation in case this singleton decides to
         * comm_spawn other procs
         */
        if (ORTE_SUCCESS != (rc = orte_ras.allocate(jdata))) {
            ORTE_ERROR_LOG(rc);
            /* don't quit as this would cause the singleton
             * to hang!
             */
        }
        
        nodes = (orte_node_t**)orte_node_pool->addr;
        
        /* setup a proc object for the singleton - since we
         * -must- be the HNP, and therefore we stored our
         * node on the global node pool, and since the singleton
         * -must- be on the same node as us, indicate that
         */
        proc = OBJ_NEW(orte_proc_t);
        proc->name.jobid = jdata->jobid;
        proc->name.vpid = 0;
        ORTE_EPOCH_SET(proc->name.epoch,ORTE_EPOCH_MIN);

        proc->state = ORTE_PROC_STATE_RUNNING;
        proc->app_idx = 0;
        proc->node = nodes[0]; /* hnp node must be there */
        OBJ_RETAIN(nodes[0]);  /* keep accounting straight */
        opal_pointer_array_add(jdata->procs, proc);
        jdata->num_procs = 1;
        
        /* create a string that contains our uri + the singleton's name + sysinfo */
        orte_util_convert_process_name_to_string(&nptr, &proc->name);
        orte_util_convert_sysinfo_to_string(&sysinfo, orte_local_cpu_type, orte_local_cpu_model);
        asprintf(&tmp, "%s[%s][%s]", orte_process_info.my_daemon_uri, nptr, sysinfo);
        free(nptr);
	free(sysinfo);

        /* pass that info to the singleton */
#ifndef __WINDOWS__
        write(orted_globals.uri_pipe, tmp, strlen(tmp)+1); /* need to add 1 to get the NULL */
#else
        send(orted_globals.uri_pipe, tmp, strlen(tmp)+1, 0); /* need to add 1 to get the NULL */
#endif

        /* cleanup */
        free(tmp);
    }

    /* if we were given a pipe to monitor for singleton termination, set that up */
    if (orted_globals.singleton_died_pipe > 0) {
        /* register shutdown handler */
        opal_event_set(opal_event_base, &pipe_handler,
                       orted_globals.singleton_died_pipe,
                       OPAL_EV_READ|OPAL_EV_PERSIST,
                       shutdown_callback,
                       &orted_globals.singleton_died_pipe);
        opal_event_add(&pipe_handler, NULL);
    }

    /* if we are not the HNP...the only time we will be an HNP
     * is if we are launched by a singleton to provide support
     * for it
     */
    if (!ORTE_PROC_IS_HNP) {
        /* send the information to the orted report-back point - this function
         * will process the data, but also counts the number of
         * orteds that reported back so the launch procedure can continue.
         * We need to do this at the last possible second as the HNP
         * can turn right around and begin issuing orders to us
         */

        buffer = OBJ_NEW(opal_buffer_t);
        /* for now, always include our contact info, even if we are using
         * static ports. Eventually, this will be removed
         */
        rml_uri = orte_rml.get_contact_info();
        if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &rml_uri, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            OBJ_RELEASE(buffer);
            goto DONE;
        }
        if (orte_timing) {
            int64_t secs, usecs;
            /* add our start time */
            secs = starttime.tv_sec;
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &secs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            usecs = starttime.tv_usec;
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &usecs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            /* get and send our setup time */
            gettimeofday(&setuptime, NULL);
            secs = setuptime.tv_sec - starttime.tv_sec;
            if (starttime.tv_usec <= setuptime.tv_usec) {
                usecs = setuptime.tv_usec - starttime.tv_usec;
            } else {
                secs--;
                usecs = 1000000 - starttime.tv_usec + setuptime.tv_usec;
            }
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &secs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &usecs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            /* include the actual timestamp so the HNP can figure out how
             * long it took for this message to arrive
             */
            secs = setuptime.tv_sec;
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &secs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            usecs = setuptime.tv_usec;
            if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &usecs, 1, OPAL_INT64))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
        }

        /* If I have a parent, then first let him know my URI, and only
         * after report back to the HNP.
         */
        mca_base_param_reg_string_name("orte", "parent_uri",
                                       "URI for the parent if tree launch is enabled.",
                                       true, false, NULL,  &rml_uri);
        if (NULL != rml_uri) {
            orte_process_name_t parent;

            /* set the contact info into the hash table */
            if (ORTE_SUCCESS != (ret = orte_rml.set_contact_info(rml_uri))) {
                ORTE_ERROR_LOG(ret);
                free(rml_uri);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            ret = orte_rml_base_parse_uris(rml_uri, &parent, NULL );
            if( ORTE_SUCCESS != ret ) {
                ORTE_ERROR_LOG(ret);
                free(rml_uri);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
            free(rml_uri);

            if( 0 > (ret = orte_rml.send_buffer(&parent, buffer,
                                                ORTE_RML_TAG_ORTED_CALLBACK, 0)) ) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }
        } else {
            /* include our node name */
            opal_dss.pack(buffer, &orte_process_info.nodename, 1, OPAL_STRING);

#if OPAL_HAVE_HWLOC
            /* add the local topology */
            if (NULL != opal_hwloc_topology) {
                if (ORTE_SUCCESS != (ret = opal_dss.pack(buffer, &opal_hwloc_topology, 1, OPAL_HWLOC_TOPO))) {
                    ORTE_ERROR_LOG(ret);
                }
            }
#endif

            /* send to the HNP's callback - this will flow up the routing
             * tree if static ports are enabled
             */
            if (0 > (ret = orte_rml.send_buffer(ORTE_PROC_MY_HNP, buffer,
                                                ORTE_RML_TAG_ORTED_CALLBACK, 0))) {
                ORTE_ERROR_LOG(ret);
                OBJ_RELEASE(buffer);
                goto DONE;
            }            
        }
        OBJ_RELEASE(buffer);  /* done with this */
    }

    if (orte_debug_daemons_flag) {
        opal_output(0, "%s orted: up and running - waiting for commands!", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }

    /* wait to hear we are done */
    opal_event_dispatch(opal_event_base);

    /* should never get here, but if we do... */
 DONE:
    /* Finalize and clean up ourselves */
    orte_quit();
    return ret;
}

static void shutdown_callback(int fd, short flags, void *arg)
{
    if (NULL != arg) {
        /* it's the singleton pipe...  remove that handler */
        opal_event_del(&pipe_handler);
    }
    
    if (orte_debug_daemons_flag) {
        opal_output(0, "%s orted: finalizing", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    
    /* if we were ordered to abort, do so */
    if (orted_globals.abort) {
        opal_output(0, "%s is executing clean abort", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* do -not- call finalize as this will send a message to the HNP
         * indicating clean termination! Instead, just kill our
         * local procs, forcibly cleanup the local session_dir tree, and abort
         */
        orte_odls.kill_local_procs(NULL);
        orte_session_dir_cleanup(ORTE_JOBID_WILDCARD);
        abort();
    } else if ((int)ORTE_PROC_MY_NAME->vpid == orted_globals.fail) {
        opal_output(0, "%s is executing clean abnormal termination", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        /* do -not- call finalize as this will send a message to the HNP
         * indicating clean termination! Instead, just forcibly cleanup
         * the local session_dir tree and exit
         */
        orte_odls.kill_local_procs(NULL);
        orte_session_dir_cleanup(ORTE_JOBID_WILDCARD);
        exit(ORTE_ERROR_DEFAULT_EXIT_CODE);
    }

    orte_quit();
}
