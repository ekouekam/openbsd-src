/* $OpenBSD: strtonum.c,v 1.1 2004/05/03 17:09:24 tedu Exp $ */
/*
 * Copyright (c) 2004 Ted Unangst and Todd Miller
 * All rights reserved.
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define INVALID 	1
#define TOOSMALL 	2
#define TOOLARGE 	3

unsigned long long
strtonum(const char *numstr, long long minval, unsigned long long maxval,
    const char **errstrp)
{
	unsigned long long ull;
	char *ep;
	int error;
	struct errval {
		const char *errstr;
		int errno;
	} ev[4] = {
		{ NULL,		0 },
		{ "invalid",	EINVAL },
		{ "too small",	ERANGE },
		{ "too large",	ERANGE },
	};

	ev[0].errno = errno;
	errno = 0;
	error = 0;
	ull = 0;
	if (minval > maxval || maxval < minval ||
	    (minval < 0 && maxval > LLONG_MAX))
		error = INVALID;
	else if (minval >= 0) {
		ull = strtoull(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ull == ULLONG_MAX && errno == ERANGE) || ull > maxval)
			error = TOOLARGE;
	} else {
		long long ll = strtoll(numstr, &ep, 10);
		if (numstr == ep || *ep != '\0')
			error = INVALID;
		else if ((ll == LLONG_MIN && errno == ERANGE) || ll < minval)
			error = TOOSMALL;
		else if ((ll == LLONG_MAX && errno == ERANGE) || ll > maxval)
			error = TOOLARGE;
		ull = (unsigned long long)ll;
	}
	if (errstrp != NULL)
		*errstrp = ev[error].errstr;
	errno = ev[error].errno;
	if (error)
		ull = 0;

	return (ull);
}
