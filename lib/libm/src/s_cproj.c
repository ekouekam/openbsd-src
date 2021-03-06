/*	$OpenBSD: s_cproj.c,v 1.5 2013/01/13 03:45:00 martynas Exp $	*/
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

#include <complex.h>
#include <float.h>
#include <math.h>

double complex
cproj(double complex z)
{
	double complex res;

	if (isinf(__real__ z) || isinf(__imag__ z)) {
		__real__ res = INFINITY;
		__imag__ res = copysign(0.0, __imag__ z);
	} else {
		res = z;
	}

	return res;
}

#if	LDBL_MANT_DIG == 53
__weak_alias(cprojl, cproj);
#endif	/* LDBL_MANT_DIG == 53 */
