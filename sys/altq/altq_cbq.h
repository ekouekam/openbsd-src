/*	$OpenBSD: altq_cbq.h,v 1.1 2001/06/27 05:28:34 kjc Exp $	*/
/*	$KAME: altq_cbq.h,v 1.5 2000/12/02 13:44:40 kjc Exp $	*/

/*
 * Copyright (c) Sun Microsystems, Inc. 1993-1998 All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the SMCC Technology
 *      Development Group at Sun Microsystems, Inc.
 *
 * 4. The name of the Sun Microsystems, Inc nor may not be used to endorse or
 *      promote products derived from this software without specific prior
 *      written permission.
 *
 * SUN MICROSYSTEMS DOES NOT CLAIM MERCHANTABILITY OF THIS SOFTWARE OR THE
 * SUITABILITY OF THIS SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is
 * provided "as is" without express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

#ifndef _ALTQ_ALTQ_CBQ_H_
#define	_ALTQ_ALTQ_CBQ_H_

#include <sys/ioccom.h>
#include <altq/altq.h>
#include <altq/altq_rmclass.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>

/* #pragma ident "@(#)cbq.h  1.18     98/05/13 SMI" */

#ifdef __cplusplus
extern "C" {
#endif 

/*
 * Define a well known class handles
 */
#define	NULL_CLASS_HANDLE	0xffffffff
#define	ROOT_CLASS_HANDLE	0xfffffffe
#define	DEFAULT_CLASS_HANDLE	0xfffffffd
#define	CTL_CLASS_HANDLE	0xfffffffc

/*
 * Define structures associated with IOCTLS for cbq.
 */

/*
 * Define the CBQ interface structure.  This must be included in all
 * IOCTL's such that the CBQ driver may find the appropriate CBQ module
 * associated with the network interface to be affected.
 */
struct cbq_interface {
	char	cbq_ifacename[IFNAMSIZ];
};

typedef struct cbq_class_spec {
	u_int		priority;
	u_int		nano_sec_per_byte;
	u_int		maxq;
	u_int		maxidle;
	int		minidle;	
	u_int		offtime;
	u_long		parent_class_handle;
	u_long		borrow_class_handle;

	u_int		pktsize;
	int		flags;
} cbq_class_spec_t;

/* class flags shoud be same as class flags in rm_class.h */
#define	CBQCLF_RED		0x0001	/* use RED */
#define	CBQCLF_ECN		0x0002  /* use RED/ECN */
#define	CBQCLF_RIO		0x0004  /* use RIO */
#define	CBQCLF_FLOWVALVE	0x0008	/* use flowvalve (aka penalty-box) */
#define	CBQCLF_CLEARDSCP	0x0010  /* clear diffserv codepoint */

/* class flags only for root class */
#define	CBQCLF_WRR		0x0100	/* weighted-round robin */
#define	CBQCLF_EFFICIENT	0x0200  /* work-conserving */

/* class flags for special classes */
#define	CBQCLF_ROOTCLASS	0x1000	/* root class */
#define	CBQCLF_DEFCLASS		0x2000	/* default class */
#define	CBQCLF_CTLCLASS		0x4000	/* control class */
#define	CBQCLF_CLASSMASK	0xf000	/* class mask */

#define	CBQ_MAXQSIZE	200

struct cbq_add_class {
	struct cbq_interface	cbq_iface;

	cbq_class_spec_t	cbq_class;	
	u_long			cbq_class_handle;
};

struct cbq_delete_class {
	struct cbq_interface	cbq_iface;
	u_long			cbq_class_handle;
};

struct cbq_modify_class {
	struct cbq_interface	cbq_iface;

	cbq_class_spec_t	cbq_class;	
	u_long			cbq_class_handle;
};

struct cbq_add_filter {
	struct cbq_interface		cbq_iface;
	u_long			cbq_class_handle;
	struct flow_filter	cbq_filter;

	u_long			cbq_filter_handle;
};

struct cbq_delete_filter {
	struct cbq_interface	cbq_iface;
	u_long			cbq_filter_handle;
};

typedef struct _cbq_class_stats_ {
	u_int		handle;
	u_int		depth;

	struct pktcntr	xmit_cnt;	/* packets sent in this class */
	struct pktcntr	drop_cnt;	/* dropped packets */
	u_int		over;		/* # times went over limit */
	u_int		borrows;	/* # times tried to borrow */
	u_int		overactions;	/* # times invoked overlimit action */
	u_int		delays;		/* # times invoked delay actions */

	/* other static class parameters useful for debugging */
	int		priority;
	int		maxidle;
	int		minidle;
	int		offtime;
	int		qmax;
	int		ns_per_byte;
	int		wrr_allot;

	int		qcnt;		/* # packets in queue */
	int		avgidle;

	/* red and rio related info */
	int		qtype;
	struct redstats	red[3];
} class_stats_t;

/* number of classes are returned in nclasses field */
struct cbq_getstats {
	struct cbq_interface	iface;
	int			nclasses;
	class_stats_t		*stats;
};

/* 
 * Define IOCTLs for CBQ.
 */
#define	CBQ_IF_ATTACH		_IOW('Q', 1, struct cbq_interface)
#define	CBQ_IF_DETACH		_IOW('Q', 2, struct cbq_interface)
#define	CBQ_ENABLE		_IOW('Q', 3, struct cbq_interface)
#define	CBQ_DISABLE		_IOW('Q', 4, struct cbq_interface)
#define	CBQ_CLEAR_HIERARCHY	_IOW('Q', 5, struct cbq_interface)
#define	CBQ_ADD_CLASS		_IOWR('Q', 7, struct cbq_add_class)
#define	CBQ_DEL_CLASS		_IOW('Q', 8, struct cbq_delete_class)
#define	CBQ_MODIFY_CLASS	_IOWR('Q', 9, struct cbq_modify_class)
#define	CBQ_ADD_FILTER		_IOWR('Q', 10, struct cbq_add_filter)
#define	CBQ_DEL_FILTER		_IOW('Q', 11, struct cbq_delete_filter)
#define	CBQ_GETSTATS		_IOWR('Q', 12, struct cbq_getstats)

#ifdef _KERNEL
/*
 * Define macros only good for kernel drivers and modules.
 */

#define	DISABLE		0x00
#define	ENABLE		0x01

#define	CBQ_WATCHDOG    	(HZ / 20)
#define	CBQ_TIMEOUT		10
#define	CBQ_LS_TIMEOUT		(20 * hz / 1000)

#define	CBQ_MAX_CLASSES	256
#define	CBQ_MAX_FILTERS 256

/*
 * Define State structures.
 */
typedef struct cbqstate {
	struct cbqstate		*cbq_next;
	int			cbq_qlen;	/* # of packets in cbq */
	struct rm_class		**cbq_class_tbl;

	struct rm_ifdat		ifnp;
	struct callout		cbq_callout;	/* for timeouts */

	struct acc_classifier	cbq_classifier;
} cbq_state_t;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif 

#endif /* !_ALTQ_ALTQ_CBQ_H_ */
