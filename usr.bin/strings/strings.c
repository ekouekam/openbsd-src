/*	$OpenBSD: strings.c,v 1.15 2009/10/27 23:59:43 deraadt Exp $	*/
/*	$NetBSD: strings.c,v 1.7 1995/02/15 15:49:19 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1987, 1993
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

#include <sys/types.h>

#include <a.out.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <unistd.h>
#include <err.h>

#define FORMAT_DEC "%07ld "
#define FORMAT_OCT "%07lo "
#define FORMAT_HEX "%07lx "

#define DEF_LEN		4		/* default minimum string length */
#define ISSTR(ch)	(isascii(ch) && (isprint(ch) || ch == '\t'))

typedef struct exec	EXEC;		/* struct exec cast */

static long	foff;			/* offset in the file */
static int	hcnt,			/* head count */
		head_len,		/* length of header */
		read_len;		/* length to read */
static u_char	hbfr[sizeof(EXEC)];	/* buffer for struct exec */

static void usage(void);
int getch(void);

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch, cnt;
	u_char *C;
	EXEC *head;
	int exitcode, minlen, maxlen, bfrlen;
	short asdata, fflg;
	u_char *bfr;
	char *file, *p;
	char *offset_format;

	setlocale(LC_ALL, "");

	/*
	 * for backward compatibility, allow '-' to specify 'a' flag; no
	 * longer documented in the man page or usage string.
	 */
	asdata = exitcode = fflg = 0;
	offset_format = NULL;
	minlen = -1;
	maxlen = -1;
	while ((ch = getopt(argc, argv, "0123456789an:m:oft:-")) != -1)
		switch((char)ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: strings was originally designed to take
			 * a number after a dash.
			 */
			if (minlen == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					minlen = atoi(++p);
				else
					minlen = atoi(argv[optind] + 1);
			}
			break;
		case '-':
		case 'a':
			asdata = 1;
			break;
		case 'f':
			fflg = 1;
			break;
		case 'n':
			minlen = atoi(optarg);
			break;
		case 'm':
			maxlen = atoi(optarg);
			break;
		case 'o':
			offset_format = FORMAT_OCT;
			break;
		case 't':
			switch (*optarg) {
			case 'o':
			        offset_format = FORMAT_OCT;
				break;
			case 'd':
				offset_format = FORMAT_DEC;
				break;
			case 'x':
				offset_format = FORMAT_HEX;
				break;
			default:
				usage();
				/* NOTREACHED */
			}
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (minlen == -1)
		minlen = DEF_LEN;
	else if (minlen < 1)
		errx(1, "length less than 1");
	if (maxlen != -1 && maxlen < minlen)
		errx(1, "max length less than min");
	bfrlen = maxlen == -1 ? minlen : maxlen;
	bfr = malloc(bfrlen + 1);
	if (!bfr)
		err(1, "malloc");
	bfr[bfrlen] = '\0';
	file = "stdin";
	do {
		if (*argv) {
			file = *argv++;
			if (!freopen(file, "r", stdin)) {
				warn("%s", file);
				exitcode = 1;
				goto nextfile;
			}
		}
		foff = 0;
#define DO_EVERYTHING()		{read_len = -1; head_len = 0; goto start;}
		read_len = -1;
		if (asdata)
			DO_EVERYTHING()
		else {
			head = (EXEC *)hbfr;
			if ((head_len =
			    read(fileno(stdin), head, sizeof(EXEC))) == -1)
				DO_EVERYTHING()
			if (head_len == sizeof(EXEC) && !N_BADMAG(*head)) {
				foff = N_TXTOFF(*head);
				if (fseek(stdin, foff, SEEK_SET) == -1)
					DO_EVERYTHING()
				read_len = head->a_text + head->a_data;
				head_len = 0;
			}
			else
				hcnt = 0;
		}
start:
		for (cnt = 0, C = bfr; (ch = getch()) != EOF;) {
			if (ISSTR(ch)) {
				*C++ = ch;
				if (++cnt < minlen)
					continue;
				if (maxlen != -1) {
					while ((ch = getch()) != EOF &&
					       ISSTR(ch) && cnt++ < maxlen)
						*C++ = ch;
					if (ch == EOF ||
					    (ch != 0 && ch != '\n')) {
						/* get all of too big string */
						while ((ch = getch()) != EOF &&
						       ISSTR(ch))
							;
						ungetc(ch, stdin);
						goto out;
					}
					*C = 0;
				}

				if (fflg)
					printf("%s:", file);

				if (offset_format) 
					printf(offset_format, foff - minlen);

				printf("%s", bfr);
				
				if (maxlen == -1)
					while ((ch = getch()) != EOF &&
					       ISSTR(ch))
						putchar((char)ch);
				putchar('\n');
			out:
				;
			}
			cnt = 0;
			C = bfr;
		}
nextfile: ;
	} while (*argv);
	exit(exitcode);
}

/*
 * getch --
 *	get next character from wherever
 */
int
getch(void)
{
	++foff;
	if (head_len) {
		if (hcnt < head_len)
			return((int)hbfr[hcnt++]);
		head_len = 0;
	}
	if (read_len == -1 || read_len-- > 0)
		return(getchar());
	return(EOF);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: strings [-afo] [-m number] [-n number] [-t radix] [file ...]\n");
	exit(1);
}
