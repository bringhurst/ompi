/*
 * $HEADER$
 */

#include "lam_config.h"

#include <stdio.h>
#include "mpi.h"

#include "communicator/communicator.h"
#include "include/constants.h"
#include "mca/pml/pml.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/base.h"


/*
 * Global variables
 */

/*
** Table for Fortran <-> C communicator handle conversion
** Also used by P2P code to lookup communicator based
** on cid.
** 
*/
lam_pointer_array_t lam_mpi_communicators; 
lam_communicator_t  lam_mpi_comm_world;
lam_communicator_t  lam_mpi_comm_self;
lam_communicator_t  lam_mpi_comm_null;

static void lam_comm_construct(lam_communicator_t* comm);
static void lam_comm_destruct(lam_communicator_t* comm);

OBJ_CLASS_INSTANCE(lam_communicator_t, lam_object_t,lam_comm_construct, lam_comm_destruct );

/*
** sort-function for MPI_Comm_split 
*/
static int rankkeycompare(const void *, const void *);

/*
 * Initialize comm world/self/null.
 */
int lam_comm_init(void)
{
    lam_group_t *group;
    size_t size;

    /* Setup communicator array */
    OBJ_CONSTRUCT(&lam_mpi_communicators, lam_pointer_array_t); 

    /* Setup MPI_COMM_NULL */
    OBJ_CONSTRUCT(&lam_mpi_comm_null, lam_communicator_t);
    group = OBJ_NEW(lam_group_t);
    group->grp_proc_pointers = NULL;
    group->grp_my_rank = MPI_PROC_NULL;
    group->grp_proc_count = 0;
    group->grp_ok_to_free = false;
    OBJ_RETAIN(group); /* bump reference count for remote reference */

    lam_mpi_comm_null.c_contextid = MPI_UNDEFINED;
    lam_mpi_comm_null.c_my_rank = MPI_PROC_NULL;
    lam_mpi_comm_null.c_local_group = group;
    lam_mpi_comm_null.c_remote_group = group;
    lam_mpi_comm_null.error_handler = &lam_mpi_errors_are_fatal;
    OBJ_RETAIN( &lam_mpi_errors_are_fatal );
    lam_pointer_array_set_item(&lam_mpi_communicators, 0, &lam_mpi_comm_null);

    strncpy (lam_mpi_comm_null.c_name, "MPI_COMM_NULL", 
             strlen("MPI_COMM_NULL")+1 );
    lam_mpi_comm_null.c_flags |= LAM_COMM_NAMEISSET;

    /* VPS: Remove this later */
    lam_mpi_comm_null.bcast_lin_reqs = NULL;
    lam_mpi_comm_null.bcast_log_reqs = NULL;

    /* Setup MPI_COMM_WORLD */
    OBJ_CONSTRUCT(&lam_mpi_comm_world, lam_communicator_t);
    group = OBJ_NEW(lam_group_t);
    group->grp_proc_pointers = lam_proc_world(&size);
    group->grp_my_rank = lam_proc_local()->proc_vpid ;
    group->grp_proc_count = size;
    group->grp_ok_to_free = false;
    OBJ_RETAIN(group); /* bump reference count for remote reference */

    lam_mpi_comm_world.c_contextid = 0;
    lam_mpi_comm_world.c_my_rank = group->grp_my_rank;
    lam_mpi_comm_world.c_local_group = group;
    lam_mpi_comm_world.c_remote_group = group;
    lam_mpi_comm_world.c_cube_dim = lam_cube_dim(size);
    lam_mpi_comm_world.error_handler = &lam_mpi_errors_are_fatal;
    OBJ_RETAIN( &lam_mpi_errors_are_fatal );
    mca_pml.pml_add_comm(&lam_mpi_comm_world);
    lam_pointer_array_set_item(&lam_mpi_communicators, 0, &lam_mpi_comm_world);

    strncpy (lam_mpi_comm_world.c_name, "MPI_COMM_WORLD", 
             strlen("MPI_COMM_WORLD")+1 );
    lam_mpi_comm_world.c_flags |= LAM_COMM_NAMEISSET;

    /* VPS: Remove this later */
    lam_mpi_comm_world.bcast_lin_reqs =
	malloc (mca_coll_base_bcast_collmaxlin * sizeof(lam_request_t*));
    if (NULL ==  lam_mpi_comm_world.bcast_lin_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }
    lam_mpi_comm_world.bcast_log_reqs = 
	malloc (mca_coll_base_bcast_collmaxdim * sizeof(lam_request_t*));
    if (NULL ==  lam_mpi_comm_world.bcast_log_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }
    
    /* Setup MPI_COMM_SELF */
    OBJ_CONSTRUCT(&lam_mpi_comm_self, lam_communicator_t);
    group = OBJ_NEW(lam_group_t);
    group->grp_proc_pointers = lam_proc_self(&size);
    group->grp_my_rank = 0;
    group->grp_proc_count = size;
    group->grp_ok_to_free = false;
    OBJ_RETAIN(group); /* bump reference count for remote reference */

    lam_mpi_comm_self.c_contextid = 1;
    lam_mpi_comm_self.c_my_rank = group->grp_my_rank;
    lam_mpi_comm_self.c_local_group = group;
    lam_mpi_comm_self.c_remote_group = group;
    lam_mpi_comm_self.error_handler = &lam_mpi_errors_are_fatal;
    OBJ_RETAIN( &lam_mpi_errors_are_fatal );
    mca_pml.pml_add_comm(&lam_mpi_comm_self);
    lam_pointer_array_set_item(&lam_mpi_communicators, 1, &lam_mpi_comm_self);

    strncpy (lam_mpi_comm_self.c_name, "MPI_COMM_SELF", 
             strlen("MPI_COMM_SELF")+1 );
    lam_mpi_comm_self.c_flags |= LAM_COMM_NAMEISSET;

    /* VPS: Remove this later */
    lam_mpi_comm_self.bcast_lin_reqs =
	malloc (mca_coll_base_bcast_collmaxlin * sizeof(lam_request_t*));
    if (NULL ==  lam_mpi_comm_self.bcast_lin_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }
    lam_mpi_comm_self.bcast_log_reqs = 
	malloc (mca_coll_base_bcast_collmaxdim * sizeof(lam_request_t*));
    if (NULL ==  lam_mpi_comm_self.bcast_log_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }

    /* Some topo setup */

    /* Some attribute setup stuff */

    return LAM_SUCCESS;
}


lam_communicator_t *lam_comm_allocate ( int local_size, int remote_size )
{
    lam_communicator_t *new_comm=NULL;

    /* create new communicator element */
    new_comm = OBJ_NEW(lam_communicator_t);
    if ( new_comm ) {
        if ( LAM_ERROR == new_comm->c_f_to_c_index) {
            OBJ_RELEASE(new_comm);
            new_comm = NULL;
        }
        else {
            new_comm->c_local_group = lam_group_allocate ( local_size );
            if ( remote_size > 0 ) {
                new_comm->c_remote_group = lam_group_allocate (remote_size);
                
                /* mark the comm as an inter-communicator */
                new_comm->c_flags |= LAM_COMM_INTER;
            }
            else {
                /* simplifies some operations (e.g. p2p), if 
                   we can always use the remote group */
                new_comm->c_remote_group = new_comm->c_local_group;
            }
        }
        /* fill in the inscribing hyper-cube dimensions */
        new_comm->c_cube_dim = lam_cube_dim(local_size);
        if (LAM_ERROR == new_comm->c_cube_dim) {
            OBJ_RELEASE(new_comm);
            new_comm = NULL;
        }
    }

    return new_comm;
}

/* Proper implementation of this function to follow soon.
** the function has to be thread safe and work for 
** any combinationf of inter- and intra-communicators.
*/
int lam_comm_nextcid ( lam_communicator_t* comm, int mode )
{
    static int nextcid=1;
    return nextcid++;
}


/*
** COunterpart to MPI_Comm_group. To be used within LAM functions.
*/
int lam_comm_group ( lam_communicator_t* comm, lam_group_t **group )
{
     /* local variable */
    lam_group_t *group_p;

   /* get new group struct */
    group_p=lam_group_allocate(comm->c_local_group->grp_proc_count);
    if( NULL == group_p ) {
        return MPI_ERR_GROUP;
    }

    /* set elements of the struct */
    group_p->grp_my_rank = comm->c_local_group->grp_my_rank;
    memcpy ( group_p->grp_proc_pointers, 
             comm->c_local_group->grp_proc_pointers,
             group_p->grp_proc_count * sizeof ( lam_proc_t *));

    /* increment proc reference counters */
    lam_group_increment_proc_count(group_p);

    /* set the user handle */
    *group = group_p;
    return MPI_SUCCESS;
}

/*
** Counterpart to MPI_Comm_split. To be used within LAM (e.g. MPI_Cart_sub).
*/
int lam_comm_split ( lam_communicator_t* comm, int color, int key, 
                     lam_communicator_t **newcomm )
{
    lam_group_t *new_group;
    int myinfo[2];
    int size, my_size;
    int my_grank;
    int i, loc;
    int *results, *sorted; 
    int rc=MPI_SUCCESS;
    lam_proc_t *my_gpointer;
    
    if ( comm->c_flags & LAM_COMM_INTER ) {
        /* creating an inter-communicator using MPI_Comm_create will
           be supported soon, but not in this version */
        return MPI_ERR_COMM;
    }
    else {
        /* sort according to color and rank */
        size = lam_comm_size ( comm );

        results = (int*) malloc ( 2 * size * sizeof(int));
        if ( !results ) return MPI_ERR_INTERN;

	/* Fill in my information */
	myinfo[0] = color;
	myinfo[1] = key;

	/* Gather information from everyone */
        rc = comm->c_coll.coll_allgather_intra ( myinfo, 2, MPI_INT,
                                    results, 2, MPI_INT, comm );
        if ( rc != LAM_SUCCESS ) {
            free ( results );
            return rc;
        }
        
        
        /* now how many do we have in 'my/our' new group */
        for ( my_size = 0, i=0; i < size; i++) 
            if ( results[(2*i)+0] == color) my_size++;
        
        sorted = (int *) malloc ( sizeof( int ) * my_size * 2);
        if (!sorted) {
            free ( results );
            return MPI_ERR_INTERN;
        }
        
        /* ok we can now fill this info */
        for( loc = 0, i = 0; i < size; i++ ) 
            if ( results[(2*i)+0] == color) {
                sorted[(2*loc)+0] = i;		       /* copy org rank */
                sorted[(2*loc)+1] = results[(2*i)+1];  /* copy key */
                loc++;
            }
        
        /* the new array needs to be sorted so that it is in 'key' order */
        /* if two keys are equal then it is sorted in original rank order! */
        if(my_size>1)
            qsort ((int*)sorted, my_size, sizeof(int)*2, rankkeycompare);
    
        /* to build a new group we need a separate array of ranks! */
        for ( i = 0; i < my_size; i++ ) 
            results[i] = sorted[(i*2)+0];
        
        /* we can now free the 'sorted' array */
        free ( sorted );
    
        /* we now set a group and use lam_comm_create.
           Get new group struct, Skipping here
           the increasing of the proc-reference counters,
           since the group is freed a couple of lines later.
        */
        new_group = lam_group_allocate(my_size);
        if( NULL == new_group ) {
            free ( results );
            return MPI_ERR_GROUP;
        }

        /* put group elements in the list */
        for (i = 0; i < my_size; i++) {
            new_group->grp_proc_pointers[i] =
                comm->c_local_group->grp_proc_pointers[results[i]];
        }  /* end proc loop */
        
        /* find my rank */
        my_grank=comm->c_local_group->grp_my_rank;             
        my_gpointer=comm->c_local_group->grp_proc_pointers[my_grank];
        lam_set_group_rank(new_group, my_gpointer);

        rc = lam_comm_create ( comm, new_group, newcomm );

        /* Free now the results-array*/
        free ( results );

        /* free the group */
        OBJ_RELEASE(new_group);
    }

    /* no need to set error-handler, since this is done in
       lam_comm_create already */

    return rc;
}

/*
** Counterpart to MPI_Comm_create. To be used within LAM.
*/
int lam_comm_create ( lam_communicator_t *comm, lam_group_t *group, 
                      lam_communicator_t **newcomm )
{
    lam_communicator_t *newcomp;
    
    if ( comm->c_flags & LAM_COMM_INTER ) {
        /* creating an inter-communicator using MPI_Comm_create will
           be supported soon, but not in this version */
        return MPI_ERR_COMM;
    }
    else {
        newcomp = lam_comm_allocate ( group->grp_proc_count, 0 );
        /* copy local group */
        newcomp->c_local_group->grp_my_rank = group->grp_my_rank;
        memcpy (newcomp->c_local_group->grp_proc_pointers, 
                group->grp_proc_pointers, 
                group->grp_proc_count * sizeof(lam_proc_t *));
        lam_group_increment_proc_count(newcomp->c_local_group);
        
        newcomp->c_my_rank =  group->grp_my_rank;

        /* determine context id */
        /*        newcomp->c_contextid = lam_comm_nextcid ( comm, LAM_COMM_INTRA_INTRA); */
        
        /* just for now ! */
        newcomp->c_contextid = newcomp->c_f_to_c_index;
    }

    newcomp->c_cube_dim = lam_cube_dim(group->grp_proc_count);

    /* copy error handler */
    newcomp->error_handler = comm->error_handler;
    OBJ_RETAIN ( newcomp->error_handler );

    /* Initialize the PML stuff in the newcomp  */
    mca_pml.pml_add_comm(newcomp);
    
    if (-1 == lam_pointer_array_add(&lam_mpi_communicators, newcomp)) {
	return MPI_ERR_INTERN;
    }

    /* We add the name here for debugging purposes */
    snprintf(newcomp->c_name, MPI_MAX_OBJECT_NAME, "MPI COMMUNICATOR %d", 
	     newcomp->c_my_rank);

    /* Let the collectives modules fight over who will do
       collective on this new comm.  */
    if (LAM_ERROR == mca_coll_base_init_comm(newcomp))
	return MPI_ERR_INTERN;

    /* ******* VPS: Remove this later -- need to be in coll module ******  */
    /* VPS: Cache the reqs for bcast */
    newcomp->bcast_lin_reqs =
	malloc (mca_coll_base_bcast_collmaxlin * sizeof(lam_request_t*));
    if (NULL ==  newcomp->bcast_lin_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }
    newcomp->bcast_log_reqs = 
	malloc (mca_coll_base_bcast_collmaxdim * sizeof(lam_request_t*));
    if (NULL ==  newcomp->bcast_log_reqs) {
	return LAM_ERR_OUT_OF_RESOURCE;
    }

    /* Some topo setup */

    /* Some attribute setup stuff */

    if ( MPI_PROC_NULL == newcomp->c_local_group->grp_my_rank ) {
        /* we are not part of the new comm, so we have to free it again.
           However, we could not avoid the comm_nextcid step, since
           all processes of the original comm have to participate in
           that function call. Additionally, all errhandler stuff etc.
           has to be set to make lam_comm_free happy */
        lam_comm_free ( &newcomp );
    }

    *newcomm =  newcomp;
    return MPI_SUCCESS;
}

/*
** Counterpart to MPI_Comm_free. To be used within LAM.
*/
int lam_comm_free ( lam_communicator_t **comm )
{
    int proc;
    lam_group_t *grp;
    lam_communicator_t *comp;

    comp = (lam_communicator_t *)*comm;

    /* Release attributes */
#if 0
    lam_attr_delete_all ( COMM_ATTR, comp );
#endif

    /* **************VPS: need a coll_base_finalize ******** */

    /* Release topology information */

    /* Release local group */
    grp = comp->c_local_group;
    for ( proc = 0; proc <grp->grp_proc_count; proc++ )
        OBJ_RELEASE (grp->grp_proc_pointers[proc]);

    /* Release remote group */
    if ( comp->c_flags & LAM_COMM_INTER ) {
        grp = comp->c_remote_group;
        for ( proc = 0; proc <grp->grp_proc_count; proc++ )
            OBJ_RELEASE (grp->grp_proc_pointers[proc]);
    }

    /* Release error handler */
    OBJ_RELEASE ( comp->error_handler );

    /******** VPS: this goes away *************/
    /* Release the cached bcast requests */
    free (comp->bcast_lin_reqs);
    free (comp->bcast_log_reqs);

    /* Release finally the communicator itself */
    OBJ_RELEASE ( comp );

    *comm = MPI_COMM_NULL;
    return MPI_SUCCESS;
}

int lam_comm_finalize(void) 
{
    return LAM_SUCCESS;
}

/*
 * For linking only.
 */
int lam_comm_link_function(void)
{
  return LAM_SUCCESS;
}

/********************************************************************************/
/********************************************************************************/
/********************************************************************************/
/* static functions */

/* 
** rankkeygidcompare() compares a tuple of (rank,key,gid) producing 
** sorted lists that match the rules needed for a MPI_Comm_split 
*/
static int rankkeycompare (const void *p, const void *q)
{
    int *a, *b;
  
    /* ranks at [0] key at [1] */
    /* i.e. we cast and just compare the keys and then the original ranks.. */
    a = (int*)p;
    b = (int*)q;
    
    /* simple tests are those where the keys are different */
    if (a[1] < b[1]) return (-1);
    if (a[1] > b[1]) return (1);
    
    /* ok, if the keys are the same then we check the original ranks */
    if (a[1] == b[1]) {
        if (a[0] < b[0]) return (-1);
        if (a[0] == b[0]) return (0);
        if (a[0] > b[0]) return (1);
    }
    return ( 0 );
}

static void lam_comm_construct(lam_communicator_t* comm)
{
    /* assign entry in fortran <-> c translation array */
    comm->c_f_to_c_index = lam_pointer_array_add (&lam_mpi_communicators,
                                                  comm);
    comm->c_name[0]   = '\0';
    comm->c_contextid = MPI_UNDEFINED;
    comm->c_flags     = 0;
    comm->c_my_rank   = 0;
    comm->c_cube_dim  = 0;
    comm->c_local_group  = NULL;
    comm->c_remote_group = NULL;
    comm->error_handler  = NULL;
    comm->c_pml_comm     = NULL;
    comm->c_coll_comm    = NULL;
    comm->c_topo_comm    = NULL; 

    return;
}

static void lam_comm_destruct(lam_communicator_t* dead_comm)
{
    /* release the grps */
    OBJ_RELEASE ( dead_comm->c_local_group );
    if ( dead_comm->c_flags & LAM_COMM_INTER )
        OBJ_RELEASE ( dead_comm->c_remote_group );

    /* reset the lam_comm_f_to_c_table entry */
    if ( NULL != lam_pointer_array_get_item ( &lam_mpi_communicators,
                                              dead_comm->c_f_to_c_index )) {
        lam_pointer_array_set_item ( &lam_mpi_communicators,
                                     dead_comm->c_f_to_c_index, NULL);
    }
    return;
}




