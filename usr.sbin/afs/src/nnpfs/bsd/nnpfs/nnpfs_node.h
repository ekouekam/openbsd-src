/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* 	$arla: nnpfs_node.h,v 1.33 2003/01/25 18:48:28 lha Exp $	 */

#ifndef _nnpfs_xnode_h
#define _nnpfs_xnode_h

#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_KERNEL_LF_ADVLOCK
#include <sys/lockf.h>
#endif

#include <nnpfs/nnpfs_attr.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_queue.h>

#ifdef __APPLE__
typedef struct lock__bsd__ nnpfs_vnode_lock;
#else
typedef struct lock nnpfs_vnode_lock;
#endif

struct nnpfs_node {
#if defined(__NetBSD_Version__) && __NetBSD_Version__ >= 105280000
    struct genfs_node gnode;
#endif
    struct vnode *vn;
    struct vnode *data;
    struct vattr attr;
    uint32_t offset;
    u_int flags;
    u_int tokens;
    nnpfs_handle handle;
    nnpfs_pag_t id[MAXRIGHTS];
    u_char rights[MAXRIGHTS];
    u_char anonrights;
#if defined(HAVE_KERNEL_LOCKMGR) || defined(HAVE_KERNEL_DEBUGLOCKMGR)
    nnpfs_vnode_lock lock;
#else
    int vnlocks;
#endif
#ifdef HAVE_KERNEL_LF_ADVLOCK
    struct   lockf *lockf;
#endif
    struct ucred *rd_cred;
    struct ucred *wr_cred;
    NNPQUEUE_ENTRY(nnpfs_node) nn_hash;
};

#define XN_HASHSIZE	101

NNPQUEUE_HEAD(nh_node_list, nnpfs_node);

struct nnpfs_nodelist_head {
    struct nh_node_list	nh_nodelist[XN_HASHSIZE];
};

void	nnfs_init_head(struct nnpfs_nodelist_head *);
void	nnpfs_node_purge(struct nnpfs_nodelist_head *,
			 void (*func)(struct nnpfs_node *));
struct nnpfs_node *
	nnpfs_node_find(struct nnpfs_nodelist_head *, nnpfs_handle *);
void	nnpfs_remove_node(struct nnpfs_nodelist_head *, struct nnpfs_node *);
void	nnpfs_insert(struct nnpfs_nodelist_head *, struct nnpfs_node *);
int	nnpfs_update_handle(struct nnpfs_nodelist_head *, nnpfs_handle *, 
			    nnpfs_handle *);


struct nnpfs;

int nnpfs_getnewvnode(struct nnpfs *nnpfsp, struct vnode **vpp,
		      struct nnpfs_handle *handle);


#define DATA_FROM_VNODE(vp) DATA_FROM_XNODE(VNODE_TO_XNODE(vp))

#define DATA_FROM_XNODE(xp) ((xp)->data)

#define XNODE_TO_VNODE(xp) ((xp)->vn)
#define VNODE_TO_XNODE(vp) ((struct nnpfs_node *) (vp)->v_data)

#if defined(HAVE_ONE_ARGUMENT_VGET)
#define nnpfs_do_vget(vp, lockflag, proc) vget((vp))
#elif defined(HAVE_TWO_ARGUMENT_VGET)
#define nnpfs_do_vget(vp, lockflag, proc) vget((vp), (lockflag))
#elif defined(HAVE_THREE_ARGUMENT_VGET)
#define nnpfs_do_vget(vp, lockflag, proc) vget((vp), (lockflag), (proc))
#else
#error what kind of vget
#endif

#ifndef HAVE_VOP_T
typedef int vop_t (void *);
#endif

#ifdef LK_INTERLOCK
#define HAVE_LK_INTERLOCK
#else
#define LK_INTERLOCK 0
#endif

#ifdef LK_RETRY
#define HAVE_LK_RETRY
#else
#define LK_RETRY 0
#endif

/*
 * This is compat code for older vfs that have a 
 * vget that only take a integer (really boolean) argument
 * that the the returned vnode will be returned locked
 */

#ifdef LK_EXCLUSIVE
#define HAVE_LK_EXCLUSIVE 1
#else
#define LK_EXCLUSIVE 1
#endif

#ifdef LK_SHARED
#define HAVE_LK_SHARED 1
#else
#define LK_SHARED 1
#endif

void	nnpfs_update_write_cred(struct nnpfs_node *, struct ucred *);
void	nnpfs_update_read_cred(struct nnpfs_node *, struct ucred *);

#endif				       /* _nnpfs_xnode_h */
