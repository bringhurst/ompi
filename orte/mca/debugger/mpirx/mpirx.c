/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
 * Copyright (c) 2010-2011 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif  /* HAVE_STDLIB_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif  /* HAVE_STRINGS_H */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/path.h"
#include "opal/util/os_path.h"
#include "opal/util/opal_sos.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/util/opal_getcwd.h"
#include "opal/mca/event/event.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/rmaps/rmaps_types.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/show_help.h"
#include "orte/util/name_fns.h"

#include "orte/mca/debugger/base/base.h"
#include "mpirx.h"

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

/* Static API's */
static int init(void);
static void finalize(void);
static void init_before_spawn(orte_job_t *jdata);

/* Module definition */
orte_debugger_base_module_t orte_debugger_mpirx_module = {
    init,
    finalize,
    init_before_spawn,
    orte_debugger_base_init_after_spawn
};

/* local globals and functions */
static void attach_debugger(int fd, short event, void *arg);
static void build_debugger_args(orte_app_context_t *debugger);
static void open_fifo(void);
static opal_event_t attach;
static int attach_fd = -1;
static bool fifo_active=false;

static int init(void)
{
    return ORTE_SUCCESS;
}

/**
 * Release resources associated with data structures for running under
 * a debugger using the MPICH/TotalView parallel debugger interface.
 */
void finalize(void)
{
    if (fifo_active) {
        opal_event_del(&attach);
        close(attach_fd);
    }

    if (MPIR_proctable) {
        free(MPIR_proctable);
        MPIR_proctable = NULL;
    }
}

/**
 * Initialization of data structures for running under a debugger
 * using an extended MPICH/TotalView parallel debugger interface.  Before the
 * spawn we need to check if we are being run under a TotalView-like
 * debugger; if so then inform applications via an MCA parameter.
 */
void init_before_spawn(orte_job_t *jdata)
{
    char *env_name;
    orte_app_context_t *app;
    int i;
    int32_t ljob;
    char *attach_fifo;

    if (!MPIR_being_debugged && !orte_in_parallel_debugger) {
        /* if we were given a test debugger, then we still want to
         * colaunch it
         */
        if (NULL != orte_debugger_base.test_daemon) {
            opal_output_verbose(2, orte_debugger_base.output,
                                "%s No debugger test daemon specified",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            goto launchit;
        }
        /* if we were given an auto-detect rate, then we want to setup
         * an event so we periodically do the check
         */
        if (0 < orte_debugger_mpirx_check_rate) {
            opal_output_verbose(2, orte_debugger_base.output,
                                "%s Setting debugger attach check rate for %d seconds",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                orte_debugger_mpirx_check_rate);
            ORTE_TIMER_EVENT(orte_debugger_mpirx_check_rate, 0, attach_debugger);
        } else {
            /* create the attachment FIFO and put it into MPIR, setup readevent */
            /* create a FIFO name in the session dir */
            attach_fifo = opal_os_path(false, orte_process_info.job_session_dir, "debugger_attach_fifo", NULL);
            if ((mkfifo(attach_fifo, FILE_MODE) < 0) && errno != EEXIST) {
                opal_output(0, "CANNOT CREATE FIFO %s: errno %d", attach_fifo, errno);
                free(attach_fifo);
                return;
            }
            strncpy(MPIR_attach_fifo, attach_fifo, MPIR_MAX_PATH_LENGTH - 1);
	    free (attach_fifo);
	    open_fifo ();
        }
        return;
    }
    
 launchit:
    opal_output_verbose(2, orte_debugger_base.output,
                        "%s: Spawned by a debugger",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* tell the procs they are being debugged */
    env_name = mca_base_param_environ_variable("orte", 
                                               "in_parallel_debugger", NULL);
    
    for (i=0; i < jdata->apps->size; i++) {
        if (NULL == (app = (orte_app_context_t*)opal_pointer_array_get_item(jdata->apps, i))) {
            continue;
        }
        opal_setenv(env_name, "1", true, &app->env);
    }
    free(env_name);

    /* check if we need to co-spawn the debugger daemons */
    if ('\0' != MPIR_executable_path[0] || NULL != orte_debugger_base.test_daemon) {
        /* can only have one debugger */
        if (NULL != orte_debugger_daemon) {
            opal_output(0, "-------------------------------------------\n"
                        "Only one debugger can be used on a job.\n"
                        "-------------------------------------------\n");
            ORTE_UPDATE_EXIT_STATUS(ORTE_ERROR_DEFAULT_EXIT_CODE);
            return;
        }
        opal_output_verbose(2, orte_debugger_base.output,
                            "%s Cospawning debugger daemons %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            (NULL == orte_debugger_base.test_daemon) ?
                            MPIR_executable_path : orte_debugger_base.test_daemon);
        /* add debugger info to launch message */
        orte_debugger_daemon = OBJ_NEW(orte_job_t);
        /* create a jobid for these daemons - this is done solely
         * to avoid confusing the rest of the system's bookkeeping
         */
        orte_plm_base_create_jobid(orte_debugger_daemon);
        /* flag the job as being debugger daemons */
        orte_debugger_daemon->controls |= ORTE_JOB_CONTROL_DEBUGGER_DAEMON;
        /* unless directed, we do not forward output */
        if (!MPIR_forward_output) {
            orte_debugger_daemon->controls &= ~ORTE_JOB_CONTROL_FORWARD_OUTPUT;
        }
        /* add it to the global job pool */
        ljob = ORTE_LOCAL_JOBID(orte_debugger_daemon->jobid);
        opal_pointer_array_set_item(orte_job_data, ljob, orte_debugger_daemon);
        /* create an app_context for the debugger daemon */
        app = OBJ_NEW(orte_app_context_t);
        if (NULL != orte_debugger_base.test_daemon) {
            app->app = strdup(orte_debugger_base.test_daemon);
        } else {
            app->app = strdup((char*)MPIR_executable_path);
        }
        opal_argv_append_nosize(&app->argv, app->app);
        build_debugger_args(app);
        opal_pointer_array_add(orte_debugger_daemon->apps, app);
        orte_debugger_daemon->num_apps = 1;
    }
    return;
}

static void open_fifo (void)
{
    if (attach_fd > 0) {
	close(attach_fd);
    }

    attach_fd = open(MPIR_attach_fifo, O_RDONLY | O_NONBLOCK, 0);
    if (attach_fd < 0) {
	opal_output(0, "%s unable to open debugger attach fifo",
		    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
	return;
    }
    opal_output_verbose(2, orte_debugger_base.output,
			"%s Monitoring debugger attach fifo %s",
			ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
			MPIR_attach_fifo);
    opal_event_set(opal_event_base, &attach, attach_fd, OPAL_EV_READ, attach_debugger, NULL);

    fifo_active = true;
    opal_event_add(&attach, 0);
}

static void attach_debugger(int fd, short event, void *arg)
{
    orte_app_context_t *app;
    unsigned char fifo_cmd;
    int rc;
    int32_t ljob;
    orte_job_t *jdata;

    /* read the file descriptor to clear that event, if necessary */
    if (fifo_active) {
	opal_event_del(&attach);
	fifo_active = false;

        rc = read(attach_fd, &fifo_cmd, sizeof(fifo_cmd));
	if (!rc) {
	    /* reopen device to clear hangup */
	    open_fifo();
	    return;
	}
        if (1 != fifo_cmd) {
            /* ignore the cmd */
            goto RELEASE;
        }
    }

    if (!MPIR_being_debugged && !orte_debugger_base.test_attach) {
        /* false alarm */
        goto RELEASE;
    }

    opal_output_verbose(1, orte_debugger_base.output,
                        "%s Attaching debugger %s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        (NULL == orte_debugger_base.test_daemon) ? MPIR_executable_path : orte_debugger_base.test_daemon);

    /* a debugger has attached! All the MPIR_Proctable
     * data is already available, so we only need to
     * check to see if we should spawn any daemons
     */
    if ('\0' != MPIR_executable_path[0] || NULL != orte_debugger_base.test_daemon) {
        /* can only have one debugger */
        if (NULL != orte_debugger_daemon) {
            opal_output(0, "-------------------------------------------\n"
                        "Only one debugger can be used on a job.\n"
                        "-------------------------------------------\n");
            goto RELEASE;
        }
        opal_output_verbose(2, orte_debugger_base.output,
                            "%s Spawning debugger daemons %s",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            (NULL == orte_debugger_base.test_daemon) ?
                            MPIR_executable_path : orte_debugger_base.test_daemon);
        /* this will be launched just like a regular job,
         * so we do not use the global orte_debugger_daemon
         * as this is reserved for co-location upon startup
         */
        jdata = OBJ_NEW(orte_job_t);
        /* create a jobid for these daemons - this is done solely
         * to avoid confusing the rest of the system's bookkeeping
         */
        orte_plm_base_create_jobid(jdata);
        /* flag the job as being debugger daemons */
        jdata->controls |= ORTE_JOB_CONTROL_DEBUGGER_DAEMON;
        /* unless directed, we do not forward output */
        if (!MPIR_forward_output) {
            jdata->controls &= ~ORTE_JOB_CONTROL_FORWARD_OUTPUT;
        }
        /* add it to the global job pool */
        ljob = ORTE_LOCAL_JOBID(jdata->jobid);
        opal_pointer_array_set_item(orte_job_data, ljob, jdata);
        /* create an app_context for the debugger daemon */
        app = OBJ_NEW(orte_app_context_t);
        if (NULL != orte_debugger_base.test_daemon) {
            app->app = strdup(orte_debugger_base.test_daemon);
        } else {
            app->app = strdup((char*)MPIR_executable_path);
        }

	jdata->state = ORTE_JOB_STATE_INIT;

        opal_argv_append_nosize(&app->argv, app->app);
        build_debugger_args(app);
        opal_pointer_array_add(jdata->apps, app);
        jdata->num_apps = 1;
        /* setup the mapping policy to bynode so we get one
         * daemon on each node
         */
        jdata->map = OBJ_NEW(orte_job_map_t);
        jdata->map->policy = ORTE_MAPPING_BYNODE;
	jdata->map->npernode = 1;
        /* now go ahead and spawn this job */
        if (ORTE_SUCCESS != (rc = orte_plm.spawn(jdata))) {
            ORTE_ERROR_LOG(rc);
        }
    }
        
 RELEASE:
    /* reset the read or timer event */
    if (0 == orte_debugger_mpirx_check_rate) {
        fifo_active = true;
        opal_event_add(&attach, 0);
    } else if (!MPIR_being_debugged) {
	ORTE_TIMER_EVENT(orte_debugger_mpirx_check_rate, 0, attach_debugger);
    }

    /* notify the debugger that all is ready */
    MPIR_Breakpoint();
}

static void build_debugger_args(orte_app_context_t *debugger)
{
    int i, j;
    char mpir_arg[MPIR_MAX_ARG_LENGTH];

    if ('\0' != MPIR_server_arguments[0]) {
        j=0;
        memset(mpir_arg, 0, MPIR_MAX_ARG_LENGTH);
        for (i=0; i < MPIR_MAX_ARG_LENGTH; i++) {
            if (MPIR_server_arguments[i] == '\0') {
                if (0 < j) {
                    opal_argv_append_nosize(&debugger->argv, mpir_arg);
                    memset(mpir_arg, 0, MPIR_MAX_ARG_LENGTH);
                    j=0;
                }
            } else {
                mpir_arg[j] = MPIR_server_arguments[i];
                j++;
            }
        }
    }
}
