/*	$OpenBSD: process_machdep.c,v 1.5 2002/03/14 00:42:24 miod Exp $	*/
/*	$NetBSD: process_machdep.c,v 1.1 1996/09/30 16:34:53 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <machine/reg.h>

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = trapframe(p);

	bcopy(&(tf->fixreg[0]), &(regs->gpr[0]), sizeof(regs->gpr));
	bzero(&(regs->fpr[0]), sizeof(regs->fpr));
	/* 
	 * need to do floating point here
	 */
	regs->pc  = tf->srr0;
	regs->ps  = tf->srr1; /* is this the correct value for this ? */
	regs->cnd = tf->cr;
	regs->lr  = tf->lr;
	regs->cnt = tf->ctr;
	regs->xer = tf->xer;
	regs->mq  = 0; /*  what should this really be? */

	return (0);
}

#ifdef PTRACE

/*
 * Set the process's program counter.
 */
int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	struct trapframe *tf = trapframe(p);
	
	tf->srr0 = (int)addr;
	return 0;
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	struct trapframe *tf = trapframe(p);
	
	if (sstep)
		tf->srr1 |= PSL_SE;
	else
		tf->srr1 &= ~PSL_SE;
	return 0;
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf = trapframe(p);

	bcopy(&(regs->gpr[0]), &(tf->fixreg[0]), sizeof(regs->gpr));
	/* 
	 * need to do floating point here
	 */
	tf->srr0 = regs->pc;
	tf->srr1 = regs->ps;  /* is this the correct value for this ? */
	tf->cr   = regs->cnd;
	tf->lr   = regs->lr;
	tf->ctr  = regs->cnt;
	tf->xer  = regs->xer;
	/*  regs->mq = 0; what should this really be? */

	return (0);
}

#endif	/* PTRACE */
