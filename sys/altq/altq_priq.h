/*	$OpenBSD: altq_priq.h,v 1.5 2003/04/03 14:31:02 henning Exp $	*/
/*	$KAME: altq_priq.h,v 1.1 2000/10/18 09:15:23 kjc Exp $	*/
/*
 * Copyright (C) 2000-2002
 *	Sony Computer Science Laboratories Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ALTQ_ALTQ_PRIQ_H_
#define	_ALTQ_ALTQ_PRIQ_H_

#include <altq/altq.h>
#include <altq/altq_classq.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	PRIQ_MAXPRI	16	/* upper limit of the number of priorities */
#define	PRIQ_MAXQID	256	/* upper limit of queues */

/* priq class flags */
#define	PRCF_RED		0x0001	/* use RED */
#define	PRCF_ECN		0x0002  /* use RED/ECN */
#define	PRCF_RIO		0x0004  /* use RIO */
#define	PRCF_CLEARDSCP		0x0010  /* clear diffserv codepoint */
#define	PRCF_DEFAULTCLASS	0x1000	/* default class */

/* special class handles */
#define	PRIQ_NULLCLASS_HANDLE	0

struct priq_classstats {
	u_int32_t		class_handle;

	u_int			qlength;
	u_int			qlimit;
	u_int			period;
	struct pktcntr		xmitcnt;  /* transmitted packet counter */
	struct pktcntr		dropcnt;  /* dropped packet counter */

	/* red and rio related info */
	int			qtype;
	struct redstats		red[3];	/* rio has 3 red stats */
};

#ifdef _KERNEL

struct priq_class {
	u_int32_t	cl_handle;	/* class handle */
	class_queue_t	*cl_q;		/* class queue structure */
	struct red	*cl_red;	/* RED state */
	int		cl_pri;		/* priority */
	int		cl_flags;	/* class flags */
	struct priq_if	*cl_pif;	/* back pointer to pif */
	struct altq_pktattr *cl_pktattr; /* saved header used by ECN */

	/* statistics */
	u_int		cl_period;	/* backlog period */
	struct pktcntr  cl_xmitcnt;	/* transmitted packet counter */
	struct pktcntr  cl_dropcnt;	/* dropped packet counter */
};

/*
 * priq interface state
 */
struct priq_if {
	struct priq_if		*pif_next;	/* interface state list */
	struct ifaltq		*pif_ifq;	/* backpointer to ifaltq */
	u_int			pif_bandwidth;	/* link bandwidth in bps */
	int			pif_maxpri;	/* max priority in use */
	struct priq_class	*pif_default;	/* default class */
	struct priq_class	*pif_classes[PRIQ_MAXPRI]; /* classes */
};

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _ALTQ_ALTQ_PRIQ_H_ */
