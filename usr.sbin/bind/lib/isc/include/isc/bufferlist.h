/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: bufferlist.h,v 1.10 2001/01/09 21:56:48 bwelling Exp $ */

#ifndef ISC_BUFFERLIST_H
#define ISC_BUFFERLIST_H 1

/*****
 ***** Module Info
 *****/

/*
 * Buffer Lists
 *
 *	Buffer lists have no synchronization.  Clients must ensure exclusive
 *	access.
 *
 * Reliability:
 *	No anticipated impact.

 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/***
 *** Functions
 ***/

unsigned int
isc_bufferlist_usedcount(isc_bufferlist_t *bl);
/*
 * Return the length of the sum of all used regions of all buffers in
 * the buffer list 'bl'
 *
 * Requires:
 *
 *	'bl' is not NULL.
 *
 * Returns:
 *	sum of all used regions' lengths.
 */

unsigned int
isc_bufferlist_availablecount(isc_bufferlist_t *bl);
/*
 * Return the length of the sum of all available regions of all buffers in
 * the buffer list 'bl'
 *
 * Requires:
 *
 *	'bl' is not NULL.
 *
 * Returns:
 *	sum of all available regions' lengths.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_BUFFERLIST_H */
