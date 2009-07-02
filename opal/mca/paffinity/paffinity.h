/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * paffinity (processor affinity) framework component interface
 * definitions.
 *
 * Intent
 *
 * This is an extremely simple framework that is used to support the
 * OS-specific API for placement of processes on processors.  It does
 * *not* decide scheduling issues -- it is simply for assigning the
 * current process it to a specific processor set.  As such, the
 * components are likely to be extremely short/simple -- there will
 * likely be one component for each OS/API that we support (e.g.,
 * Linux, IRIX, etc.).  As a direct consequence, there will likely
 * only be one component that is useable on a given platform (making
 * selection easy).
 *
 * It is *not* an error if there is no paffinity component available;
 * processor affinity services are simply not available.  Hence,
 * paffinity component functions are invoked through short wrapper
 * functions in paffinity/base (that check to see if there is a
 * selected component before invoking function pointers).  If there is
 * no selected component, they return an appropriate error code.
 *
 * In the paffinity interface, we make the distinction between LOGICAL
 * and PHYSICAL processors.  LOGICAL processors are defined to have
 * some corresponding PHYSICAL processor that both exists and is
 * currently online.  LOGICAL processors numbered countiguously
 * starting with 0.  PHYSICAL processors are numbered according to the
 * underlying operating system; they are represented by integers, but
 * no guarantees are made about their values.
 *
 * Hence, LOGICAL processor IDs are convenient for humans and are in
 * the range of [0,N-1] (assuming N processors are currently online).
 * Each LOGICAL processor has a 1:1 relationship with a PHYSICAL
 * processor, but the PHYSICAL processor's ID can be any unique
 * integer value.
 * 
 * ***NOTE*** Obtaining information about socket/core IDs is not well
 * supported in many OS's.  Programmers using this paffinity interface
 * should fully expect to sometimes get OPAL_ERR_NOT_SUPPORTED back
 * when calling such functions.

 * General scheme
 *
 * The component has one function: query().  It simply returns a
 * priority (for the unlikely event where there are multiple
 * components available on a given platform).
 *
 * The module has the following functions:
 *
 * - module_init: initialze the module
 * - set: set this process's affinity to a specific processor set
 * - get: get this process's processor affinity set
 * - map physical (socket ID, core ID) -> physical processor ID
 * - map physical processor ID -> physical (socket ID, core ID)
 * - get the number of logical processors
 * - get the number of logical sockets
 * - get the number of logical cores on a specific socket
 * - map logical processor ID -> physical processor ID
 * - map logical socket ID -> physical socket ID
 * - map physical socket ID, logical core ID -> physical core ID
 * - module_finalize: finalize the module
 */

#ifndef OPAL_PAFFINITY_H
#define OPAL_PAFFINITY_H

#include "opal_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

/* ******************************************************************** */

/** Process locality definitions */
#define OPAL_PROC_ON_CLUSTER    0x10
#define OPAL_PROC_ON_CU         0x08
#define OPAL_PROC_ON_NODE       0x04
#define OPAL_PROC_ON_BOARD      0x02
#define OPAL_PROC_ON_SOCKET     0x01
#define OPAL_PROC_NON_LOCAL     0x00
#define OPAL_PROC_ALL_LOCAL     0x1f

/** Process locality macros */
#define OPAL_PROC_ON_LOCAL_SOCKET(n)    ((n) & OPAL_PROC_ON_SOCKET)
#define OPAL_PROC_ON_LOCAL_BOARD(n)     ((n) & OPAL_PROC_ON_BOARD)
#define OPAL_PROC_ON_LOCAL_NODE(n)      ((n) & OPAL_PROC_ON_NODE)
#define OPAL_PROC_ON_LOCAL_CU(n)        ((n) & OPAL_PROC_ON_CU)
#define OPAL_PROC_ON_LOCAL_CLUSTER(n)   ((n) & OPAL_PROC_ON_CLUSTER)

/* ******************************************************************** */


/**
 * Buffer type for paffinity processor masks.
 * Copied almost directly from PLPA.
 */

/**
 * \internal
 * Internal type used for the underlying bitmask unit
 */
typedef unsigned long int opal_paffinity_base_bitmask_t;

/**
 * \internal
 * Number of bits in opal_paffinity_base_bitmask_t
 */
#define OPAL_PAFFINITY_BITMASK_T_NUM_BITS (sizeof(opal_paffinity_base_bitmask_t) * 8)
/**
 * \internal
 * How many bits we want
 */
#define OPAL_PAFFINITY_BITMASK_CPU_MAX 1024
/**
 * \internal
 * How many opal_paffinity_base_bitmask_t's we need
 */
#define OPAL_PAFFINITY_BITMASK_NUM_ELEMENTS (OPAL_PAFFINITY_BITMASK_CPU_MAX / OPAL_PAFFINITY_BITMASK_T_NUM_BITS)

/**
 * Public processor bitmask type
 */
typedef struct opal_paffinity_base_cpu_set_t { 
    opal_paffinity_base_bitmask_t bitmask[OPAL_PAFFINITY_BITMASK_NUM_ELEMENTS];
} opal_paffinity_base_cpu_set_t;

/***************************************************************************/

/**
 * \internal
 * Internal macro for identifying the byte in a bitmask array 
 */
#define OPAL_PAFFINITY_CPU_BYTE(num) ((num) / OPAL_PAFFINITY_BITMASK_T_NUM_BITS)

/**
 * \internal
 * Internal macro for identifying the bit in a bitmask array 
 */
#define OPAL_PAFFINITY_CPU_BIT(num) ((num) % OPAL_PAFFINITY_BITMASK_T_NUM_BITS)

/***************************************************************************/

/**
 * Public macro to zero out a OPAL_PAFFINITY cpu set 
 */
#define OPAL_PAFFINITY_CPU_ZERO(cpuset) \
    memset(&(cpuset), 0, sizeof(opal_paffinity_base_cpu_set_t))

/**
 * Public macro to set a bit in a OPAL_PAFFINITY cpu set 
 */
#define OPAL_PAFFINITY_CPU_SET(num, cpuset) \
    (cpuset).bitmask[OPAL_PAFFINITY_CPU_BYTE(num)] |= ((opal_paffinity_base_bitmask_t) 1 << OPAL_PAFFINITY_CPU_BIT(num))

/**
 * Public macro to clear a bit in a OPAL_PAFFINITY cpu set 
 */
#define OPAL_PAFFINITY_CPU_CLR(num, cpuset) \
    (cpuset).bitmask[OPAL_PAFFINITY_CPU_BYTE(num)] &= ~((opal_paffinity_base_bitmask_t) 1 << OPAL_PAFFINITY_CPU_BIT(num))

/**
 * Public macro to test if a bit is set in a OPAL_PAFFINITY cpu set 
 */
#define OPAL_PAFFINITY_CPU_ISSET(num, cpuset) \
    (0 != (((cpuset).bitmask[OPAL_PAFFINITY_CPU_BYTE(num)]) & ((opal_paffinity_base_bitmask_t) 1 << OPAL_PAFFINITY_CPU_BIT(num))))

/**
 * Public macro to test if a process is bound anywhere
 */
#define OPAL_PAFFINITY_PROCESS_IS_BOUND(cpuset, bound)              \
    do {                                                            \
        int i, num_processors, num_bound;                           \
        opal_paffinity_base_get_processor_info(&num_processors);    \
        *(bound) = false;                                           \
        num_bound = 0;                                              \
        for (i=0; i < num_processors; i++) {                        \
            if (OPAL_PAFFINITY_CPU_ISSET(i, (cpuset))) {            \
                num_bound++;                                        \
            }                                                       \
        }                                                           \
        if (num_bound < num_processors) {                           \
            *(bound) = true;                                        \
        }                                                           \
    } while(0);

/***************************************************************************/

/**
 * Module initialization function.  Should return OPAL_SUCCESS.
 */
typedef int (*opal_paffinity_base_module_init_1_1_0_fn_t)(void);

/**
 * Module function to set this process' affinity to a specific set of
 * PHYSICAL CPUs.
 */
typedef int (*opal_paffinity_base_module_set_fn_t)(opal_paffinity_base_cpu_set_t cpumask);


/**
 * Module function to get this process' affinity to a specific set of
 * PHYSICAL CPUs.  Returns any binding in the cpumask. This function -only-
 * returns something other than OPAL_SUCCESS if an actual error is encountered.
 * You will need to check the mask to find out if this process is actually
 * bound somewhere specific - a macro for that purpose is provided above
 */
typedef int (*opal_paffinity_base_module_get_fn_t)(opal_paffinity_base_cpu_set_t *cpumask);

/**
 * Returns mapping of PHYSICAL socket:core -> PHYSICAL processor id.
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supported
 */
typedef int (*opal_paffinity_base_module_get_map_to_processor_id_fn_t)(int physical_socket,
                                                                       int physical_core,
                                                                       int *physical_processor_id);

/**
 * Provides mapping of PHYSICAL processor id -> PHYSICAL socket:core.
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supported
 */
typedef int (*opal_paffinity_base_module_get_map_to_socket_core_fn_t)(int physical_processor_id,
                                                                      int *physical_socket,
                                                                      int *physical_core);

/**
 * Provides number of LOGICAL processors in a host.
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supported
 */
typedef int (*opal_paffinity_base_module_get_processor_info_fn_t)(int *num_processors);

/**
 * Provides the number of LOGICAL sockets in a host.
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supported
 */
typedef int (*opal_paffinity_base_module_get_socket_info_fn_t)(int *num_sockets);

/**
 * Provides the number of LOGICAL cores in a PHYSICAL socket. currently supported
 * only in Linux hosts
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supporeted (solaris, windows, etc...)
 */
typedef int (*opal_paffinity_base_module_get_core_info_fn_t)(int physical_socket, int *num_cores);

/**
 * Return the PHYSICAL processor id that corresponds to the
 * given LOGICAL processor id
 *
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supporeted (solaris, windows, etc...)
 */
typedef int (*opal_paffinity_base_module_get_physical_processor_id_fn_t)(int logical_processor_id);

/**
 * Return the PHYSICAL socket id that corresponds to the given
 * LOGICAL socket id
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supporeted (solaris, windows, etc...)
 */
typedef int (*opal_paffinity_base_module_get_physical_socket_id_fn_t)(int logical_socket_id);

/**
 * Return the PHYSICAL core id that corresponds to the given LOGICAL
 * core id on the given PHYSICAL socket id
 * 
 * return OPAL_SUCCESS or OPAL_ERR_NOT_SUPPORTED if not
 * supporeted (solaris, windows, etc...)
 */
typedef int (*opal_paffinity_base_module_get_physical_core_id_fn_t)(int physical_socket_id, int logical_core_id);


/**
 * Module finalize function.  Invoked by the base on the selected
 * module when the paffinity framework is being shut down.
 */
typedef int (*opal_paffinity_base_module_finalize_fn_t)(void);


/**
 * Structure for paffinity components.
 */
struct opal_paffinity_base_component_2_0_0_t {
    /** MCA base component */
    mca_base_component_t base_version;
    /** MCA base data */
    mca_base_component_data_t base_data;
};
/**
 * Convenience typedef
 */
typedef struct opal_paffinity_base_component_2_0_0_t opal_paffinity_base_component_2_0_0_t;
typedef struct opal_paffinity_base_component_2_0_0_t opal_paffinity_base_component_t;


/**
 * Structure for paffinity modules
 */
struct opal_paffinity_base_module_1_1_0_t {
    /** Module initialization function */
    opal_paffinity_base_module_init_1_1_0_fn_t                  paff_module_init;

    /** Set this process' affinity */
    opal_paffinity_base_module_set_fn_t                         paff_module_set;

    /** Get this process' affinity */
    opal_paffinity_base_module_get_fn_t                         paff_module_get;

    /** Map socket:core to processor ID */
    opal_paffinity_base_module_get_map_to_processor_id_fn_t     paff_get_map_to_processor_id;

    /** Map processor ID to socket:core */
    opal_paffinity_base_module_get_map_to_socket_core_fn_t      paff_get_map_to_socket_core;

    /** Return the max processor ID */
    opal_paffinity_base_module_get_processor_info_fn_t          paff_get_processor_info;

    /** Return the max socket number */
    opal_paffinity_base_module_get_socket_info_fn_t             paff_get_socket_info;

    /** Return the max core number */
    opal_paffinity_base_module_get_core_info_fn_t               paff_get_core_info;

    /* Return physical processor id */
    opal_paffinity_base_module_get_physical_processor_id_fn_t   paff_get_physical_processor_id;
    
    /* Return physical socket id */
    opal_paffinity_base_module_get_physical_socket_id_fn_t      paff_get_physical_socket_id;
    
    /* Return physical core id */
    opal_paffinity_base_module_get_physical_core_id_fn_t        paff_get_physical_core_id;
    
    /** Shut down this module */
    opal_paffinity_base_module_finalize_fn_t                    paff_module_finalize;
};
/**
 * Convenience typedef
 */
typedef struct opal_paffinity_base_module_1_1_0_t opal_paffinity_base_module_1_1_0_t;
typedef struct opal_paffinity_base_module_1_1_0_t opal_paffinity_base_module_t;


/*
 * Macro for use in components that are of type paffinity
 */
#define OPAL_PAFFINITY_BASE_VERSION_2_0_0 \
    MCA_BASE_VERSION_2_0_0, \
    "paffinity", 2, 0, 0

#endif /* OPAL_PAFFINITY_H */
