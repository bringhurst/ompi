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

#ifndef OPAL_MCA_TIMER_SOLARIS_TIMER_SOLARIS_H
#define OPAL_MCA_TIMER_SOLARIS_TIMER_SOLARIS_H

#include <mach/mach_time.h>

typedef hrtime_t opal_timer_t;


static inline opal_timer_t
opal_timer_base_get_cycles()
{
    return 0;
}

static inline opal_timer_t
opal_timer_base_get_usec()
{
    /* gethrtime returns nanoseconds */
    return gethrtime() / 1000;
}    

static inline opal_timer_t
opal_timer_base_get_freq()
{
    return 0;
}


#define OPAL_TIMER_CYCLE_NATIVE 0
#define OPAL_TIMER_CYCLE_SUPPORTED 0
#define OPAL_TIMER_USEC_NATIVE 1
#define OPAL_TIMER_USEC_SUPPORTED 1

#endif
