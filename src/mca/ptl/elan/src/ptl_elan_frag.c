/*
 * $HEADER$
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/errno.h>
#include "types.h"
#include "datatype/datatype.h"
#include "mca/pml/base/pml_base_sendreq.h"
#include "mca/pml/base/pml_base_recvreq.h"
#include "ptl_elan.h"
#include "ptl_elan_peer.h"
#include "ptl_elan_proc.h"
#include "ptl_elan_frag.h"
#include "ptl_elan_req.h"
#include "ptl_elan_priv.h"

#if 0
static void
mca_ptl_elan_send_frag_construct (mca_ptl_elan_send_frag_t * frag)
{

}

static void
mca_ptl_elan_send_frag_destruct (mca_ptl_elan_send_frag_t * frag)
{

}

ompi_class_t mca_ptl_elan_send_frag_t_class = {
    "mca_ptl_elan_send_frag_t",
    OBJ_CLASS (mca_ptl_base_send_frag_t),
    (ompi_construct_t) mca_ptl_elan_send_frag_construct,
    (ompi_destruct_t) mca_ptl_elan_send_frag_destruct
};
#endif

static void
mca_ptl_elan_recv_frag_construct (mca_ptl_elan_recv_frag_t * frag)
{
    /*OBJ_CONSTRUCT (frag, mca_ptl_elan_recv_frag_t);*/
    frag->frag_hdr_cnt = 0;
    frag->frag_msg_cnt = 0;
    frag->frag.qdma = NULL;
    frag->alloc_buff = (char *) malloc (sizeof (char) * 2048 + 32);
    if (NULL == frag->alloc_buff) {
        ompi_output (0,
                     "[%s:%d] Fatal error, unable to allocate recv buff \n",
                     __FILE__, __LINE__);
    }
    frag->unex_buff = (char *) (((int) frag->alloc_buff + 32) >> 5 << 5);
}

static void
mca_ptl_elan_recv_frag_destruct (mca_ptl_elan_recv_frag_t * frag)
{
    /* Does this destruct free the memory? since OBJ_DESTRUCT,
     * works only for non-dynamically allocated objects */
    frag->frag_hdr_cnt = 0;
    frag->frag_msg_cnt = 0;
    frag->frag.qdma = NULL;
    free (frag->alloc_buff);
    frag->alloc_buff = NULL;
    frag->unex_buff = NULL;
    /*OBJ_DESTRUCT (frag);*/
}

ompi_class_t mca_ptl_elan_recv_frag_t_class = {
    "mca_ptl_elan_recv_frag_t",
    OBJ_CLASS (mca_ptl_base_recv_frag_t),
    (ompi_construct_t) mca_ptl_elan_recv_frag_construct,
    (ompi_destruct_t) mca_ptl_elan_recv_frag_destruct
};

extern mca_ptl_elan_state_t mca_ptl_elan_global_state;

mca_ptl_elan_desc_item_t *
mca_ptl_elan_alloc_send_desc (struct mca_pml_base_send_request_t *req)
{
    struct ompi_ptl_elan_queue_ctrl_t *queue;
    mca_ptl_elan_t *ptl;
    struct mca_ptl_elan_peer_t *peer;
    size_t      offset;
    size_t      size;

    ompi_free_list_t *flist;
    ompi_list_item_t *item;
    mca_ptl_elan_desc_item_t *desc;

    ptl = (mca_ptl_elan_t *) req->req_owner;
    peer = (mca_ptl_elan_peer_t *) req->req_peer;
    offset = req->req_offset;
    size = ptl->super.ptl_first_frag_size;

    /* For now, bind to queue DMA directly */
    {
        queue = ptl->queue;
        flist = &queue->tx_desc_free;

        if (ompi_using_threads ()) {

            ompi_mutex_lock (&((flist)->fl_lock));
            item = ompi_list_remove_first (&((flist)->super));

            /* Progress this PTL module to get back a descriptor,
             * Is it OK to progress with ptl->ptl_send_progress? */
            while (NULL == item) {
                mca_ptl_tstamp_t tstamp = 0;

                ptl->super.ptl_module->ptlm_progress (tstamp);
                item = ompi_list_remove_first (&((flist)->super));
            }
            ompi_mutex_unlock (&((flist)->fl_lock));
        } else {
            item = ompi_list_remove_first (&((flist)->super));

            /* Progress this PTL module to get back a descriptor,
             * Is it OK to progress with ptl->ptl_send_progress? */
            while (NULL == item) {
                mca_ptl_tstamp_t tstamp = 0;

                /* 
                 * Well, this still does not trigger the progress on 
                 * PTL's from other modules.  Wait for PML to change.
                 * Otherwise have to trigger PML progress from PTL.  Ouch..
                 */
                ptl->super.ptl_module->ptlm_progress (tstamp);
                item = ompi_list_remove_first (&((flist)->super));
            }
        }

        ((struct mca_ptl_elan_send_request_t *) req)->desc_type
            = MCA_PTL_ELAN_QDMA_DESC;
        desc = (mca_ptl_elan_desc_item_t *) item;
    }

    return desc;
}

mca_ptl_elan_recv_frag_t *
mca_ptl_elan_alloc_recv_desc (struct mca_pml_base_recv_request_t * req)
{
    return NULL;
}
