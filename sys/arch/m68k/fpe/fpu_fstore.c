/*	$NetBSD: fpu_fstore.c,v 1.2 1995/11/05 00:35:29 briggs Exp $	*/

/*
 * Copyright (c) 1995 Ken Nakata
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <machine/frame.h>

#include "fpu_emulate.h"

/*
 * type 0: fmove mem/fpr->fpr
 * In this function, we know
 *	(opcode & 0x01c0) == 0
 *	(word1 & 0xe000) == 0x6000
 */
int
fpu_emul_fstore(fe, insn)
     struct fpemu *fe;
     struct instruction *insn;
{
    struct frame *frame = fe->fe_frame;
    u_int *fpregs = fe->fe_fpframe->fpf_regs;
    int word1, sig;
    int regnum;
    int format;
    u_int buf[3];
    u_int flags;
    char regname;

    if (fpu_debug_level & DL_FSTORE) {
	printf("  fpu_emul_fstore: frame at %08x fpframe at %08x\n",
	       frame, fe->fe_fpframe);
    }

    word1 = insn->is_word1;
    format = (word1 >> 10) & 7;
    regnum = (word1 >> 7) & 7;

    insn->is_advance = 4;

    if (format == FTYPE_DBL) {
	insn->is_datasize = 8;
    } else if (format == FTYPE_SNG || format == FTYPE_LNG) {
	insn->is_datasize = 4;
    } else if (format == FTYPE_WRD) {
	insn->is_datasize = 2;
	format = FTYPE_LNG;
    } else if (format == FTYPE_BYT) {
	insn->is_datasize = 1;
	format = FTYPE_LNG;
    } else if (format == FTYPE_EXT) {
	insn->is_datasize = 12;
    } else {
	/* invalid or unsupported operand format */
	if (fpu_debug_level & DL_FSTORE) {
	    printf("  fpu_emul_fstore: invalid format %d\n", format);
	}
	sig = SIGFPE;
    }
    if (fpu_debug_level & DL_FSTORE) {
	printf("  fpu_emul_fstore: format %d, size %d\n",
	       format, insn->is_datasize);
    }

    /* Get effective address. (modreg=opcode&077) */
    sig = fpu_decode_ea(frame, insn, &insn->is_ea0, insn->is_opcode);
    if (sig) {
	if (fpu_debug_level & DL_FSTORE) {
	    printf("  fpu_emul_fstore: failed in decode_ea sig=%d\n", sig);
	}
	return sig;
    }

    if (insn->is_datasize > 4 && insn->is_ea0.ea_flags == EA_DIRECT) {
	/* trying to store dbl or ext into a data register */
#ifdef DEBUG
	printf("  fpu_fstore: attempted to store dbl/ext to reg\n");
#endif
	return SIGILL;
    }

    if (fpu_debug_level & DL_OPERANDS)
	printf("  fpu_emul_fstore: saving FP%d (%08x,%08x,%08x)\n",
	       regnum, fpregs[regnum * 3], fpregs[regnum * 3 + 1],
	       fpregs[regnum * 3 + 2]);
    fpu_explode(fe, &fe->fe_f3, FTYPE_EXT, &fpregs[regnum * 3]);
    if (fpu_debug_level & DL_VALUES) {
	static char *class_name[] = { "SNAN", "QNAN", "ZERO", "NUM", "INF" };
	printf("  fpu_emul_fstore: fpn (%s,%c,%d,%08x,%08x,%08x,%08x)\n",
	       class_name[fe->fe_f3.fp_class + 2],
	       fe->fe_f3.fp_sign ? '-' : '+', fe->fe_f3.fp_exp,
	       fe->fe_f3.fp_mant[0], fe->fe_f3.fp_mant[1],
	       fe->fe_f3.fp_mant[2], fe->fe_f3.fp_mant[3]);
    }
    fpu_implode(fe, &fe->fe_f3, format, buf);

    fpu_store_ea(frame, insn, &insn->is_ea0, (char *)buf);
    if (fpu_debug_level & DL_RESULT)
	printf("  fpu_emul_fstore: %08x,%08x,%08x size %d\n",
	       buf[0], buf[1], buf[2], insn->is_datasize);

    return 0;
}
