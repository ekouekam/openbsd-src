/*	$OpenBSD: privsep.h,v 1.6 2012/10/30 18:39:44 krw Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE, ABUSE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

struct buf {
	u_char			*buf;
	size_t			 size;
	size_t			 wpos;
	size_t			 rpos;
};

enum imsg_code {
	IMSG_NONE,
	IMSG_DELETE_ADDRESS,
	IMSG_ADD_ADDRESS,
	IMSG_FLUSH_ROUTES,
	IMSG_ADD_DEFAULT_ROUTE,
	IMSG_NEW_RESOLV_CONF
};

struct imsg_hdr {
	enum imsg_code	code;
	size_t		len;
};

struct buf	*buf_open(size_t);
void		 buf_add(struct buf *, void *, size_t);
void		 buf_close(int, struct buf *);
void		 buf_read(int, void *, size_t);
void		 dispatch_imsg(int);
