/*	$OpenBSD: amd7930var.h,v 1.3 1997/08/08 08:24:38 downsj Exp $	*/
/*	$NetBSD: amd7930var.h,v 1.3 1996/02/01 22:32:25 mycroft Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)bsd_audiovar.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _LOCORE

/* XXX I think these defines should go into some other header file */
#define SUNAUDIO_MIC_PORT	0
#define SUNAUDIO_SPEAKER	1
#define SUNAUDIO_HEADPHONES	2
#define SUNAUDIO_MONITOR	3
#define SUNAUDIO_INPUT_CLASS	4
#define SUNAUDIO_OUTPUT_CLASS	5

struct auio {
	volatile struct amd7930 *au_amd;/* chip registers */

	u_char	*au_rdata;		/* record data */
	u_char	*au_rend;		/* end of record data */
	u_char	*au_pdata;		/* play data */
	u_char	*au_pend;		/* end of play data */
	struct	evcnt au_intrcnt;	/* statistics */
};

/*
 * Chip interface
 */
struct mapreg {
        u_short mr_x[8];
        u_short mr_r[8];
        u_short mr_gx;
        u_short mr_gr;
        u_short mr_ger;
        u_short mr_stgr;
        u_short mr_ftgr;
        u_short mr_atgr;
        u_char  mr_mmr1;
        u_char  mr_mmr2;
};

#endif /* !_LOCORE */
