/*	$OpenBSD: board.c,v 1.7 2003/06/03 03:01:38 millert Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)board.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: board.c,v 1.7 2003/06/03 03:01:38 millert Exp $";
#endif
#endif /* not lint */

#include "back.h"

static int i, j, k;
static char ln[60];

void
wrboard()
{
	int     l;
	static const char bl[] =
	"|                       |   |                       |\n";
	static const char sv[] =
	"|                       |   |                       |    \n";

	clear();

	fboard();
	goto lastline;
	addstr("_____________________________________________________\n");
	addstr(bl);
	strlcpy(ln, bl, sizeof ln);
	for (j = 1; j < 50; j += 4) {
		k = j / 4 + (j > 24 ? 12 : 13);
		ln[j + 1] = k % 10 + '0';
		ln[j]     = k / 10 + '0';
		if (j == 21)
			j += 4;
	}
	addstr(ln);
	for (i = 0; i < 5; i++) {
		strlcpy(ln, sv, sizeof ln);
		for (j = 1; j < 50; j += 4) {
			k = j / 4 + (j > 24 ? 12 : 13);
			wrbsub();
			if (j == 21)
				j += 4;
		}
		if (-board[25] > i)
			ln[26] = 'w';
		if (-board[25] > i + 5)
			ln[25] = 'w';
		if (-board[25] > i + 10)
			ln[27] = 'w';
		l = 53;
		if (off[1] > i || (off[1] < 0 && off[1] + 15 > i)) {
			ln[54] = 'r';
			l = 55;
		}
		if (off[1] > i + 5 || (off[1] < 0 && off[1] + 15 > i + 5)) {
			ln[55] = 'r';
			l = 56;
		}
		if (off[1] > i + 10 || (off[1] < 0 && off[1] + 15 > i + 10)) {
			ln[56] = 'r';
			l = 57;
		}
		ln[l++] = '\n';
		ln[l] = '\0';
		addstr(ln);
	}
	strlcpy(ln, bl, sizeof ln);
	ln[25] = 'B';
	ln[26] = 'A';
	ln[27] = 'R';
	addstr(ln);
	strlcpy(ln, sv, sizeof ln);
	for (i = 4; i > -1; i--) {
		for (j = 1; j < 50; j += 4) {
			k = ((j > 24 ? 53 : 49) - j) / 4;
			wrbsub();
			if (j == 21)
				j += 4;
		}
		if (board[0] > i)
			ln[26] = 'r';
		if (board[0] > i + 5)
			ln[25] = 'r';
		if (board[0] > i + 10)
			ln[27] = 'r';
		l = 53;
		if (off[0] > i || (off[0] < 0 && off[0] + 15 > i)) {
			ln[54] = 'w';
			l = 55;
		}
		if (off[0] > i + 5 || (off[0] < 0 && off[0] + 15 > i + 5)) {
			ln[55] = 'w';
			l = 56;
		}
		if (off[0] > i + 10 || (off[0] < 0 && off[0] + 15 > i + 10)) {
			ln[56] = 'w';
			l = 57;
		}
		ln[l++] = '\n';
		ln[l] = '\0';
		addstr(ln);
	}
	strlcpy(ln, bl, sizeof ln);
	for (j = 1; j < 50; j += 4) {
		k = ((j > 24 ? 53 : 49) - j) / 4;
		ln[j + 1] = k % 10 + '0';
		if (k > 9)
			ln[j] = k / 10 + '0';
		if (j == 21)
			j += 4;
	}
	addstr(ln);
	addstr("|_______________________|___|_______________________|\n");

lastline:
	gwrite();
	move(18, 0);
}

void
wrbsub()
{
	int     m;
	char    d;

	if (board[k] > 0) {
		m = board[k];
		d = 'r';
	} else {
		m = -board[k];
		d = 'w';
	}
	if (m > i)
		ln[j + 1] = d;
	if (m > i + 5)
		ln[j] = d;
	if (m > i + 10)
		ln[j + 2] = d;
}
