/*	$OpenBSD: send.c,v 1.6 1997/07/14 15:56:25 millert Exp $	*/
/*	$NetBSD: send.c,v 1.6 1996/06/08 19:48:39 christos Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
static char sccsid[] = "@(#)send.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: send.c,v 1.6 1997/07/14 15:56:25 millert Exp $";
#endif
#endif /* not lint */

#include "rcv.h"
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * Mail to others.
 */

/*
 * Send message described by the passed pointer to the
 * passed output buffer.  Return -1 on error.
 * Adjust the status: field if need be.
 * If doign is given, suppress ignored header fields.
 * prefix is a string to prepend to each output line.
 */
int
send(mp, obuf, doign, prefix)
	register struct message *mp;
	FILE *obuf;
	struct ignoretab *doign;
	char *prefix;
{
	long count;
	register FILE *ibuf;
	char line[LINESIZE];
	int ishead, infld, ignoring = 0, dostat, firstline;
	register char *cp, *cp2;
	register int c = 0;
	int length;
	int prefixlen = 0;

	/*
	 * Compute the prefix string, without trailing whitespace
	 */
	if (prefix != NULL) {
		cp2 = 0;
		for (cp = prefix; *cp; cp++)
			if (*cp != ' ' && *cp != '\t')
				cp2 = cp;
		prefixlen = cp2 == 0 ? 0 : cp2 - prefix + 1;
	}
	ibuf = setinput(mp);
	count = mp->m_size;
	ishead = 1;
	dostat = doign == 0 || !isign("status", doign);
	infld = 0;
	firstline = 1;
	/*
	 * Process headers first
	 */
	while (count > 0 && ishead) {
		if (fgets(line, sizeof(line), ibuf) == NULL)
			break;
		count -= length = strlen(line);
		if (firstline) {
			/*
			 * First line is the From line, so no headers
			 * there to worry about
			 */
			firstline = 0;
			ignoring = doign == ignoreall;
		} else if (line[0] == '\n') {
			/*
			 * If line is blank, we've reached end of
			 * headers, so force out status: field
			 * and note that we are no longer in header
			 * fields
			 */
			if (dostat) {
				statusput(mp, obuf, prefix);
				dostat = 0;
			}
			ishead = 0;
			ignoring = doign == ignoreall;
		} else if (infld && (line[0] == ' ' || line[0] == '\t')) {
			/*
			 * If this line is a continuation (via space or tab)
			 * of a previous header field, just echo it
			 * (unless the field should be ignored).
			 * In other words, nothing to do.
			 */
		} else {
			/*
			 * Pick up the header field if we have one.
			 */
			for (cp = line; (c = *cp++) && c != ':' && !isspace(c);)
				;
			cp2 = --cp;
			while (isspace(*cp++))
				;
			if (cp[-1] != ':') {
				/*
				 * Not a header line, force out status:
				 * This happens in uucp style mail where
				 * there are no headers at all.
				 */
				if (dostat) {
					statusput(mp, obuf, prefix);
					dostat = 0;
				}
				if (doign != ignoreall)
					/* add blank line */
					(void)putc('\n', obuf);
				ishead = 0;
				ignoring = 0;
			} else {
				/*
				 * If it is an ignored field and
				 * we care about such things, skip it.
				 */
				*cp2 = 0;	/* temporarily null terminate */
				if (doign && isign(line, doign))
					ignoring = 1;
				else if ((line[0] == 's' || line[0] == 'S') &&
					 strcasecmp(line, "status") == 0) {
					/*
					 * If the field is "status," go compute
					 * and print the real Status: field
					 */
					if (dostat) {
						statusput(mp, obuf, prefix);
						dostat = 0;
					}
					ignoring = 1;
				} else {
					ignoring = 0;
					*cp2 = c;	/* restore */
				}
				infld = 1;
			}
		}
		if (!ignoring) {
			/*
			 * Strip trailing whitespace from prefix
			 * if line is blank.
			 */
			if (prefix != NULL)
				if (length > 1)
					fputs(prefix, obuf);
				else
					(void)fwrite(prefix, sizeof(*prefix),
							prefixlen, obuf);
			(void)fwrite(line, sizeof(*line), length, obuf);
			if (ferror(obuf))
				return(-1);
		}
	}
	/*
	 * Copy out message body
	 */
	if (doign == ignoreall)
		count--;		/* skip final blank line */
	if (prefix != NULL)
		while (count > 0) {
			if (fgets(line, sizeof(line), ibuf) == NULL) {
				c = 0;
				break;
			}
			count -= c = strlen(line);
			/*
			 * Strip trailing whitespace from prefix
			 * if line is blank.
			 */
			if (c > 1)
				fputs(prefix, obuf);
			else
				(void)fwrite(prefix, sizeof(*prefix),
						prefixlen, obuf);
			(void)fwrite(line, sizeof(*line), c, obuf);
			if (ferror(obuf))
				return(-1);
		}
	else
		while (count > 0) {
			c = count < LINESIZE ? count : LINESIZE;
			if ((c = fread(line, sizeof(*line), c, ibuf)) <= 0)
				break;
			count -= c;
			if (fwrite(line, sizeof(*line), c, obuf) != c)
				return(-1);
		}
	if (doign == ignoreall && c > 0 && line[c - 1] != '\n')
		/* no final blank line */
		if ((c = getc(ibuf)) != EOF && putc(c, obuf) == EOF)
			return(-1);
	return(0);
}

/*
 * Output a reasonable looking status field.
 */
void
statusput(mp, obuf, prefix)
	register struct message *mp;
	FILE *obuf;
	char *prefix;
{
	char statout[3];
	register char *cp = statout;

	if (mp->m_flag & MREAD)
		*cp++ = 'R';
	if ((mp->m_flag & MNEW) == 0)
		*cp++ = 'O';
	*cp = 0;
	if (statout[0])
		fprintf(obuf, "%sStatus: %s\n",
			prefix == NULL ? "" : prefix, statout);
}

/*
 * Interface between the argument list and the mail1 routine
 * which does all the dirty work.
 */
int
mail(to, cc, bcc, smopts, subject)
	struct name *to, *cc, *bcc, *smopts;
	char *subject;
{
	struct header head;

	head.h_to = to;
	head.h_subject = subject;
	head.h_cc = cc;
	head.h_bcc = bcc;
	head.h_smopts = smopts;
	mail1(&head, 0);
	return(0);
}


/*
 * Send mail to a bunch of user names.  The interface is through
 * the mail routine below.
 */
int
sendmail(v)
	void *v;
{
	char *str = v;
	struct header head;

	head.h_to = extract(str, GTO);
	head.h_subject = NULL;
	head.h_cc = NIL;
	head.h_bcc = NIL;
	head.h_smopts = NIL;
	mail1(&head, 0);
	return(0);
}

/*
 * Mail a message on standard input to the people indicated
 * in the passed header.  (Internal interface).
 */
void
mail1(hp, printheaders)
	struct header *hp;
	int printheaders;
{
	char *cp;
	int pid;
	char **namelist;
	struct name *to;
	FILE *mtf;

	/*
	 * Collect user's mail from standard input.
	 * Get the result as mtf.
	 */
	if ((mtf = collect(hp, printheaders)) == NULL)
		return;
	if (value("interactive") != NULL)
		if (value("askcc") != NULL || value("askbcc") != NULL) {
			if (value("askcc") != NULL)
				grabh(hp, GCC);
			if (value("askbcc") != NULL)
				grabh(hp, GBCC);
		} else {
			puts("EOT");
			(void)fflush(stdout);
		}
	if (fsize(mtf) == 0)
		if (hp->h_subject == NULL)
			puts("No message, no subject; hope that's ok");
		else
			puts("Null message body; hope that's ok");
	/*
	 * Now, take the user names from the combined
	 * to and cc lists and do all the alias
	 * processing.
	 */
	senderr = 0;
	to = usermap(cat(hp->h_bcc, cat(hp->h_to, hp->h_cc)));
	if (to == NIL) {
		puts("No recipients specified");
		senderr++;
	}
	/*
	 * Look through the recipient list for names with /'s
	 * in them which we write to as files directly.
	 */
	to = outof(to, mtf, hp);
	if (senderr)
		savedeadletter(mtf);
	to = elide(to);
	if (count(to) == 0)
		goto out;
	fixhead(hp, to);
	if ((mtf = infix(hp, mtf)) == NULL) {
		fputs(". . . message lost, sorry.\n", stderr);
		return;
	}
	namelist = unpack(cat(hp->h_smopts, to));
	if (debug) {
		char **t;

		fputs("Sendmail arguments:", stdout);
		for (t = namelist; *t != NULL; t++)
			printf(" \"%s\"", *t);
		putchar('\n');
		goto out;
	}
	if ((cp = value("record")) != NULL)
		(void)savemail(expand(cp), mtf);
	/*
	 * Fork, set up the temporary mail file as standard
	 * input for "mail", and exec with the user list we generated
	 * far above.
	 */
	pid = fork();
	if (pid == -1) {
		warn("fork");
		savedeadletter(mtf);
		goto out;
	}
	if (pid == 0) {
		sigset_t nset;

		sigemptyset(&nset);
		sigaddset(&nset, SIGHUP);
		sigaddset(&nset, SIGINT);
		sigaddset(&nset, SIGQUIT);
		sigaddset(&nset, SIGTSTP);
		sigaddset(&nset, SIGTTIN);
		sigaddset(&nset, SIGTTOU);
		prepare_child(&nset, fileno(mtf), -1);
		if ((cp = value("sendmail")) != NULL)
			cp = expand(cp);
		else
			cp = _PATH_SENDMAIL;
		execv(cp, namelist);
		warn(cp);
		_exit(1);
	}
	if (value("verbose") != NULL)
		(void)wait_child(pid);
	else
		free_child(pid);
out:
	(void)Fclose(mtf);
}

/*
 * Fix the header by glopping all of the expanded names from
 * the distribution list into the appropriate fields.
 */
void
fixhead(hp, tolist)
	struct header *hp;
	struct name *tolist;
{
	register struct name *np;

	hp->h_to = NIL;
	hp->h_cc = NIL;
	hp->h_bcc = NIL;
	for (np = tolist; np != NIL; np = np->n_flink)
		if ((np->n_type & GMASK) == GTO)
			hp->h_to =
				cat(hp->h_to, nalloc(np->n_name, np->n_type));
		else if ((np->n_type & GMASK) == GCC)
			hp->h_cc =
				cat(hp->h_cc, nalloc(np->n_name, np->n_type));
		else if ((np->n_type & GMASK) == GBCC)
			hp->h_bcc =
				cat(hp->h_bcc, nalloc(np->n_name, np->n_type));
}

/*
 * Prepend a header in front of the collected stuff
 * and return the new file.
 */
FILE *
infix(hp, fi)
	struct header *hp;
	FILE *fi;
{
	extern char *tempMail;
	register FILE *nfo, *nfi;
	register int c;

	if ((nfo = Fopen(tempMail, "w")) == NULL) {
		warn(tempMail);
		return(fi);
	}
	if ((nfi = Fopen(tempMail, "r")) == NULL) {
		warn(tempMail);
		(void)Fclose(nfo);
		return(fi);
	}
	(void)rm(tempMail);
	(void)puthead(hp, nfo, GTO|GSUBJECT|GCC|GBCC|GNL|GCOMMA);
	c = getc(fi);
	while (c != EOF) {
		(void)putc(c, nfo);
		c = getc(fi);
	}
	if (ferror(fi)) {
		warn("read");
		rewind(fi);
		return(fi);
	}
	(void)fflush(nfo);
	if (ferror(nfo)) {
		warn(tempMail);
		(void)Fclose(nfo);
		(void)Fclose(nfi);
		rewind(fi);
		return(fi);
	}
	(void)Fclose(nfo);
	(void)Fclose(fi);
	rewind(nfi);
	return(nfi);
}

/*
 * Dump the to, subject, cc header on the
 * passed file buffer.
 */
int
puthead(hp, fo, w)
	struct header *hp;
	FILE *fo;
	int w;
{
	register int gotcha;

	gotcha = 0;
	if (hp->h_to != NIL && w & GTO)
		fmt("To:", hp->h_to, fo, w&GCOMMA), gotcha++;
	if (hp->h_subject != NULL && w & GSUBJECT)
		fprintf(fo, "Subject: %s\n", hp->h_subject), gotcha++;
	if (hp->h_cc != NIL && w & GCC)
		fmt("Cc:", hp->h_cc, fo, w&GCOMMA), gotcha++;
	if (hp->h_bcc != NIL && w & GBCC)
		fmt("Bcc:", hp->h_bcc, fo, w&GCOMMA), gotcha++;
	if (gotcha && w & GNL)
		(void)putc('\n', fo);
	return(0);
}

/*
 * Format the given header line to not exceed 72 characters.
 */
void
fmt(str, np, fo, comma)
	char *str;
	register struct name *np;
	FILE *fo;
	int comma;
{
	register col, len;

	comma = comma ? 1 : 0;
	col = strlen(str);
	if (col)
		fputs(str, fo);
	for (; np != NIL; np = np->n_flink) {
		if (np->n_flink == NIL)
			comma = 0;
		len = strlen(np->n_name);
		col++;		/* for the space */
		if (col + len + comma > 72 && col > 4) {
			fputs("\n    ", fo);
			col = 4;
		} else
			putc(' ', fo);
		fputs(np->n_name, fo);
		if (comma)
			putc(',', fo);
		col += len + comma;
	}
	putc('\n', fo);
}

/*
 * Save the outgoing mail on the passed file.
 */

/*ARGSUSED*/
int
savemail(name, fi)
	char name[];
	register FILE *fi;
{
	register FILE *fo;
	char buf[BUFSIZ];
	register i;
	time_t now;

	if ((fo = Fopen(name, "a")) == NULL) {
		warn(name);
		return(-1);
	}
	(void)time(&now);
	fprintf(fo, "From %s %s", myname, ctime(&now));
	while ((i = fread(buf, 1, sizeof(buf), fi)) > 0)
		(void)fwrite(buf, 1, i, fo);
	(void)putc('\n', fo);
	(void)fflush(fo);
	if (ferror(fo))
		warn(name);
	(void)Fclose(fo);
	rewind(fi);
	return(0);
}
