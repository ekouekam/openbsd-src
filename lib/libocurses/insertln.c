/*
 * Copyright (c) 1981, 1993, 1994
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
static char sccsid[] = "@(#)insertln.c	8.2 (Berkeley) 5/4/94";
#endif	/* not lint */

#include <string.h>

#include "curses.h"

/*
 * winsertln --
 *	Do an insert-line on the window, leaving (cury, curx) unchanged.
 */
int
winsertln(win)
	register WINDOW *win;
{

	register int y, i;
	register __LINE *temp;

#ifdef DEBUG
	__CTRACE("insertln: (%0.2o)\n", win);
#endif
	if (win->orig == NULL)
		temp = win->lines[win->maxy - 1];
	for (y = win->maxy - 1; y > win->cury; --y) {
		win->lines[y]->flags &= ~__ISPASTEOL;
		win->lines[y - 1]->flags &= ~__ISPASTEOL;
		if (win->orig == NULL)
			win->lines[y] = win->lines[y - 1];
		else
			(void)memcpy(win->lines[y]->line, 
			    win->lines[y - 1]->line, 
			    win->maxx * __LDATASIZE);
		__touchline(win, y, 0, win->maxx - 1, 0);
	}
	if (win->orig == NULL)
		win->lines[y] = temp;
	else
		temp = win->lines[y];
	for(i = 0; i < win->maxx; i++) {
		temp->line[i].ch = ' ';
		temp->line[i].attr = 0;
	}
	__touchline(win, y, 0, win->maxx - 1, 0);
	if (win->orig == NULL)
		__id_subwins(win);
	return (OK);
}
