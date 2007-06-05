/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2007 The University of Tennessee and The University
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
 *
 */

#include "ompi_config.h"

#include "opal/opal_socket_errno.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <fcntl.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
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
#if OPAL_WANT_IPV6
#  ifdef HAVE_NETDB_H
#  include <netdb.h>
#  endif
#endif

#include "ompi/constants.h"
#include "opal/event/event.h"
#include "opal/util/if.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "orte/mca/oob/base/base.h"
#include "orte/mca/ns/ns_types.h"
#include "ompi/mca/pml/pml.h"
#include "ompi/mca/btl/btl.h"

#include "opal/mca/base/mca_base_param.h"
#include "ompi/mca/pml/base/pml_base_module_exchange.h"
#include "orte/mca/errmgr/errmgr.h"
#include "ompi/mca/mpool/base/base.h" 
#include "ompi/mca/btl/base/btl_base_error.h"
#include "btl_tcp.h"
#include "btl_tcp_addr.h"
#include "btl_tcp_proc.h"
#include "btl_tcp_frag.h"
#include "btl_tcp_endpoint.h" 
#include "ompi/mca/btl/base/base.h" 
#include "ompi/datatype/convertor.h" 


mca_btl_tcp_component_t mca_btl_tcp_component = {
    {
        /* First, the mca_base_component_t struct containing meta information
           about the component itself */

        {
            /* Indicate that we are a pml v1.0.0 component (which also implies a
               specific MCA version) */

            MCA_BTL_BASE_VERSION_1_0_1,

            "tcp", /* MCA component name */
            1,  /* MCA component major version */
            0,  /* MCA component minor version */
            0,  /* MCA component release version */
            mca_btl_tcp_component_open,  /* component open */
            mca_btl_tcp_component_close  /* component close */
        },

        /* Next the MCA v1.0.0 component meta data */

        {
            /* The component is checkpoint ready */
            MCA_BASE_METADATA_PARAM_CHECKPOINT
        },

        mca_btl_tcp_component_init,  
        NULL,
    }
};

/*
 * utility routines for parameter registration
 */

static inline char* mca_btl_tcp_param_register_string(
                                                     const char* param_name, 
                                                     const char* default_value)
{
    char *param_value;
    int id = mca_base_param_register_string("btl","tcp",param_name,NULL,default_value);
    mca_base_param_lookup_string(id, &param_value);
    return param_value;
}

static inline int mca_btl_tcp_param_register_int(
        const char* param_name, 
        int default_value)
{
    int id = mca_base_param_register_int("btl","tcp",param_name,NULL,default_value);
    int param_value = default_value;
    mca_base_param_lookup_int(id,&param_value);
    return param_value;
}


/*
 * Data structure for accepting connections.
 */

struct mca_btl_tcp_event_t {
    opal_list_item_t item;
    opal_event_t event;
};
typedef struct mca_btl_tcp_event_t mca_btl_tcp_event_t;

static void mca_btl_tcp_event_construct(mca_btl_tcp_event_t* event)
{
    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    opal_list_append(&mca_btl_tcp_component.tcp_events, &event->item);
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);
}

static void mca_btl_tcp_event_destruct(mca_btl_tcp_event_t* event)
{
    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    opal_list_remove_item(&mca_btl_tcp_component.tcp_events, &event->item);
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);
}

OBJ_CLASS_INSTANCE(
    mca_btl_tcp_event_t,
    opal_list_item_t,
    mca_btl_tcp_event_construct,
    mca_btl_tcp_event_destruct);


/*
 * functions for receiving event callbacks
 */
static void mca_btl_tcp_component_recv_handler(int, short, void*);
static void mca_btl_tcp_component_accept_handler(int, short, void*);


/*
 *  Called by MCA framework to open the component, registers
 *  component parameters.
 */

int mca_btl_tcp_component_open(void)
{
#ifdef __WINDOWS__
    WSADATA win_sock_data;
    if( WSAStartup(MAKEWORD(2,2), &win_sock_data) != 0 ) {
        BTL_ERROR(("failed to initialise windows sockets:%d", WSAGetLastError()));
        return OMPI_ERROR;
    }
#endif

    /* initialize state */
    mca_btl_tcp_component.tcp_listen_sd = -1;
#if OPAL_WANT_IPV6
    mca_btl_tcp_component.tcp6_listen_sd = -1;
#endif
    mca_btl_tcp_component.tcp_num_btls=0;
    mca_btl_tcp_component.tcp_addr_count = 0;
    mca_btl_tcp_component.tcp_btls=NULL;
    
    /* initialize objects */ 
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_lock, opal_mutex_t);
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_procs, opal_hash_table_t);
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_events, opal_list_t);
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_frag_eager, ompi_free_list_t);
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_frag_max, ompi_free_list_t);
    OBJ_CONSTRUCT(&mca_btl_tcp_component.tcp_frag_user, ompi_free_list_t);
    opal_hash_table_init(&mca_btl_tcp_component.tcp_procs, 256);

    /* register TCP component parameters */
    mca_btl_tcp_component.tcp_num_links =
        mca_btl_tcp_param_register_int("links", 1);
    mca_btl_tcp_component.tcp_if_include =
        mca_btl_tcp_param_register_string("if_include", "");
    mca_btl_tcp_component.tcp_if_exclude =
        mca_btl_tcp_param_register_string("if_exclude", "lo");
    mca_btl_tcp_component.tcp_free_list_num =
        mca_btl_tcp_param_register_int ("free_list_num", 8);
    mca_btl_tcp_component.tcp_free_list_max =
        mca_btl_tcp_param_register_int ("free_list_max", -1);
    mca_btl_tcp_component.tcp_free_list_inc =
        mca_btl_tcp_param_register_int ("free_list_inc", 32);
    mca_btl_tcp_component.tcp_sndbuf =
        mca_btl_tcp_param_register_int ("sndbuf", 128*1024);
    mca_btl_tcp_component.tcp_rcvbuf =
        mca_btl_tcp_param_register_int ("rcvbuf", 128*1024);
    mca_btl_tcp_component.tcp_endpoint_cache =
        mca_btl_tcp_param_register_int ("endpoint_cache", 30*1024);

    mca_btl_tcp_module.super.btl_exclusivity =  MCA_BTL_EXCLUSIVITY_LOW;
    mca_btl_tcp_module.super.btl_eager_limit = 64*1024;
    mca_btl_tcp_module.super.btl_min_send_size = 64*1024;
    mca_btl_tcp_module.super.btl_max_send_size = 128*1024;
    mca_btl_tcp_module.super.btl_rdma_pipeline_offset = 128*1024;
    mca_btl_tcp_module.super.btl_rdma_pipeline_frag_size = INT_MAX;
    mca_btl_tcp_module.super.btl_min_rdma_pipeline_size = 0;
    mca_btl_tcp_module.super.btl_flags = MCA_BTL_FLAGS_PUT |
                                       MCA_BTL_FLAGS_SEND_INPLACE |
                                       MCA_BTL_FLAGS_NEED_CSUM |
                                       MCA_BTL_FLAGS_NEED_ACK;
    mca_btl_tcp_module.super.btl_bandwidth = 100;
    mca_btl_tcp_module.super.btl_latency = 0;
    mca_btl_base_param_register(&mca_btl_tcp_component.super.btl_version,
            &mca_btl_tcp_module.super);

    mca_btl_tcp_component.tcp_disable_family =
        mca_btl_tcp_param_register_int ("disable_family", 0);
    return OMPI_SUCCESS;
}


/*
 * module cleanup - sanity checking of queue lengths
 */

int mca_btl_tcp_component_close(void)
{
    opal_list_item_t* item;

    if(NULL != mca_btl_tcp_component.tcp_if_include)
        free(mca_btl_tcp_component.tcp_if_include);
    if(NULL != mca_btl_tcp_component.tcp_if_exclude)
       free(mca_btl_tcp_component.tcp_if_exclude);
    if (NULL != mca_btl_tcp_component.tcp_btls)
        free(mca_btl_tcp_component.tcp_btls);
 
    if (mca_btl_tcp_component.tcp_listen_sd >= 0) {
        opal_event_del(&mca_btl_tcp_component.tcp_recv_event);
        CLOSE_THE_SOCKET(mca_btl_tcp_component.tcp_listen_sd);
        mca_btl_tcp_component.tcp_listen_sd = -1;
    }
#if OPAL_WANT_IPV6
    if (mca_btl_tcp_component.tcp6_listen_sd >= 0) {
        opal_event_del(&mca_btl_tcp_component.tcp6_recv_event);
        CLOSE_THE_SOCKET(mca_btl_tcp_component.tcp6_listen_sd);
        mca_btl_tcp_component.tcp6_listen_sd = -1;
    }
#endif

    /* cleanup any pending events */
    OPAL_THREAD_LOCK(&mca_btl_tcp_component.tcp_lock);
    for(item =  opal_list_remove_first(&mca_btl_tcp_component.tcp_events);
        item != NULL; 
        item =  opal_list_remove_first(&mca_btl_tcp_component.tcp_events)) {
        mca_btl_tcp_event_t* event = (mca_btl_tcp_event_t*)item;
        opal_event_del(&event->event);
        OBJ_RELEASE(event);
    }
    OPAL_THREAD_UNLOCK(&mca_btl_tcp_component.tcp_lock);

    /* release resources */
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_procs);
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_events);
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_frag_eager);
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_frag_max);
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_frag_user);
    OBJ_DESTRUCT(&mca_btl_tcp_component.tcp_lock);

#ifdef __WINDOWS__
    WSACleanup();
#endif

    return OMPI_SUCCESS;
}


/*
 *  Create a btl instance and add to modules list.
 */

static int mca_btl_tcp_create(int if_kindex, const char* if_name)
{
    struct mca_btl_tcp_module_t* btl;
    char param[256];
    int i;

    for( i = 0; i < (int)mca_btl_tcp_component.tcp_num_links; i++ ) {
        btl = (struct mca_btl_tcp_module_t *)malloc(sizeof(mca_btl_tcp_module_t));
        if(NULL == btl)
            return OMPI_ERR_OUT_OF_RESOURCE;
        memcpy(btl, &mca_btl_tcp_module, sizeof(mca_btl_tcp_module));
        OBJ_CONSTRUCT(&btl->tcp_endpoints, opal_list_t);
        mca_btl_tcp_component.tcp_btls[mca_btl_tcp_component.tcp_num_btls++] = btl;

        /* initialize the btl */
        btl->tcp_ifkindex = (uint16_t) if_kindex;
#if MCA_BTL_TCP_STATISTICS
        btl->tcp_bytes_recv = 0;
        btl->tcp_bytes_sent = 0;
        btl->tcp_send_handler = 0;
#endif

        /* allow user to specify interface bandwidth */
        sprintf(param, "bandwidth_%s", if_name);
        btl->super.btl_bandwidth = mca_btl_tcp_param_register_int(param, btl->super.btl_bandwidth);

        /* allow user to override/specify latency ranking */
        sprintf(param, "latency_%s", if_name);
        btl->super.btl_latency = mca_btl_tcp_param_register_int(param, btl->super.btl_latency);
        if( i > 0 ) {
            btl->super.btl_bandwidth >>= 1;
            btl->super.btl_latency   <<= 1;
        }

        /* allow user to specify interface bandwidth */
        sprintf(param, "bandwidth_%s:%d", if_name, i);
        btl->super.btl_bandwidth = mca_btl_tcp_param_register_int(param, btl->super.btl_bandwidth);

        /* allow user to override/specify latency ranking */
        sprintf(param, "latency_%s:%d", if_name, i);
        btl->super.btl_latency = mca_btl_tcp_param_register_int(param, btl->super.btl_latency);
#if 0 && OMPI_ENABLE_DEBUG
        BTL_OUTPUT(("interface %s instance %i: bandwidth %d latency %d\n", if_name, i,
                    btl->super.btl_bandwidth, btl->super.btl_latency));
#endif
    }
    return OMPI_SUCCESS;
}

/*
 * Create a TCP BTL instance for either:
 * (1) all interfaces specified by the user
 * (2) all available interfaces 
 * (3) all available interfaces except for those excluded by the user
 */

static int mca_btl_tcp_component_create_instances(void)
{
    const int if_count = opal_ifcount();
    int if_index;
    int kif_count = 0;
    int *kindexes = NULL; /* this array is way too large, but never too small */
    char **include;
    char **exclude;
    char **argv;
    int ret = OMPI_SUCCESS;

    if(if_count <= 0) {
        return OMPI_ERROR;
    }

    kindexes = malloc(sizeof(int) * if_count);
    if (NULL == kindexes) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    /* calculate the number of kernel indexes (number of physical NICs) */
    {
        int j;

        /* initialize array to 0. Assumption: 0 isn't a valid kernel index */
        memset (kindexes, 0, sizeof(int) * if_count);

        /* assign the corresponding kernel indexes for all opal_list indexes
         * (loop over all addresses)
         */
        for(if_index = opal_ifbegin(); if_index >= 0; if_index = opal_ifnext(if_index)){
            int index = opal_ifindextokindex (if_index);
            if (index > 0) {
                bool already_seen = false;
                for (j=0; (false == already_seen) && (j < kif_count); j++) {
                    if (kindexes[j] == index) {
                        already_seen = true;
                    }
                }

                if (false == already_seen) {
                    kindexes[kif_count] = index;
                    kif_count++;
                }
            }
        }
    }

    /* allocate memory for btls */
    mca_btl_tcp_component.tcp_btls = (mca_btl_tcp_module_t**)malloc(mca_btl_tcp_component.tcp_num_links *
                                                                    kif_count * sizeof(mca_btl_tcp_module_t*));
    if(NULL == mca_btl_tcp_component.tcp_btls) {
        ret = OMPI_ERR_OUT_OF_RESOURCE;
        goto cleanup;
    }

    mca_btl_tcp_component.tcp_addr_count = if_count;

    /* if the user specified an interface list - use these exclusively */
    argv = include = opal_argv_split(mca_btl_tcp_component.tcp_if_include,',');
    while(argv && *argv) {
        char* if_name = *argv;
        int if_index = opal_ifnametokindex(if_name);
        if(if_index < 0) {
            BTL_ERROR(("invalid interface \"%s\"", if_name));
        } else {
            mca_btl_tcp_create(if_index, if_name);
        }
        argv++;
    }
    opal_argv_free(include);
    if(mca_btl_tcp_component.tcp_num_btls) {
        ret = OMPI_SUCCESS;
        goto cleanup;
    }

    /* if the interface list was not specified by the user, create 
     * a BTL for each interface that was not excluded.
    */
    exclude = opal_argv_split(mca_btl_tcp_component.tcp_if_exclude,',');
    {
        int i;
        for(i = 0; i < kif_count; i++) {
            /* Bug, FIXME: Don't hardcode length of if_name, use IFNAMESIZE */
            char if_name[32];
            if_index = kindexes[i];

            opal_ifkindextoname(if_index, if_name, sizeof(if_name));

            /* check to see if this interface exists in the exclude list */
            if(opal_ifcount() > 1) {
                argv = exclude;
                while(argv && *argv) {
                    if(strncmp(*argv,if_name,strlen(*argv)) == 0)
                        break;
                    argv++;
                }
                /* if this interface was not found in the excluded list, create a BTL */
                if(argv == 0 || *argv == 0) {
                    mca_btl_tcp_create(if_index, if_name);
                }
            } else {
                mca_btl_tcp_create(if_index, if_name);
            }
        }
    }
    opal_argv_free(exclude);

 cleanup:
    if (NULL != kindexes) {
        free(kindexes);
    }
    return ret;
}

/*
 * Create a listen socket and bind to all interfaces
 */

static int mca_btl_tcp_component_create_listen(uint16_t af_family)
{
    int flags;
    int sd;
#if OPAL_WANT_IPV6
    struct sockaddr_in6 inaddr;
#else
    struct sockaddr_in inaddr;
#endif
    opal_socklen_t addrlen;

    /* create a listen socket for incoming connections */
    sd = socket(af_family, SOCK_STREAM, 0);
    if(sd < 0) {
        if (EAFNOSUPPORT != opal_socket_errno) {
            BTL_ERROR(("socket() failed: %s (%d)",
                       strerror(opal_socket_errno), opal_socket_errno));
        }
        return OMPI_ERR_IN_ERRNO;
    }

    /* we now have a socket. Assign it to the real mca_btl_tcp_component */
#if OPAL_WANT_IPV6
    if (AF_INET6 == af_family) {
        mca_btl_tcp_component.tcp6_listen_sd = sd;
        addrlen = sizeof(struct sockaddr_in6);
    } else {
        mca_btl_tcp_component.tcp_listen_sd = sd;
        addrlen = sizeof(struct sockaddr_in);
    }
#else
    mca_btl_tcp_component.tcp_listen_sd = sd;
    addrlen = sizeof(struct sockaddr_in);
#endif

    mca_btl_tcp_set_socket_options(sd);

    /* bind to all addresses and dynamically assigned port */
    memset(&inaddr, 0, sizeof(inaddr));
#if OPAL_WANT_IPV6
    {
        struct addrinfo hints, *res = NULL;
        int error;

        memset (&hints, 0, sizeof(hints));
        hints.ai_family = af_family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;

        if ((error = getaddrinfo(NULL, "0", &hints, &res))) {
            opal_output (0,
               "mca_btl_tcp_create_listen: unable to resolve. %s\n",
               gai_strerror (error));
               return ORTE_ERROR;
        }

        memcpy (&inaddr, res->ai_addr, res->ai_addrlen);
        addrlen = res->ai_addrlen;
        freeaddrinfo (res);

        /* in case of AF_INET6, disable v4-mapped addresses */
        if (AF_INET6 == af_family) {
            int flg = 0;
            if (setsockopt (sd, IPPROTO_IPV6, IPV6_V6ONLY,
                            &flg, sizeof (flg)) < 0) {
                opal_output(0,
                    "mca_btl_tcp_create_listen: unable to disable v4-mapped addresses\n");
            }
        }
    }
#else
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = INADDR_ANY;
    inaddr.sin_port = 0;
#endif

    if(bind(sd, (struct sockaddr*)&inaddr, addrlen) < 0) {
        BTL_ERROR(("bind() failed: %s (%d)",
                   strerror(opal_socket_errno), opal_socket_errno));
        return OMPI_ERROR;
    }

    /* resolve system assignend port */
    if(getsockname(sd, (struct sockaddr*)&inaddr, &addrlen) < 0) {
        BTL_ERROR(("getsockname() failed: %s (%d)",
                   strerror(opal_socket_errno), opal_socket_errno));
        return OMPI_ERROR;
    }

#if OPAL_WANT_IPV6
    if (AF_INET == af_family) {
        mca_btl_tcp_component.tcp_listen_port = inaddr.sin6_port;
    }
    if (AF_INET6 == af_family) {
        mca_btl_tcp_component.tcp6_listen_port = inaddr.sin6_port;
    }
#else
    mca_btl_tcp_component.tcp_listen_port = inaddr.sin_port;
#endif

    /* setup listen backlog to maximum allowed by kernel */
    if(listen(sd, SOMAXCONN) < 0) {
        BTL_ERROR(("listen() failed: %s (%d)", 
                   strerror(opal_socket_errno), opal_socket_errno));
        return OMPI_ERROR;
    }

    /* set socket up to be non-blocking, otherwise accept could block */
    if((flags = fcntl(sd, F_GETFL, 0)) < 0) {
        BTL_ERROR(("fcntl(F_GETFL) failed: %s (%d)",
                   strerror(opal_socket_errno), opal_socket_errno));
        return OMPI_ERROR;
    } else {
        flags |= O_NONBLOCK;
        if(fcntl(sd, F_SETFL, flags) < 0) {
            BTL_ERROR(("fcntl(F_SETFL) failed: %s (%d)",
                       strerror(opal_socket_errno), opal_socket_errno));
            return OMPI_ERROR;
        }
    }

    /* register listen port */
#if OPAL_WANT_IPV6
    if (AF_INET == af_family) {
        opal_event_set( &mca_btl_tcp_component.tcp_recv_event,
                        sd,
                        OPAL_EV_READ|OPAL_EV_PERSIST,
                        mca_btl_tcp_component_accept_handler,
                        0 );
        opal_event_add(&mca_btl_tcp_component.tcp_recv_event, 0);
    }

    if (AF_INET6 == af_family) {
        opal_event_set( &mca_btl_tcp_component.tcp6_recv_event,
                        sd,
                        OPAL_EV_READ|OPAL_EV_PERSIST,
                        mca_btl_tcp_component_accept_handler,
                        0 );
        opal_event_add(&mca_btl_tcp_component.tcp6_recv_event, 0);
    }
#else
    opal_event_set( &mca_btl_tcp_component.tcp_recv_event,
                    mca_btl_tcp_component.tcp_listen_sd, 
                    OPAL_EV_READ|OPAL_EV_PERSIST, 
                    mca_btl_tcp_component_accept_handler, 
                    0 );
    opal_event_add(&mca_btl_tcp_component.tcp_recv_event,0);
#endif
    return OMPI_SUCCESS;
}

/*
 *  Register TCP module addressing information. The MCA framework
 *  will make this available to all peers. 
 */

static int mca_btl_tcp_component_exchange(void)
{
     int rc = 0, index;
     size_t i = 0;
     size_t size = mca_btl_tcp_component.tcp_addr_count * 
                   mca_btl_tcp_component.tcp_num_links * sizeof(mca_btl_tcp_addr_t);
     /* adi@2007-04-12:
      *
      * We'll need to explain things a bit here:
      *    1. We normally have as many BTLs as physical NICs.
      *    2. With num_links, we now have num_btl = num_links * #NICs
      *    3. we might have more than one address per NIC
      */
     size_t xfer_size = 0; /* real size to transfer (may differ from 'size') */
     size_t current_addr = 0;

     if(mca_btl_tcp_component.tcp_num_btls != 0) {
         mca_btl_tcp_addr_t *addrs = (mca_btl_tcp_addr_t *)malloc(size);
         memset(addrs, 0, size);

         /* here we start populating our addresses */
         for( i = 0; i < mca_btl_tcp_component.tcp_num_btls; i++ ) {
             for (index = opal_ifbegin(); index >= 0;
                     index = opal_ifnext(index)) {
                 struct sockaddr_storage my_ss;

                 /* look if the address belongs to this (enabled) NIC.
                  * If not, go to next address
                  */
                 if (opal_ifindextokindex (index) !=
                         mca_btl_tcp_component.tcp_btls[i]->tcp_ifkindex) {
                     continue;
                 }

                 if (OPAL_SUCCESS != 
                     opal_ifindextoaddr(index, (struct sockaddr*) &my_ss,
                                        sizeof (my_ss))) {
                     opal_output (0, 
                             "btl_tcp_component: problems getting address for index %i (kernel index %i)\n",
                             index, opal_ifindextokindex (index));
                     continue;
                 }

                 if ((AF_INET == my_ss.ss_family) &&
                     (4 != mca_btl_tcp_component.tcp_disable_family)) {
                     memcpy(&addrs[current_addr].addr_inet, 
                             &((struct sockaddr_in*)&my_ss)->sin_addr,
                             sizeof(addrs[0].addr_inet));
                     addrs[current_addr].addr_port = 
                         mca_btl_tcp_component.tcp_listen_port;
                     addrs[current_addr].addr_family = MCA_BTL_TCP_AF_INET;
                     xfer_size += sizeof (mca_btl_tcp_addr_t);
                     addrs[current_addr].addr_inuse   = 0;
                     addrs[current_addr].addr_ifkindex =
                         opal_ifindextokindex (index);
                     current_addr++;
                 }
#if OPAL_WANT_IPV6
                 if ((AF_INET6 == my_ss.ss_family) &&
                     (6 != mca_btl_tcp_component.tcp_disable_family)) {
                     memcpy(&addrs[current_addr].addr_inet,
                             &((struct sockaddr_in6*)&my_ss)->sin6_addr,
                             sizeof(addrs[0].addr_inet));
                     addrs[current_addr].addr_port = 
                         mca_btl_tcp_component.tcp6_listen_port;
                     addrs[current_addr].addr_family = MCA_BTL_TCP_AF_INET6;
                     xfer_size += sizeof (mca_btl_tcp_addr_t);
                     addrs[current_addr].addr_inuse   = 0;
                     addrs[current_addr].addr_ifkindex =
                         opal_ifindextokindex (index);
                     current_addr++;
                 }
#endif
             } /* end of for opal_ifbegin() */
         } /* end of for tcp_num_btls */
         rc =  mca_pml_base_modex_send(&mca_btl_tcp_component.super.btl_version,
	                               addrs, xfer_size);
         free(addrs);
     } /* end if */
     return rc;
}

/*
 *  TCP module initialization:
 *  (1) read interface list from kernel and compare against module parameters
 *      then create a BTL instance for selected interfaces
 *  (2) setup TCP listen socket for incoming connection attempts
 *  (3) register BTL parameters with the MCA
 */
mca_btl_base_module_t** mca_btl_tcp_component_init(int *num_btl_modules, 
                                                   bool enable_progress_threads,
                                                   bool enable_mpi_threads)
{
    int ret;
    mca_btl_base_module_t **btls;
    *num_btl_modules = 0;

    /* initialize free lists */
    ompi_free_list_init( &mca_btl_tcp_component.tcp_frag_eager,
                         sizeof (mca_btl_tcp_frag_eager_t) + 
                         mca_btl_tcp_module.super.btl_eager_limit,
                         OBJ_CLASS (mca_btl_tcp_frag_eager_t),
                         mca_btl_tcp_component.tcp_free_list_num,
                         mca_btl_tcp_component.tcp_free_list_max,
                         mca_btl_tcp_component.tcp_free_list_inc,
                         NULL );
                                                                                                                                  
    ompi_free_list_init( &mca_btl_tcp_component.tcp_frag_max,
                         sizeof (mca_btl_tcp_frag_max_t) + 
                         mca_btl_tcp_module.super.btl_max_send_size,
                         OBJ_CLASS (mca_btl_tcp_frag_max_t),
                         mca_btl_tcp_component.tcp_free_list_num,
                         mca_btl_tcp_component.tcp_free_list_max,
                         mca_btl_tcp_component.tcp_free_list_inc,
                         NULL );
                                                                                                                                  
    ompi_free_list_init( &mca_btl_tcp_component.tcp_frag_user,
                         sizeof (mca_btl_tcp_frag_user_t),
                         OBJ_CLASS (mca_btl_tcp_frag_user_t),
                         mca_btl_tcp_component.tcp_free_list_num,
                         mca_btl_tcp_component.tcp_free_list_max,
                         mca_btl_tcp_component.tcp_free_list_inc,
                         NULL );
                                                                                                                                  
    /* create a BTL TCP module for selected interfaces */
    if(mca_btl_tcp_component_create_instances() != OMPI_SUCCESS) {
        return 0;
    }

    /* create a TCP listen socket for incoming connection attempts */
    if(mca_btl_tcp_component_create_listen(AF_INET) != OMPI_SUCCESS) {
        return 0;
    }
#if OPAL_WANT_IPV6
    if((ret = mca_btl_tcp_component_create_listen(AF_INET6)) != OMPI_SUCCESS) {
        if (!(OMPI_ERR_IN_ERRNO == ret && EAFNOSUPPORT == opal_socket_errno)) {
            opal_output (0, "mca_btl_tcp_component: IPv6 listening socket failed\n");
            return 0;
        }
    }
#endif

    /* publish TCP parameters with the MCA framework */
    if(mca_btl_tcp_component_exchange() != OMPI_SUCCESS) {
        return 0;
    }

    btls = (mca_btl_base_module_t **)malloc(mca_btl_tcp_component.tcp_num_btls * 
                  sizeof(mca_btl_base_module_t*));
    if(NULL == btls) {
        return NULL;
    }

    memcpy(btls, mca_btl_tcp_component.tcp_btls, mca_btl_tcp_component.tcp_num_btls*sizeof(mca_btl_tcp_module_t*));
    *num_btl_modules = mca_btl_tcp_component.tcp_num_btls;
    return btls;
}

/*
 *  TCP module control
 */

int mca_btl_tcp_component_control(int param, void* value, size_t size)
{
    return OMPI_SUCCESS;
}


/**
 * Called by the event engine when the listening socket has
 * a connection event. Accept the incoming connection request
 * and queue them for completion of the connection handshake.
 */
static void mca_btl_tcp_component_accept_handler( int incoming_sd,
                                                  short ignored,
                                                  void* unused )
{
    while(true) {
#if OPAL_WANT_IPV6
        struct sockaddr_in6 addr;
#else
        struct sockaddr_in addr;
#endif
        opal_socklen_t addrlen = sizeof(addr);

        mca_btl_tcp_event_t *event;
        int sd = accept(incoming_sd, (struct sockaddr*)&addr, &addrlen);
        if(sd < 0) {
            if(opal_socket_errno == EINTR)
                continue;
            if(opal_socket_errno != EAGAIN && opal_socket_errno != EWOULDBLOCK)
                BTL_ERROR(("accept() failed: %s (%d).", 
                           strerror(opal_socket_errno), opal_socket_errno));
            return;
        }
        mca_btl_tcp_set_socket_options(sd);

        /* wait for receipt of peers process identifier to complete this connection */
         
        event = OBJ_NEW(mca_btl_tcp_event_t);
        opal_event_set(&event->event, sd, OPAL_EV_READ, mca_btl_tcp_component_recv_handler, event);
        opal_event_add(&event->event, 0);
    }
}


/**
 * Event callback when there is data available on the registered 
 * socket to recv. This callback is triggered only once per lifetime
 * for any socket, in the beginning when we setup the handshake
 * protocol.
 */
static void mca_btl_tcp_component_recv_handler(int sd, short flags, void* user)
{
    orte_process_name_t guid;
    struct sockaddr_storage addr;
    int retval;
    mca_btl_tcp_proc_t* btl_proc;
    opal_socklen_t addr_len = sizeof(addr);
    mca_btl_tcp_event_t *event = (mca_btl_tcp_event_t *)user;

    OBJ_RELEASE(event);

    /* recv the process identifier */
    retval = recv(sd, (char *)&guid, sizeof(guid), 0);
    if(retval != sizeof(guid)) {
        CLOSE_THE_SOCKET(sd);
        return;
    }
    ORTE_PROCESS_NAME_NTOH(guid);

    /* now set socket up to be non-blocking */
    if((flags = fcntl(sd, F_GETFL, 0)) < 0) {
        BTL_ERROR(("fcntl(F_GETFL) failed: %s (%d)",
                   strerror(opal_socket_errno), opal_socket_errno));
    } else {
        flags |= O_NONBLOCK;
        if(fcntl(sd, F_SETFL, flags) < 0) {
            BTL_ERROR(("fcntl(F_SETFL) failed: %s (%d)",
                       strerror(opal_socket_errno), opal_socket_errno));
        }
    }
   
    /* lookup the corresponding process */
    btl_proc = mca_btl_tcp_proc_lookup(&guid);
    if(NULL == btl_proc) {
        CLOSE_THE_SOCKET(sd);
        return;
    }

    /* lookup peer address */
    if(getpeername(sd, (struct sockaddr*)&addr, &addr_len) != 0) {
        BTL_ERROR(("getpeername() failed: %s (%d)", 
                   strerror(opal_socket_errno), opal_socket_errno));
        CLOSE_THE_SOCKET(sd);
        return;
    }

    /* are there any existing peer instances will to accept this connection */
    if(mca_btl_tcp_proc_accept(btl_proc, (struct sockaddr*)&addr, sd) == false) {
        CLOSE_THE_SOCKET(sd);
        return;
    }
}

