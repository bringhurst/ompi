/*
 * $HEADER$
 */

#ifndef LAM_GROUP_H
#define LAM_GROUP_H

#include "mpi.h"
#include "mpi/proc/proc.h"
#include "lam/lfc/lam_pointer_array.h"
extern lam_class_t lam_group_t_class;

struct lam_group_t {
    /* base class */
    lam_object_t super;
    /* number of processes in group */
    int grp_proc_count;
    /* rank in group */
    int grp_my_rank;
    /* index in Fortran <-> C translation array */
    int grp_f_to_c_index;
    /* list of pointers to lam_proc_t structures for each
     *   process in the group */
    lam_proc_t **grp_proc_pointers;
};
typedef struct lam_group_t lam_group_t;

/*
 * Table for Fortran <-> C group handle conversion
 */
extern lam_pointer_array_t *lam_group_f_to_c_table;

/* 
 * function prototypes 
 */

/**
 * This routine is used to allocate a new group structure
 *
 * @param group_size Number of MPI processes in the group
 *
 * @return Pointer to new group structure
 */
lam_group_t *group_allocate(int group_size);

/**
 * Initialize LAM group infrastructure
 *
 * @return Error code
 */
int lam_group_init(void);

/**
 * Clean up LAM group infrastructure
 *
 * @return Error code
 */
int lam_group_finalize(void);

/**
 * Get group size
 *
 * @param group Pointer to lam_group_t structute (IN)
 *
 * @return Group size
 */
static inline int lam_group_size(lam_group_t *group){
    return group->grp_proc_count;
}

/**
 * Get group rank
 *
 * @param group Pointer to lam_group_t structure (IN)
 *
 * @return Group rank
 */
static inline int lam_group_rank(lam_group_t *group){
    return group->grp_proc_count;
}

/**
 * Set group rank in the input group structure
 *
 * @param group Group Pointer to lam_group_t structure (IN)
 * @param proc_pointer Pointer to lam_proc_t structure for process.
 *                     MPI_PROC_NULL may be used to indicate proc not
 *                     in group
 *
 * @return Error code
 */
void lam_set_group_rank(lam_group_t *group, lam_proc_t *proc_pointer);

#endif /* LAM_GROUP_H */
