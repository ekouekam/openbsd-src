/*	$OpenBSD: preen.c,v 1.9 2002/03/14 06:51:41 mpech Exp $	*/
/*	$NetBSD: preen.c,v 1.15 1996/09/28 19:21:42 christos Exp $	*/

/*
 * Copyright (c) 1990, 1993
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
static char sccsid[] = "@(#)preen.c	8.3 (Berkeley) 12/6/94";
#else
static char rcsid[] = "$NetBSD: preen.c,v 1.15 1996/09/28 19:21:42 christos Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/queue.h>

#include <fstab.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <err.h>

#include "fsutil.h"

struct partentry {
	TAILQ_ENTRY(partentry)	 p_entries;
	char		  	*p_devname;	/* device name */
	char			*p_mntpt;	/* mount point */
	char		  	*p_type;	/* filesystem type */
	void			*p_auxarg;	/* auxiliary argument */
};

TAILQ_HEAD(part, partentry) badh;

struct diskentry {
	TAILQ_ENTRY(diskentry) 	    d_entries;
	char		       	   *d_name;	/* disk base name */
	TAILQ_HEAD(prt, partentry)  d_part;	/* list of partitions on disk */
	pid_t			    d_pid;	/* 0 or pid of fsck proc */
};

TAILQ_HEAD(disk, diskentry) diskh;

static int nrun = 0, ndisks = 0;

static struct diskentry *finddisk(const char *);
static void addpart(const char *, const char *, const char *, void *);
static int startdisk(struct diskentry *, 
    int (*)(const char *, const char *, const char *, void *, pid_t *));
static void printpart(void);

int
checkfstab(flags, maxrun, docheck, checkit)
	int flags, maxrun;
	void *(*docheck)(struct fstab *);
	int (*checkit)(const char *, const char *, const char *, void *,
	    pid_t *);
{
	struct fstab *fs;
	struct diskentry *d, *nextdisk;
	struct partentry *p;
	int ret, pid, retcode, passno, sumstatus, status;
	void *auxarg;
	char *name;

	TAILQ_INIT(&badh);
	TAILQ_INIT(&diskh);

	sumstatus = 0;

	for (passno = 1; passno <= 2; passno++) {
		if (setfsent() == 0) {
			warnx("Can't open checklist file: %s", _PATH_FSTAB);
			return (8);
		}
		while ((fs = getfsent()) != 0) {
			if ((auxarg = (*docheck)(fs)) == NULL)
				continue;

			name = blockcheck(fs->fs_spec);
			if (flags & CHECK_DEBUG)
				printf("pass %d, name %s\n", passno, name);

			if ((flags & CHECK_PREEN) == 0 ||
			    (passno == 1 && fs->fs_passno == 1)) {
				if (name == NULL) {
					if (flags & CHECK_PREEN)
						return 8;
					else
						continue;
				}
				sumstatus = (*checkit)(fs->fs_vfstype,
				    name, fs->fs_file, auxarg, NULL);

				if (sumstatus)
					return (sumstatus);
			} else if (passno == 2 && fs->fs_passno > 1) {
				if (name == NULL) {
					(void) fprintf(stderr,
					    "BAD DISK NAME %s\n", fs->fs_spec);
					sumstatus |= 8;
					continue;
				}
				addpart(fs->fs_vfstype, name, fs->fs_file,
				    auxarg);
			}
		}
		if ((flags & CHECK_PREEN) == 0)
			return 0;
	}

	if (flags & CHECK_DEBUG)
		printpart();

	if (flags & CHECK_PREEN) {
		if (maxrun == 0)
			maxrun = ndisks;
		if (maxrun > ndisks)
			maxrun = ndisks;
		nextdisk = diskh.tqh_first;
		for (passno = 0; passno < maxrun; ++passno) {
			if ((ret = startdisk(nextdisk, checkit)) != 0)
				return ret;
			nextdisk = nextdisk->d_entries.tqe_next;
		}

		while ((pid = wait(&status)) != -1) {
			for (d = diskh.tqh_first; d; d = d->d_entries.tqe_next)
				if (d->d_pid == pid)
					break;

			if (d == NULL) {
				warnx("Unknown pid %d", pid);
				continue;
			}


			if (WIFEXITED(status))
				retcode = WEXITSTATUS(status);
			else
				retcode = 0;

			p = d->d_part.tqh_first;

			if (flags & (CHECK_DEBUG|CHECK_VERBOSE))
				(void) printf("done %s: %s (%s) = %x\n",
				    p->p_type, p->p_devname, p->p_mntpt,
				    status);

			if (WIFSIGNALED(status)) {
				(void) fprintf(stderr,
				    "%s: %s (%s): EXITED WITH SIGNAL %d\n",
				    p->p_type, p->p_devname, p->p_mntpt,
				    WTERMSIG(status));
				retcode = 8;
			}

			TAILQ_REMOVE(&d->d_part, p, p_entries);

			if (retcode != 0) {
				TAILQ_INSERT_TAIL(&badh, p, p_entries);
				sumstatus |= retcode;
			} else {
				free(p->p_type);
				free(p->p_devname);
				free(p);
			}
			d->d_pid = 0;
			nrun--;

			if (d->d_part.tqh_first == NULL)
				ndisks--;

			if (nextdisk == NULL) {
				if (d->d_part.tqh_first) {
					if ((ret = startdisk(d, checkit)) != 0)
						return ret;
				}
			} else if (nrun < maxrun && nrun < ndisks) {
				for ( ;; ) {
					nextdisk = nextdisk->d_entries.tqe_next;
					if (nextdisk == NULL)
						nextdisk = diskh.tqh_first;
					if (nextdisk->d_part.tqh_first != NULL
					    && nextdisk->d_pid == 0)
						break;
				}
				if ((ret = startdisk(nextdisk, checkit)) != 0)
					return ret;
			}
		}
	}
	if (sumstatus) {
		p = badh.tqh_first;
		if (p == NULL)
			return (sumstatus);

		(void) fprintf(stderr,
			"THE FOLLOWING FILE SYSTEM%s HAD AN %s\n\t",
			p->p_entries.tqe_next ? "S" : "",
			"UNEXPECTED INCONSISTENCY:");

		for (; p; p = p->p_entries.tqe_next)
			(void) fprintf(stderr,
			    "%s: %s (%s)%s", p->p_type, p->p_devname,
			    p->p_mntpt, p->p_entries.tqe_next ? ", " : "\n");

		return sumstatus;
	}
	(void) endfsent();
	return (0);
}


static struct diskentry *
finddisk(name)
	const char *name;
{
	const char *p;
	size_t len = 0;
	struct diskentry *d;

	for (p = name + strlen(name) - 1; p >= name; --p)
		if (isdigit(*p)) {
			len = p - name + 1;
			break;
		}

	if (p < name)
		len = strlen(name);

	for (d = diskh.tqh_first; d != NULL; d = d->d_entries.tqe_next)
		if (strncmp(d->d_name, name, len) == 0 && d->d_name[len] == 0)
			return d;

	d = emalloc(sizeof(*d));
	d->d_name = estrdup(name);
	d->d_name[len] = '\0';
	TAILQ_INIT(&d->d_part);
	d->d_pid = 0;

	TAILQ_INSERT_TAIL(&diskh, d, d_entries);
	ndisks++;

	return d;
}


static void
printpart()
{
	struct diskentry *d;
	struct partentry *p;

	for (d = diskh.tqh_first; d != NULL; d = d->d_entries.tqe_next) {
		(void) printf("disk %s: ", d->d_name);
		for (p = d->d_part.tqh_first; p != NULL;
		    p = p->p_entries.tqe_next)
			(void) printf("%s ", p->p_devname);
		(void) printf("\n");
	}
}


static void
addpart(type, devname, mntpt, auxarg)
	const char *type, *devname, *mntpt;
	void *auxarg;
{
	struct diskentry *d = finddisk(devname);
	struct partentry *p;

	for (p = d->d_part.tqh_first; p != NULL; p = p->p_entries.tqe_next)
		if (strcmp(p->p_devname, devname) == 0) {
			warnx("%s in fstab more than once!", devname);
			return;
		}

	p = emalloc(sizeof(*p));
	p->p_devname = estrdup(devname);
	p->p_mntpt = estrdup(mntpt);
	p->p_type = estrdup(type);
	p->p_auxarg = auxarg;

	TAILQ_INSERT_TAIL(&d->d_part, p, p_entries);
}


static int
startdisk(d, checkit)
	struct diskentry *d;
	int (*checkit)(const char *, const char *, const char *, void *,
	    pid_t *);
{
	struct partentry *p = d->d_part.tqh_first;
	int rv;

	while ((rv = (*checkit)(p->p_type, p->p_devname, p->p_mntpt,
	    p->p_auxarg, &d->d_pid)) != 0 && nrun > 0)
		sleep(10);

	if (rv == 0)
		nrun++;

	return rv;
}
