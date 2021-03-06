/*	$OpenBSD: signbit.c,v 1.5 2012/12/05 23:19:59 deraadt Exp $	*/
/*
 * Copyright (c) 2008 Martynas Venckus <martynas@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* LINTLIBRARY */

#include <sys/types.h>
#include <machine/vaxfp.h>
#include <math.h>

int
__signbit(double d)
{
	struct vax_d_floating *p = (struct vax_d_floating *)&d;

	return p->dflt_sign;
}

int
__signbitf(float f)
{
	struct vax_f_floating *p = (struct vax_f_floating *)&f;

	return p->fflt_sign;
}

#ifdef	lint
/* PROTOLIB1 */
int __signbitl(long double);
#else	/* lint */
__weak_alias(__signbitl, __signbit);
#endif	/* lint */
