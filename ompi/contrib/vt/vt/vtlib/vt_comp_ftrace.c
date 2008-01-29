/**
 * VampirTrace
 * http://www.tu-dresden.de/zih/vampirtrace
 *
 * Copyright (c) 2005-2008, ZIH, TU Dresden, Federal Republic of Germany
 *
 * Copyright (c) 1998-2005, Forschungszentrum Juelich GmbH, Federal
 * Republic of Germany
 *
 * See the file COPYRIGHT in the package base directory for details
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vt_memhook.h"
#include "vt_pform.h"
#include "vt_trc.h"

#if (defined (VT_OMPI) || defined (VT_OMP))
#include <omp.h>
#define VT_MY_THREAD   omp_get_thread_num() 
#define VT_NUM_THREADS omp_get_max_threads() 
#else
#define VT_MY_THREAD   0
#define VT_NUM_THREADS 1 
#endif

extern void* vftr_getname(void);
extern int vftr_getname_len(void);

/*
 *-----------------------------------------------------------------------------
 * Simple hash table to map function names to region identifier
 *-----------------------------------------------------------------------------
 */

typedef struct HN {
  long id;             /* hash code (address of function name) */
  uint32_t vtid;       /* associated region identifier  */
  struct HN* next;
} HashNode;

#define HASH_MAX 1021

static int necsx_init = 1;       /* is initialization needed? */

static long *stk_level;          /* stack level */

static HashNode* htab[HASH_MAX];

/*
 * Stores region identifier `e' under hash code `h'
 */

static void hash_put(long h, uint32_t e) {
  long id = h % HASH_MAX;
  HashNode *add = (HashNode*)malloc(sizeof(HashNode));
  add->id = h;
  add->vtid = e;
  add->next = htab[id];
  htab[id] = add;
}

/*
 * Lookup hash code `h'
 * Returns region identifier if already stored, otherwise VT_NO_ID
 */

static uint32_t hash_get(long h) {
  long id = h % HASH_MAX;
  HashNode *curr = htab[id];
  while ( curr ) {
    if ( curr->id == h ) {
      return curr->vtid;
    }
    curr = curr->next;
  }
  return VT_NO_ID;
}

/*
 * Register new region
 */

static uint32_t register_region(char *func, int len) {
  uint32_t rid;
  static char fname[1024];

  strncpy(fname, func, len);
  fname[len] = '\0';
  rid = vt_def_region(fname, VT_NO_ID, VT_NO_LNO, VT_NO_LNO,
                       VT_DEF_GROUP, VT_FUNCTION);
  hash_put((long) func, rid);
  return rid;
}


void _ftrace_enter2_(void);
void _ftrace_exit2_(void);
void _ftrace_stop2_(void);

/*
 * This function is called at the entry of each function
 * The call is generated by the NEC SX compilers
 */

void _ftrace_enter2_() {
  int mt = VT_MY_THREAD;
  char *func = (char *)vftr_getname();
  int len = vftr_getname_len();
  uint32_t rid;
  uint64_t time;

  /* -- if not yet initialized, initialize VampirTrace -- */
  if ( necsx_init ) {
    VT_MEMHOOKS_OFF();
    necsx_init = 0;
    stk_level = (long*)calloc(VT_NUM_THREADS, sizeof(long));
    vt_open();
    VT_MEMHOOKS_ON();
  }

  /* -- return, if tracing is disabled? -- */
  if ( !VT_IS_TRACE_ON() ) return;

  /* -- ignore NEC OMP runtime functions -- */
  if ( strchr(func, '$') != NULL ) return;

  VT_MEMHOOKS_OFF();

  time = vt_pform_wtime();

  /* -- get region identifier -- */
  if ( (rid = hash_get((long) func)) == VT_NO_ID ) {
    /* -- region entered the first time, register region -- */
#   if defined (VT_OMPI) || defined (VT_OMP)
    if (omp_in_parallel()) {
#     pragma omp critical (vt_comp_ftrace_1)
      {
        if ( (rid = hash_get((long) func)) == VT_NO_ID ) {
          rid = register_region(func, len);
        }
      }
    } else {
      rid = register_region(func, len);
    }
#   else
    rid = register_region(func, len);
#   endif
  }

  /* -- write enter record -- */
  vt_enter(&time, rid);
  stk_level[mt]++;

  VT_MEMHOOKS_ON();
}

/*
 * This function is called at the exit of each function
 * The call is generated by the NEC SX compilers
 */

void _ftrace_exit2_() {
  int mt = VT_MY_THREAD;
  char *func;
  uint64_t time;

  /* -- return, if tracing is disabled? -- */
  if ( !VT_IS_TRACE_ON() ) return;

  VT_MEMHOOKS_OFF();

  func = (char *)vftr_getname();

  /* -- ignore NEC OMP runtime functions -- */
  if ( strchr(func, '$') != NULL )
  {
    VT_MEMHOOKS_ON();
    return;
  }

  /* -- write exit record -- */
  time = vt_pform_wtime();
  vt_exit(&time);
  stk_level[mt]--;

  VT_MEMHOOKS_ON();
}

/*
 * This function is called at the exit of the program
 * The call is generated by the NEC SX compilers
 */

void _ftrace_stop2_() {
  uint64_t time;

  VT_MEMHOOKS_OFF();

  while(stk_level[VT_MY_THREAD] > 0) {
    stk_level[VT_MY_THREAD]--;
    time = vt_pform_wtime();
    vt_exit(&time);
  }
  vt_close();
}

