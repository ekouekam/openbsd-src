/*	$OpenBSD: mountd.c,v 1.52 2002/07/18 08:46:18 deraadt Exp $	*/
/*	$NetBSD: mountd.c,v 1.31 1996/02/18 11:57:53 fvdl Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Herb Hasler and Rick Macklem at The University of Guelph.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mountd.c  8.15 (Berkeley) 5/1/95";
#else
static char rcsid[] = "$NetBSD: mountd.c,v 1.31 1996/02/18 11:57:53 fvdl Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <netgroup.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

#include <stdarg.h>

/*
 * Structures for keeping the mount list and export list
 */
struct mountlist {
	struct mountlist *ml_next;
	char		ml_host[RPCMNT_NAMELEN+1];
	char		ml_dirp[RPCMNT_PATHLEN+1];
};

struct dirlist {
	struct dirlist	*dp_left;
	struct dirlist	*dp_right;
	int		dp_flag;
	struct hostlist	*dp_hosts;	/* List of hosts this dir exported to */
	char		dp_dirp[1];	/* Actually malloc'd to size of dir */
};
/* dp_flag bits */
#define	DP_DEFSET	0x1
#define DP_HOSTSET	0x2

struct exportlist {
	struct exportlist *ex_next;
	struct dirlist	*ex_dirl;
	struct dirlist	*ex_defdir;
	int		ex_flag;
	fsid_t		ex_fs;
	char		*ex_fsdir;
};
/* ex_flag bits */
#define	EX_LINKED	0x1

struct netmsk {
	in_addr_t	nt_net;
	in_addr_t	nt_mask;
	char		*nt_name;
};

union grouptypes {
	struct hostent *gt_hostent;
	struct netmsk	gt_net;
};

struct grouplist {
	int gr_type;
	union grouptypes gr_ptr;
	struct grouplist *gr_next;
};
/* Group types */
#define	GT_NULL		0x0
#define	GT_HOST		0x1
#define	GT_NET		0x2
#define	GT_IGNORE	0x5

struct hostlist {
	int		 ht_flag;	/* Uses DP_xx bits */
	struct grouplist *ht_grp;
	struct hostlist	 *ht_next;
};

struct fhreturn {
	int	fhr_flag;
	int	fhr_vers;
	nfsfh_t	fhr_fh;
};

/* Global defs */
char	*add_expdir(struct dirlist **, char *, int);
void	add_dlist(struct dirlist **, struct dirlist *, struct grouplist *, int);
void	add_mlist(char *, char *);
int	check_dirpath(char *);
int	check_options(struct dirlist *);
int	chk_host(struct dirlist *, in_addr_t, int *, int *);
void	del_mlist(char *, char *);
struct dirlist *dirp_search(struct dirlist *, char *);
int	do_mount(struct exportlist *, struct grouplist *, int, struct ucred *,
	    char *, int, struct statfs *);
int	do_opt(char **, char **, struct exportlist *, struct grouplist *,
	    int *, int *, struct ucred *);
struct	exportlist *ex_search(fsid_t *);
struct	exportlist *get_exp(void);
void	free_dir(struct dirlist *);
void	free_exp(struct exportlist *);
void	free_grp(struct grouplist *);
void	free_host(struct hostlist *);
void	new_exportlist(int signo);
void	get_exportlist(void);
int	get_host(char *, struct grouplist *, struct grouplist *);
int	get_num(char *);
struct hostlist *get_ht(void);
int	get_line(void);
void	get_mountlist(void);
int	get_net(char *, struct netmsk *, int);
void	getexp_err(struct exportlist *, struct grouplist *);
struct grouplist *get_grp(void);
void	hang_dirp(struct dirlist *, struct grouplist *, struct exportlist *,
	    int);
void	mntsrv(struct svc_req *, SVCXPRT *);
void	nextfield(char **, char **);
void	out_of_mem(void);
void	parsecred(char *, struct ucred *);
int	put_exlist(struct dirlist *, XDR *, struct dirlist *, int *);
int	scan_tree(struct dirlist *, in_addr_t);
void	send_umntall(int signo);
int	umntall_each(caddr_t, struct sockaddr_in *);
int	xdr_dir(XDR *, char *);
int	xdr_explist(XDR *, caddr_t);
int	xdr_fhs(XDR *, caddr_t);
int	xdr_mlist(XDR *, caddr_t);
void	mountd_svc_run(void);

struct exportlist *exphead;
struct mountlist *mlhead;
struct grouplist *grphead;
char exname[MAXPATHLEN];
struct ucred def_anon = {
	1,
	(uid_t) -2,
	(gid_t) -2,
	0,
	{ 0, }
};
int resvport_only = 1;
int opt_flags;
/* Bits for above */
#define	OP_MAPROOT	0x01
#define	OP_MAPALL	0x02
#define	OP_MASK		0x08
#define	OP_NET		0x10
#define	OP_ALLDIRS	0x40

int debug = 0;

volatile sig_atomic_t gothup;
volatile sig_atomic_t gotterm;

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * "-d" to enable debugging
 * and "-n" to allow nonroot mount.
 */
int
main(int argc, char *argv[])
{
	SVCXPRT *udptransp, *tcptransp;
	FILE *pidfile;
	int c;

	while ((c = getopt(argc, argv, "dnr")) != -1)
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			resvport_only = 0;
			break;
		case 'r':
			/* Compatibility */
			break;
		default:
			fprintf(stderr, "Usage: mountd [-dn] [export_file]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;
	grphead = NULL;
	exphead = NULL;
	mlhead = NULL;

	strlcpy(exname, argc == 1? *argv : _PATH_EXPORTS, sizeof(exname));

	openlog("mountd", LOG_PID, LOG_DAEMON);
	if (debug)
		fprintf(stderr, "Getting export list.\n");
	get_exportlist();
	if (debug)
		fprintf(stderr, "Getting mount list.\n");
	get_mountlist();
	if (debug)
		fprintf(stderr, "Here we go.\n");
	if (debug == 0) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	/* Store pid in file unless mountd is already running */
	pidfile = fopen(_PATH_MOUNTDPID, "r");
	if (pidfile != NULL) {
		if (fscanf(pidfile, "%d\n", &c) > 0 && c > 0) {
			if (kill(c, 0) == 0) {
				syslog(LOG_ERR, "Already running (pid %d)", c);
				exit(1);
			}
		}
		pidfile = freopen(_PATH_MOUNTDPID, "w", pidfile);
	} else {
		pidfile = fopen(_PATH_MOUNTDPID, "w");
	}
	fprintf(pidfile, "%ld\n", (long)getpid());
	fclose(pidfile);

	signal(SIGHUP, (void (*)(int)) new_exportlist);
	signal(SIGTERM, (void (*)(int)) send_umntall);
	signal(SIGSYS, SIG_IGN);
	if ((udptransp = svcudp_create(RPC_ANYSOCK)) == NULL ||
	    (tcptransp = svctcp_create(RPC_ANYSOCK, 0, 0)) == NULL) {
		syslog(LOG_ERR, "Can't create socket");
		exit(1);
	}
	pmap_unset(RPCPROG_MNT, RPCMNT_VER1);
	pmap_unset(RPCPROG_MNT, RPCMNT_VER3);
	if (!svc_register(udptransp, RPCPROG_MNT, RPCMNT_VER1, mntsrv, IPPROTO_UDP) ||
	    !svc_register(udptransp, RPCPROG_MNT, RPCMNT_VER3, mntsrv, IPPROTO_UDP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, RPCMNT_VER1, mntsrv, IPPROTO_TCP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, RPCMNT_VER3, mntsrv, IPPROTO_TCP)) {
		syslog(LOG_ERR, "Can't register mount");
		exit(1);
	}
	mountd_svc_run();
	syslog(LOG_ERR, "Mountd died");
	exit(1);
}

void
mountd_svc_run()
{
	fd_set *fds = NULL;
	int fds_size = 0;
	extern fd_set *__svc_fdset;
	extern int __svc_fdsetsize;

	for (;;) {
		if (__svc_fdset) {
			int bytes = howmany(__svc_fdsetsize, NFDBITS) *
			    sizeof(fd_mask);
			if (fds_size != __svc_fdsetsize) {
				if (fds)
					free(fds);
				fds = (fd_set *)malloc(bytes);  /* XXX */
				fds_size = __svc_fdsetsize;
			}
			memcpy(fds, __svc_fdset, bytes);
		} else {
			if (fds)
				free(fds);
			fds = NULL;
		}
		switch (select(svc_maxfd+1, fds, 0, 0, (struct timeval *)0)) {
		case -1:
			if (errno == EINTR)
				break;
			perror("mountd_svc_run: - select failed");
			if (fds)
				free(fds);
			return;
		case 0:
			break;
		default:
			svc_getreqset2(fds, svc_maxfd+1);
			break;
		}
		if (gothup) {
			get_exportlist();
			gothup = 0;
		}
		if (gotterm) {
			(void) clnt_broadcast(RPCPROG_MNT, RPCMNT_VER1,
			    RPCMNT_UMNTALL, xdr_void, (caddr_t)0, xdr_void,
			    (caddr_t)0, umntall_each);
			exit(0);
		}
	}
}

/*
 * The mount rpc service
 */
void
mntsrv(struct svc_req *rqstp, SVCXPRT *transp)
{
	char rpcpath[RPCMNT_PATHLEN+1], dirpath[MAXPATHLEN];
	struct hostent *hp = NULL;
	struct exportlist *ep;
	sigset_t sighup_mask;
	int defset, hostset;
	struct fhreturn fhr;
	struct dirlist *dp;
	struct statfs fsb;
	struct stat stb;
	in_addr_t saddr;
	u_short sport;
	long bad = 0;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	saddr = transp->xp_raddr.sin_addr.s_addr;
	sport = ntohs(transp->xp_raddr.sin_port);
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCMNT_MOUNT:
		if (debug)
			fprintf(stderr, "Got mount request from %s\n",
			    inet_ntoa(transp->xp_raddr.sin_addr));
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "Refused mount RPC from host %s port %d",
			    inet_ntoa(transp->xp_raddr.sin_addr), sport);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			svcerr_decode(transp);
			return;
		}
		if (debug)
			fprintf(stderr, "rpcpath: %s\n", rpcpath);

		/*
		 * Get the real pathname and make sure it is a file or
		 * directory that exists.
		 */
		if (realpath(rpcpath, dirpath) == 0 ||
		    stat(dirpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) && !S_ISREG(stb.st_mode)) ||
		    statfs(dirpath, &fsb) < 0) {
			chdir("/");	/* Just in case realpath doesn't */
			if (debug)
				fprintf(stderr, "stat failed on %s\n", dirpath);
			bad = ENOENT;	/* We will send error reply later */
		}

		/* Check in the exports list */
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		ep = ex_search(&fsb.f_fsid);
		hostset = defset = 0;
		if (ep && (chk_host(ep->ex_defdir, saddr, &defset, &hostset) ||
		    ((dp = dirp_search(ep->ex_dirl, dirpath)) &&
		    chk_host(dp, saddr, &defset, &hostset)) ||
		    (defset && scan_tree(ep->ex_defdir, saddr) == 0 &&
		    scan_tree(ep->ex_dirl, saddr) == 0))) {
			if (bad) {
				if (!svc_sendreply(transp, xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "Can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (hostset & DP_HOSTSET)
				fhr.fhr_flag = hostset;
			else
				fhr.fhr_flag = defset;
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(nfsfh_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				if (errno == ENOSYS) {
					syslog(LOG_ERR,
					    "Kernel does not support NFS exporting, "
					    "mountd aborting..");
					_exit(1);
				}
				bad = errno;
				syslog(LOG_ERR, "Can't get fh for %s", dirpath);
				if (!svc_sendreply(transp, xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "Can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (!svc_sendreply(transp, xdr_fhs, (caddr_t)&fhr))
				syslog(LOG_ERR, "Can't send reply");
			if (hp == NULL)
				hp = gethostbyaddr((caddr_t)&saddr,
				    sizeof(saddr), AF_INET);
			if (hp)
				add_mlist(hp->h_name, dirpath);
			else
				add_mlist(inet_ntoa(transp->xp_raddr.sin_addr),
					dirpath);
			if (debug) {
				fprintf(stderr,
				    "Mount successful for %s by %s.\n",
				    dirpath,
				    inet_ntoa(transp->xp_raddr.sin_addr));
			}
		} else
			bad = EACCES;

		if (bad && !svc_sendreply(transp, xdr_long, (caddr_t)&bad))
			syslog(LOG_ERR, "Can't send reply");
		sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case RPCMNT_DUMP:
		if (!svc_sendreply(transp, xdr_mlist, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case RPCMNT_UMOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, dirpath)) {
			svcerr_decode(transp);
			return;
		}
		if (!svc_sendreply(transp, xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, dirpath);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), dirpath);
		return;
	case RPCMNT_UMNTALL:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, NULL);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), NULL);
		return;
	case RPCMNT_EXPORT:
		if (!svc_sendreply(transp, xdr_explist, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dirpath string
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
int
xdr_fhs(XDR *xdrsp, caddr_t cp)
{
	struct fhreturn *fhrp = (struct fhreturn *)cp;
	long ok = 0, len, auth;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, NFSX_V2FH));
	case 3:
		len = NFSX_V3FH;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, len))
			return (0);
		auth = RPCAUTH_UNIX;
		len = 1;
		if (!xdr_long(xdrsp, &len))
			return (0);
		return (xdr_long(xdrsp, &auth));
	}
	return (0);
}

int
xdr_mlist(XDR *xdrsp, caddr_t cp)
{
	int true = 1, false = 0;
	struct mountlist *mlp;
	char *strp;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &true))
			return (0);
		strp = &mlp->ml_host[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = &mlp->ml_dirp[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
int
xdr_explist(XDR *xdrsp, caddr_t cp)
{
	struct exportlist *ep;
	int false = 0, putdef;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
	ep = exphead;
	while (ep) {
		putdef = 0;
		if (put_exlist(ep->ex_dirl, xdrsp, ep->ex_defdir, &putdef))
			goto errout;
		if (ep->ex_defdir && putdef == 0 && put_exlist(ep->ex_defdir,
		    xdrsp, NULL, &putdef))
			goto errout;
		ep = ep->ex_next;
	}
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
errout:
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to traverse the tree and export the
 * directory paths.
 */
int
put_exlist(struct dirlist *dp, XDR *xdrsp, struct dirlist *adp,
    int *putdefp)
{
	int true = 1, false = 0, gotalldir = 0;
	struct grouplist *grp;
	struct hostlist *hp;
	char *strp;

	if (dp) {
		if (put_exlist(dp->dp_left, xdrsp, adp, putdefp))
			return (1);
		if (!xdr_bool(xdrsp, &true))
			return (1);
		strp = dp->dp_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (1);
		if (adp && !strcmp(dp->dp_dirp, adp->dp_dirp)) {
			gotalldir = 1;
			*putdefp = 1;
		}
		if ((dp->dp_flag & DP_DEFSET) == 0 &&
		    (gotalldir == 0 || (adp->dp_flag & DP_DEFSET) == 0)) {
			hp = dp->dp_hosts;
			while (hp) {
				grp = hp->ht_grp;
				if (grp->gr_type == GT_HOST) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_hostent->h_name;
					if (!xdr_string(xdrsp, &strp,
					    RPCMNT_NAMELEN))
						return (1);
				} else if (grp->gr_type == GT_NET) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_net.nt_name;
					if (!xdr_string(xdrsp, &strp,
					    RPCMNT_NAMELEN))
						return (1);
				}
				hp = hp->ht_next;
				if (gotalldir && hp == NULL) {
					hp = adp->dp_hosts;
					gotalldir = 0;
				}
			}
		}
		if (!xdr_bool(xdrsp, &false))
			return (1);
		if (put_exlist(dp->dp_right, xdrsp, adp, putdefp))
			return (1);
	}
	return (0);
}

#define LINESIZ	10240
char line[LINESIZ];
FILE *exp_file;

void
new_exportlist(int signo)
{
	gothup = 1;

}

/*
 * Get the export list
 */
void
get_exportlist(void)
{
	int len, has_host, exflags, got_nondir, dirplen = 0, num;
	int lookup_failed, num_hosts, i, netgrp;
	char *cp, *endcp, *dirp = NULL, *hst, *usr, *dom, savedc;
	struct exportlist *ep, *ep2;
	struct grouplist *grp, *tgrp;
	struct exportlist **epp;
	struct dirlist *dirhead;
	struct statfs fsb, *fsp;
	struct hostent *hpe;
	struct ucred anon;

	/*
	 * First, get rid of the old list
	 */
	ep = exphead;
	while (ep) {
		ep2 = ep;
		ep = ep->ex_next;
		free_exp(ep2);
	}
	exphead = NULL;

	grp = grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	grphead = NULL;

	/*
	 * And delete exports that are in the kernel for all local
	 * file systems.
	 * XXX: Should know how to handle all local exportable file systems
	 *      instead of just MOUNT_FFS.
	 */
	num = getmntinfo(&fsp, MNT_NOWAIT);
	for (i = 0; i < num; i++) {
		union {
			struct ufs_args ua;
			struct iso_args ia;
			struct mfs_args ma;
			struct msdosfs_args da;
			struct adosfs_args aa;
		} targs;

		if (!strncmp(fsp->f_fstypename, MOUNT_MFS, MFSNAMELEN) ||
		    !strncmp(fsp->f_fstypename, MOUNT_FFS, MFSNAMELEN) ||
		    !strncmp(fsp->f_fstypename, MOUNT_EXT2FS, MFSNAMELEN) ||
		    !strncmp(fsp->f_fstypename, MOUNT_MSDOS, MFSNAMELEN) ||
		    !strncmp(fsp->f_fstypename, MOUNT_ADOSFS, MFSNAMELEN) ||
		    !strncmp(fsp->f_fstypename, MOUNT_CD9660, MFSNAMELEN)) {
			bzero((char *)&targs, sizeof(targs));
			targs.ua.fspec = NULL;
			targs.ua.export_info.ex_flags = MNT_DELEXPORT;
			if (mount(fsp->f_fstypename, fsp->f_mntonname,
			    fsp->f_flags | MNT_UPDATE, &targs) < 0)
				syslog(LOG_ERR, "Can't delete exports for %s: %m",
				    fsp->f_mntonname);
		}
		fsp++;
	}

	/*
	 * Read in the exports file and build the list, calling
	 * mount() as we go along to push the export rules into the kernel.
	 */
	if ((exp_file = fopen(exname, "r")) == NULL) {
		syslog(LOG_ERR, "Can't open %s", exname);
		exit(2);
	}
	dirhead = NULL;
	while (get_line()) {
		if (debug)
			fprintf(stderr, "Got line %s\n",line);
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;

		/*
		 * Set defaults.
		 */
		has_host = FALSE;
		num_hosts = 0;
		lookup_failed = FALSE;
		anon = def_anon;
		exflags = MNT_EXPORTED;
		got_nondir = 0;
		opt_flags = 0;
		ep = NULL;

		/*
		 * Create new exports list entry
		 */
		len = endcp-cp;
		tgrp = grp = get_grp();
		while (len > 0) {
			if (len > RPCMNT_NAMELEN) {
				getexp_err(ep, tgrp);
				goto nextline;
			}
			if (*cp == '-') {
				if (ep == NULL) {
					getexp_err(ep, tgrp);
					goto nextline;
				}
				if (debug)
					fprintf(stderr, "doing opt %s\n", cp);
				got_nondir = 1;
				if (do_opt(&cp, &endcp, ep, grp, &has_host,
				    &exflags, &anon)) {
					getexp_err(ep, tgrp);
					goto nextline;
				}
			} else if (*cp == '/') {
			    savedc = *endcp;
			    *endcp = '\0';
			    if (check_dirpath(cp) &&
				statfs(cp, &fsb) >= 0) {
				if (got_nondir) {
				    syslog(LOG_ERR, "Dirs must be first");
				    getexp_err(ep, tgrp);
				    goto nextline;
				}
				if (ep) {
				    if (ep->ex_fs.val[0] != fsb.f_fsid.val[0] ||
					ep->ex_fs.val[1] != fsb.f_fsid.val[1]) {
					getexp_err(ep, tgrp);
					goto nextline;
				    }
				} else {
				    /*
				     * See if this directory is already
				     * in the list.
				     */
				    ep = ex_search(&fsb.f_fsid);
				    if (ep == NULL) {
					ep = get_exp();
					ep->ex_fs = fsb.f_fsid;
					ep->ex_fsdir = (char *)
					    malloc(strlen(fsb.f_mntonname) + 1);
					if (ep->ex_fsdir)
					    strcpy(ep->ex_fsdir,
						fsb.f_mntonname);
					else
					    out_of_mem();
					if (debug)
					  fprintf(stderr,
					      "Making new ep fs=0x%x,0x%x\n",
					      fsb.f_fsid.val[0],
					      fsb.f_fsid.val[1]);
				    } else if (debug)
					fprintf(stderr,
					    "Found ep fs=0x%x,0x%x\n",
					    fsb.f_fsid.val[0],
					    fsb.f_fsid.val[1]);
				}

				/*
				 * Add dirpath to export mount point.
				 */
				dirp = add_expdir(&dirhead, cp, len);
				dirplen = len;
			    } else {
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			    *endcp = savedc;
			} else {
			    savedc = *endcp;
			    *endcp = '\0';
			    got_nondir = 1;
			    if (ep == NULL) {
				getexp_err(ep, tgrp);
				goto nextline;
			    }

			    /*
			     * Get the host or netgroup.
			     */
			    setnetgrent(cp);
			    netgrp = getnetgrent((const char **)&hst,
				(const char **)&usr, (const char **)&dom);
			    do {
				if (has_host) {
				    grp->gr_next = get_grp();
				    grp = grp->gr_next;
				}
				if (netgrp) {
				    if (hst == NULL) {
					syslog(LOG_ERR,
					    "NULL hostname in netgroup %s, skipping",
					    cp);
					grp->gr_type = GT_IGNORE;
					lookup_failed = TRUE;
					continue;
				    } else if (get_host(hst, grp, tgrp)) {
					syslog(LOG_ERR,
					    "Unknown host (%s) in netgroup %s",
					    hst, cp);
					grp->gr_type = GT_IGNORE;
					lookup_failed = TRUE;
					continue;
				    }
				} else if (get_host(cp, grp, tgrp)) {
				    syslog(LOG_ERR,
					"Unknown host (%s) in line %s",
					cp, line);
				    grp->gr_type = GT_IGNORE;
				    lookup_failed = TRUE;
				    continue;
				}
				has_host = TRUE;
				num_hosts++;
			    } while (netgrp && getnetgrent((const char **)&hst,
				(const char **)&usr, (const char **)&dom));
			    endnetgrent();
			    *endcp = savedc;
			}
			cp = endcp;
			nextfield(&cp, &endcp);
			len = endcp - cp;
		}
		/*
		 * If the exports list is empty due to unresolvable hostnames
		 * we throw away the line.
		 */
		if (lookup_failed == TRUE && num_hosts == 0 &&
		    tgrp->gr_type == GT_IGNORE)  {
			getexp_err(ep, tgrp);
			goto nextline;
		}
		if (check_options(dirhead)) {
			getexp_err(ep, tgrp);
			goto nextline;
		}
		if (!has_host) {
			grp->gr_type = GT_HOST;
			if (debug)
				fprintf(stderr, "Adding a default entry\n");
			/* add a default group and make the grp list NULL */
			hpe = (struct hostent *)malloc(sizeof(struct hostent));
			if (hpe == NULL)
				out_of_mem();
			hpe->h_name = strdup("Default");
			hpe->h_addrtype = AF_INET;
			hpe->h_length = sizeof (u_int32_t);
			hpe->h_addr_list = NULL;
			grp->gr_ptr.gt_hostent = hpe;

		/*
		 * Don't allow a network export coincide with a list of
		 * host(s) on the same line.
		 */
		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			getexp_err(ep, tgrp);
			goto nextline;
		}

		/*
		 * Loop through hosts, pushing the exports into the kernel.
		 * After loop, tgrp points to the start of the list and
		 * grp points to the last entry in the list.
		 */
		grp = tgrp;
		do {
			/*
			 * Non-zero return indicates an error.  Return
			 * val of 1 means line is invalid (not just entry).
			 */
			i = do_mount(ep, grp, exflags, &anon, dirp, dirplen, &fsb);
			if (i == 1) {
				getexp_err(ep, tgrp);
				goto nextline;
			} else if (i == 2) {
				syslog(LOG_ERR,
				    "Bad exports list entry (%s) in line %s",
				    (grp->gr_type == GT_HOST)
				    ? grp->gr_ptr.gt_hostent->h_name
				    : (grp->gr_type == GT_NET)
				    ? grp->gr_ptr.gt_net.nt_name
				    : "Unknown", line);
			}
		} while (grp->gr_next && (grp = grp->gr_next));

		/*
		 * Success. Update the data structures.
		 */
		if (has_host) {
			hang_dirp(dirhead, tgrp, ep, opt_flags);
			grp->gr_next = grphead;
			grphead = tgrp;
		} else {
			hang_dirp(dirhead, NULL, ep,
				opt_flags);
			free_grp(grp);
		}
		dirhead = NULL;
		if ((ep->ex_flag & EX_LINKED) == 0) {
			ep2 = exphead;
			epp = &exphead;

			/*
			 * Insert in the list in alphabetical order.
			 */
			while (ep2 && strcmp(ep2->ex_fsdir, ep->ex_fsdir) < 0) {
				epp = &ep2->ex_next;
				ep2 = ep2->ex_next;
			}
			if (ep2)
				ep->ex_next = ep2;
			*epp = ep;
			ep->ex_flag |= EX_LINKED;
		}
nextline:
		if (dirhead) {
			free_dir(dirhead);
			dirhead = NULL;
		}
	}
	fclose(exp_file);
}

/*
 * Allocate an export list element
 */
struct exportlist *
get_exp(void)
{
	struct exportlist *ep;

	ep = (struct exportlist *)malloc(sizeof (struct exportlist));
	if (ep == NULL)
		out_of_mem();
	memset(ep, 0, sizeof(struct exportlist));
	return (ep);
}

/*
 * Allocate a group list element
 */
struct grouplist *
get_grp()
{
	struct grouplist *gp;

	gp = (struct grouplist *)malloc(sizeof (struct grouplist));
	if (gp == NULL)
		out_of_mem();
	memset(gp, 0, sizeof(struct grouplist));
	return (gp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
void
getexp_err(struct exportlist *ep, struct grouplist *grp)
{
	struct grouplist *tgrp;

	syslog(LOG_ERR, "Bad exports list line %s", line);
	if (ep && (ep->ex_flag & EX_LINKED) == 0)
		free_exp(ep);
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
}

/*
 * Search the export list for a matching fs.
 */
struct exportlist *
ex_search(fsid_t *fsid)
{
	struct exportlist *ep;

	ep = exphead;
	while (ep) {
		if (ep->ex_fs.val[0] == fsid->val[0] &&
		    ep->ex_fs.val[1] == fsid->val[1])
			return (ep);
		ep = ep->ex_next;
	}
	return (ep);
}

/*
 * Add a directory path to the list.
 */
char *
add_expdir(struct dirlist **dpp, char *cp, int len)
{
	struct dirlist *dp;

	dp = (struct dirlist *)malloc(sizeof (struct dirlist) + len);
	if (dp == NULL)
		out_of_mem();
	dp->dp_left = *dpp;
	dp->dp_right = NULL;
	dp->dp_flag = 0;
	dp->dp_hosts = NULL;
	strcpy(dp->dp_dirp, cp);
	*dpp = dp;
	return (dp->dp_dirp);
}

/*
 * Hang the dir list element off the dirpath binary tree as required
 * and update the entry for host.
 */
void
hang_dirp(struct dirlist *dp, struct grouplist *grp, struct exportlist *ep,
    int flags)
{
	struct hostlist *hp;
	struct dirlist *dp2;

	if (flags & OP_ALLDIRS) {
		if (ep->ex_defdir)
			free((caddr_t)dp);
		else
			ep->ex_defdir = dp;
		if (grp == NULL) {
			ep->ex_defdir->dp_flag |= DP_DEFSET;
		} else while (grp) {
			hp = get_ht();
			hp->ht_grp = grp;
			hp->ht_next = ep->ex_defdir->dp_hosts;
			ep->ex_defdir->dp_hosts = hp;
			grp = grp->gr_next;
		}
	} else {

		/*
		 * Loop throught the directories adding them to the tree.
		 */
		while (dp) {
			dp2 = dp->dp_left;
			add_dlist(&ep->ex_dirl, dp, grp, flags);
			dp = dp2;
		}
	}
}

/*
 * Traverse the binary tree either updating a node that is already there
 * for the new directory or adding the new node.
 */
void
add_dlist(struct dirlist **dpp, struct dirlist *newdp, struct grouplist *grp,
    int flags)
{
	struct dirlist *dp;
	struct hostlist *hp;
	int cmp;

	dp = *dpp;
	if (dp) {
		cmp = strcmp(dp->dp_dirp, newdp->dp_dirp);
		if (cmp > 0) {
			add_dlist(&dp->dp_left, newdp, grp, flags);
			return;
		} else if (cmp < 0) {
			add_dlist(&dp->dp_right, newdp, grp, flags);
			return;
		} else
			free((caddr_t)newdp);
	} else {
		dp = newdp;
		dp->dp_left = NULL;
		*dpp = dp;
	}
	if (grp) {

		/*
		 * Hang all of the host(s) off of the directory point.
		 */
		do {
			hp = get_ht();
			hp->ht_grp = grp;
			hp->ht_next = dp->dp_hosts;
			dp->dp_hosts = hp;
			grp = grp->gr_next;
		} while (grp);
	} else {
		dp->dp_flag |= DP_DEFSET;
	}
}

/*
 * Search for a dirpath on the export point.
 */
struct dirlist *
dirp_search(struct dirlist *dp, char *dirpath)
{
	int cmp;

	if (dp) {
		cmp = strcmp(dp->dp_dirp, dirpath);
		if (cmp > 0)
			return (dirp_search(dp->dp_left, dirpath));
		else if (cmp < 0)
			return (dirp_search(dp->dp_right, dirpath));
		else
			return (dp);
	}
	return (dp);
}

/*
 * Scan for a host match in a directory tree.
 */
int
chk_host(struct dirlist *dp, in_addr_t saddr, int *defsetp, int *hostsetp)
{
	struct hostlist *hp;
	struct grouplist *grp;
	u_int32_t **addrp;

	if (dp) {
		if (dp->dp_flag & DP_DEFSET)
			*defsetp = dp->dp_flag;
		hp = dp->dp_hosts;
		while (hp) {
			grp = hp->ht_grp;
			switch (grp->gr_type) {
			case GT_HOST:
			    addrp = (u_int32_t **)
				grp->gr_ptr.gt_hostent->h_addr_list;
			    while (*addrp) {
				if (**addrp == saddr) {
				    *hostsetp = (hp->ht_flag | DP_HOSTSET);
				    return (1);
				}
				addrp++;
			    }
			    break;
			case GT_NET:
			    if ((saddr & grp->gr_ptr.gt_net.nt_mask) ==
				grp->gr_ptr.gt_net.nt_net) {
				*hostsetp = (hp->ht_flag | DP_HOSTSET);
				return (1);
			    }
			    break;
			}
			hp = hp->ht_next;
		}
	}
	return (0);
}

/*
 * Scan tree for a host that matches the address.
 */
int
scan_tree(struct dirlist *dp, in_addr_t saddr)
{
	int defset, hostset;

	if (dp) {
		if (scan_tree(dp->dp_left, saddr))
			return (1);
		if (chk_host(dp, saddr, &defset, &hostset))
			return (1);
		if (scan_tree(dp->dp_right, saddr))
			return (1);
	}
	return (0);
}

/*
 * Traverse the dirlist tree and free it up.
 */
void
free_dir(struct dirlist *dp)
{

	if (dp) {
		free_dir(dp->dp_left);
		free_dir(dp->dp_right);
		free_host(dp->dp_hosts);
		free((caddr_t)dp);
	}
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
int
do_opt(char **cpp, char **endcpp, struct exportlist *ep, struct grouplist *grp,
    int *has_hostp, int *exflagsp, struct ucred *cr)
{
	char *cp, *endcp, *cpopt, savedc, savedc2 = 0;
	char *cpoptarg, *cpoptend;
	int allflag, usedarg;

	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		if ((cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if ((cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
		} else {
			if ((cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
			else {
				*cp = savedc;
				nextfield(&cp, &endcp);
				**endcpp = '\0';
				if (endcp > cp && *cp != '-') {
					cpoptarg = cp;
					savedc2 = *endcp;
					*endcp = '\0';
					usedarg = 0;
				}
			}
		}
		if (!strcmp(cpopt, "ro") || !strcmp(cpopt, "o")) {
			*exflagsp |= MNT_EXRDONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
		    !(allflag = strcmp(cpopt, "mapall")) ||
		    !strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			parsecred(cpoptarg, cr);
			if (allflag == 0) {
				*exflagsp |= MNT_EXPORTANON;
				opt_flags |= OP_MAPALL;
			} else
				opt_flags |= OP_MAPROOT;
		} else
		    if (cpoptarg && (!strcmp(cpopt, "mask") ||
				     !strcmp(cpopt, "m"))) {
			if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 1)) {
				syslog(LOG_ERR, "Bad mask: %s", cpoptarg);
				return (1);
			}
			usedarg++;
			opt_flags |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
		    !strcmp(cpopt, "n"))) {
			if (grp->gr_type != GT_NULL) {
				syslog(LOG_ERR, "Network/host conflict");
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 0)) {
				syslog(LOG_ERR, "Bad net: %s", cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			opt_flags |= OP_ALLDIRS;
		} else {
			syslog(LOG_ERR, "Bad opt %s", cpopt);
			return (1);
		}
		if (usedarg >= 0) {
			*endcp = savedc2;
			**endcpp = savedc;
			if (usedarg > 0) {
				*cpp = cp;
				*endcpp = endcp;
			}
			return (0);
		}
		cpopt = cpoptend;
	}
	**endcpp = savedc;
	return (0);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
int
get_host(char *cp, struct grouplist *grp, struct grouplist *tgrp)
{
	struct hostent *hp, *nhp, t_host;
	struct grouplist *checkgrp;
	char **addrp, **naddrp;
	struct in_addr saddr;
	char *aptr[2];
	int i;

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((hp = gethostbyname(cp)) == NULL) {
		if (isdigit(*cp)) {
			if (inet_aton(cp, &saddr) == 0) {
				syslog(LOG_ERR, "inet_aton failed for %s", cp);
				return (1);
			}
			if ((hp = gethostbyaddr((caddr_t)&saddr.s_addr,
			    sizeof (saddr.s_addr), AF_INET)) == NULL) {
				hp = &t_host;
				hp->h_name = cp;
				hp->h_addrtype = AF_INET;
				hp->h_length = sizeof (u_int32_t);
				hp->h_addr_list = aptr;
				aptr[0] = (char *)&saddr;
				aptr[1] = NULL;
			}
		} else {
			syslog(LOG_ERR, "gethostbyname; failed for %s: %s", cp,
			    hstrerror(h_errno));
			return (1);
		}
	}

	/* only insert each host onto the list once */
	for (checkgrp = tgrp; checkgrp; checkgrp = checkgrp->gr_next) {
		if (checkgrp->gr_type == GT_HOST &&
		    checkgrp->gr_ptr.gt_hostent != NULL &&
		    !strcmp(checkgrp->gr_ptr.gt_hostent->h_name, hp->h_name)) {
			grp->gr_type = GT_IGNORE;
			return (0);
		}
	}

	grp->gr_type = GT_HOST;
	nhp = grp->gr_ptr.gt_hostent = (struct hostent *)
		malloc(sizeof(struct hostent));
	if (nhp == NULL)
		out_of_mem();
	memcpy(nhp, hp, sizeof(struct hostent));
	i = strlen(hp->h_name)+1;
	nhp->h_name = (char *)malloc(i);
	if (nhp->h_name == NULL)
		out_of_mem();
	memcpy(nhp->h_name, hp->h_name, i);
	addrp = hp->h_addr_list;
	i = 1;
	while (*addrp++)
		i++;
	naddrp = nhp->h_addr_list = (char **)
		malloc(i*sizeof(char *));
	if (naddrp == NULL)
		out_of_mem();
	addrp = hp->h_addr_list;
	while (*addrp) {
		*naddrp = (char *)
		    malloc(hp->h_length);
		if (*naddrp == NULL)
		    out_of_mem();
		memcpy(*naddrp, *addrp, hp->h_length);
		addrp++;
		naddrp++;
	}
	*naddrp = NULL;
	if (debug)
		fprintf(stderr, "got host %s\n", hp->h_name);
	return (0);
}

/*
 * Free up an exports list component
 */
void
free_exp(struct exportlist *ep)
{

	if (ep->ex_defdir) {
		free_host(ep->ex_defdir->dp_hosts);
		free((caddr_t)ep->ex_defdir);
	}
	if (ep->ex_fsdir)
		free(ep->ex_fsdir);
	free_dir(ep->ex_dirl);
	free((caddr_t)ep);
}

/*
 * Free hosts.
 */
void
free_host(struct hostlist *hp)
{
	struct hostlist *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free((caddr_t)hp2);
	}
}

struct hostlist *
get_ht(void)
{
	struct hostlist *hp;

	hp = (struct hostlist *)malloc(sizeof (struct hostlist));
	if (hp == NULL)
		out_of_mem();
	hp->ht_next = NULL;
	hp->ht_flag = 0;
	return (hp);
}

/*
 * Out of memory, fatal
 */
void
out_of_mem(void)
{

	syslog(LOG_ERR, "Out of memory");
	exit(2);
}

/*
 * Do the mount syscall with the update flag to push the export info into
 * the kernel.  Returns 0 on success, 1 for fatal error, and 2 for error
 * that only invalidates the specific entry/host.
 */
int
do_mount(struct exportlist *ep, struct grouplist *grp, int exflags,
    struct ucred *anoncrp, char *dirp, int dirplen, struct statfs *fsb)
{
	struct sockaddr_in sin, imask;
	union {
		struct ufs_args ua;
		struct iso_args ia;
		struct mfs_args ma;
		struct msdosfs_args da;
		struct adosfs_args aa;
	} args;
	char savedc = '\0';
	u_int32_t **addrp;
	char *cp = NULL;
	in_addr_t net;
	int done;

	args.ua.fspec = 0;
	args.ua.export_info.ex_flags = exflags;
	args.ua.export_info.ex_anon = *anoncrp;
	memset(&sin, 0, sizeof(sin));
	memset(&imask, 0, sizeof(imask));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	imask.sin_family = AF_INET;
	imask.sin_len = sizeof(sin);
	if (grp->gr_type == GT_HOST)
		addrp = (u_int32_t **)grp->gr_ptr.gt_hostent->h_addr_list;
	else
		addrp = NULL;

	done = FALSE;
	while (!done) {
		switch (grp->gr_type) {
		case GT_HOST:
			args.ua.export_info.ex_addr = (struct sockaddr *)&sin;
			args.ua.export_info.ex_masklen = 0;
			if (!addrp) {
				args.ua.export_info.ex_addrlen = 0;
				break;
			}
			sin.sin_addr.s_addr = **addrp;
			args.ua.export_info.ex_addrlen = sizeof(sin);
			break;
		case GT_NET:
			sin.sin_addr.s_addr = grp->gr_ptr.gt_net.nt_net;
			args.ua.export_info.ex_addr = (struct sockaddr *)&sin;
			args.ua.export_info.ex_addrlen = sizeof (sin);
			args.ua.export_info.ex_mask = (struct sockaddr *)&imask;
			args.ua.export_info.ex_masklen = sizeof (imask);
			if (grp->gr_ptr.gt_net.nt_mask) {
				imask.sin_addr.s_addr = grp->gr_ptr.gt_net.nt_mask;
				break;
			}
			net = ntohl(grp->gr_ptr.gt_net.nt_net);
			if (IN_CLASSA(net))
				imask.sin_addr.s_addr = inet_addr("255.0.0.0");
			else if (IN_CLASSB(net))
				imask.sin_addr.s_addr = inet_addr("255.255.0.0");
			else
				imask.sin_addr.s_addr = inet_addr("255.255.255.0");
			grp->gr_ptr.gt_net.nt_mask = imask.sin_addr.s_addr;
			break;
		case GT_IGNORE:
			return (0);
		default:
			syslog(LOG_ERR, "Bad grouptype");
			if (cp)
				*cp = savedc;
			return (1);
		}

		/*
		 * XXX:
		 * Maybe I should just use the fsb->f_mntonname path instead
		 * of looping back up the dirp to the mount point??
		 * Also, needs to know how to export all types of local
		 * exportable file systems and not just MOUNT_FFS.
		 */
		while (mount(fsb->f_fstypename, dirp,
		    fsb->f_flags | MNT_UPDATE, &args) < 0) {
			if (cp)
				*cp-- = savedc;
			else
				cp = dirp + dirplen - 1;
			if (errno == EPERM) {
				syslog(LOG_ERR,
				    "Can't change attributes for %s (%s).\n",
				    dirp,
				    (grp->gr_type == GT_HOST)
				    ?grp->gr_ptr.gt_hostent->h_name
				    :(grp->gr_type == GT_NET)
				    ?grp->gr_ptr.gt_net.nt_name
				    :"Unknown");
				return (2);
			}
			if (opt_flags & OP_ALLDIRS) {
#if 0
				syslog(LOG_ERR, "Could not remount %s: %m",
					dirp);
				return (2);
#endif
			}
			/* back up over the last component */
			while (*cp == '/' && cp > dirp)
				cp--;
			while (*(cp - 1) != '/' && cp > dirp)
				cp--;
			if (cp == dirp) {
				if (debug)
					fprintf(stderr, "mnt unsucc\n");
				syslog(LOG_ERR, "Can't export %s: %m", dirp);
				return (2);
			}
			savedc = *cp;
			*cp = '\0';
		}
		if (addrp) {
			++addrp;
			if (*addrp == NULL)
				done = TRUE;
		} else
			done = TRUE;
	}
	if (cp)
		*cp = savedc;
	return (0);
}

/*
 * Translate a net address.
 */
int
get_net(char *cp, struct netmsk *net, int maskflg)
{
	struct in_addr inetaddr, inetaddr2;
	in_addr_t netaddr;
	struct netent *np;
	char *name;

	if ((netaddr = inet_network(cp)) != INADDR_NONE) {
		inetaddr = inet_makeaddr(netaddr, 0);
		/*
		 * Due to arbitrary subnet masks, you don't know how many
		 * bits to shift the address to make it into a network,
		 * however you do know how to make a network address into
		 * a host with host == 0 and then compare them.
		 * (What a pest)
		 */
		if (!maskflg) {
			setnetent(0);
			while ((np = getnetent())) {
				inetaddr2 = inet_makeaddr(np->n_net, 0);
				if (inetaddr2.s_addr == inetaddr.s_addr)
					break;
			}
			endnetent();
		}
	} else {
		if ((np = getnetbyname(cp)))
			inetaddr = inet_makeaddr(np->n_net, 0);
		else
			return (1);
	}
	if (maskflg)
		net->nt_mask = inetaddr.s_addr;
	else {
		if (np)
			name = np->n_name;
		else
			name = inet_ntoa(inetaddr);
		net->nt_name = (char *)malloc(strlen(name) + 1);
		if (net->nt_name == NULL)
			out_of_mem();
		strcpy(net->nt_name, name);
		net->nt_net = inetaddr.s_addr;
	}
	return (0);
}

/*
 * Parse out the next white space separated field
 */
void
nextfield(char **cp, char **endcp)
{
	char *p;

	p = *cp;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '\n' || *p == '\0')
		*cp = *endcp = p;
	else {
		*cp = p++;
		while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
			p++;
		*endcp = p;
	}
}

/*
 * Get an exports file line. Skip over blank lines and handle line
 * continuations.
 */
int
get_line(void)
{
	int totlen, cont_line, len;
	char *p, *cp;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	p = line;
	totlen = 0;
	do {
		if (fgets(p, LINESIZ - totlen, exp_file) == NULL)
			return (0);
		len = strlen(p);
		cp = p + len - 1;
		cont_line = 0;
		while (cp >= p && (*cp == ' ' || *cp == '\t' || *cp == '\n' ||
		    *cp == '\\')) {
			if (*cp == '\\')
				cont_line = 1;
			cp--;
			len--;
		}
		*++cp = '\0';
		if (len > 0) {
			totlen += len;
			if (totlen >= LINESIZ) {
				syslog(LOG_ERR, "Exports line too long");
				exit(2);
			}
			p = cp;
		}
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
void
parsecred(char *namelist, struct ucred *cr)
{
	gid_t groups[NGROUPS + 1];
	char *name, *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups, cnt;

	/*
	 * Set up the unprivileged user.
	 */
	cr->cr_ref = 1;
	cr->cr_uid = (uid_t)-2;
	cr->cr_gid = (gid_t)-2;
	cr->cr_ngroups = 0;
	/*
	 * Get the user's password table entry.
	 */
	names = strsep(&namelist, " \t\n");
	name = strsep(&names, ":");
	if (isdigit(*name) || *name == '-')
		pw = getpwuid(atoi(name));
	else
		pw = getpwnam(name);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			syslog(LOG_ERR, "Unknown user: %s", name);
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups))
			syslog(LOG_ERR, "Too many groups");
		/*
		 * compress out duplicate
		 */
		cr->cr_ngroups = ngroups - 1;
		cr->cr_gid = groups[0];
		for (cnt = 1; cnt < ngroups; cnt++)
			cr->cr_groups[cnt - 1] = groups[cnt];
		return;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL)
		cr->cr_uid = pw->pw_uid;
	else if (isdigit(*name) || *name == '-')
		cr->cr_uid = atoi(name);
	else {
		syslog(LOG_ERR, "Unknown user: %s", name);
		return;
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS) {
		name = strsep(&names, ":");
		if (isdigit(*name) || *name == '-') {
			cr->cr_groups[cr->cr_ngroups++] = atoi(name);
		} else {
			if ((gr = getgrnam(name)) == NULL) {
				syslog(LOG_ERR, "Unknown group: %s", name);
				continue;
			}
			cr->cr_groups[cr->cr_ngroups++] = gr->gr_gid;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS)
		syslog(LOG_ERR, "Too many groups");
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
void
get_mountlist(void)
{
	struct mountlist *mlp, **mlpp;
	char *host, *dirp, *cp;
	char str[STRSIZ];
	FILE *mlfile;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		syslog(LOG_ERR, "Can't open %s: %m", _PATH_RMOUNTLIST);
		return;
	}
	mlpp = &mlhead;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dirp = strsep(&cp, " \t\n");
		if (host == NULL || dirp == NULL)
			continue;
		mlp = (struct mountlist *)malloc(sizeof (*mlp));
		if (mlp == NULL)
			out_of_mem();
		strlcpy(mlp->ml_host, host, sizeof(mlp->ml_host));
		strlcpy(mlp->ml_dirp, dirp, sizeof(mlp->ml_dirp));
		mlp->ml_next = NULL;
		*mlpp = mlp;
		mlpp = &mlp->ml_next;
	}
	fclose(mlfile);
}

void
del_mlist(char *hostp, char *dirp)
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	FILE *mlfile;
	int fnd = 0;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) &&
		    (!dirp || !strcmp(mlp->ml_dirp, dirp))) {
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free((caddr_t)mlp2);
		} else {
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			syslog(LOG_ERR, "Can't update %s: %m",
			    _PATH_RMOUNTLIST);
			return;
		}
		mlp = mlhead;
		while (mlp) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
			mlp = mlp->ml_next;
		}
		fclose(mlfile);
	}
}

void
add_mlist(char *hostp, char *dirp)
{
	struct mountlist *mlp, **mlpp;
	FILE *mlfile;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) && !strcmp(mlp->ml_dirp, dirp))
			return;
		mlpp = &mlp->ml_next;
		mlp = mlp->ml_next;
	}
	mlp = (struct mountlist *)malloc(sizeof (*mlp));
	if (mlp == NULL)
		out_of_mem();
	strlcpy(mlp->ml_host, hostp, sizeof(mlp->ml_host));
	strlcpy(mlp->ml_dirp, dirp, sizeof(mlp->ml_dirp));
	mlp->ml_next = NULL;
	*mlpp = mlp;
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		syslog(LOG_ERR, "Can't update %s: %m", _PATH_RMOUNTLIST);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
	fclose(mlfile);
}

/*
 * This function is called via SIGTERM when the system is going down.
 * It sends a broadcast RPCMNT_UMNTALL.
 */
void
send_umntall(int signo)
{
	gotterm = 1;
}

int
umntall_each(caddr_t resultsp, struct sockaddr_in *raddr)
{
	return (1);
}

/*
 * Free up a group list.
 */
void
free_grp(struct grouplist *grp)
{
	char **addrp;

	if (grp->gr_type == GT_HOST) {
		if (grp->gr_ptr.gt_hostent->h_name) {
			addrp = grp->gr_ptr.gt_hostent->h_addr_list;
			while (addrp && *addrp)
				free(*addrp++);
			free((caddr_t)grp->gr_ptr.gt_hostent->h_addr_list);
			free(grp->gr_ptr.gt_hostent->h_name);
		}
		free((caddr_t)grp->gr_ptr.gt_hostent);
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_ptr.gt_net.nt_name)
			free(grp->gr_ptr.gt_net.nt_name);
	}
	free((caddr_t)grp);
}

/*
 * Check options for consistency.
 */
int
check_options(struct dirlist *dp)
{

	if (dp == NULL)
		return (1);
	if ((opt_flags & (OP_MAPROOT | OP_MAPALL)) == (OP_MAPROOT | OP_MAPALL)) {
		syslog(LOG_ERR, "-mapall and -maproot mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		syslog(LOG_ERR, "-mask requires -network");
		return (1);
	}
	if ((opt_flags & OP_ALLDIRS) && dp->dp_left) {
		syslog(LOG_ERR, "-alldirs has multiple directories");
		return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 * if no symbolic links are found.
 */
int
check_dirpath(char *dirp)
{
	struct stat sb;
	int ret = 1;
	char *cp;

	/* Remove trailing '/' */
	cp = dirp + strlen(dirp) - 1;
	while (cp > dirp && *cp == '/')
		*cp-- = '\0';

	cp = dirp + 1;
	while (*cp && ret) {
		if (*cp == '/') {
			*cp = '\0';
			if (lstat(dirp, &sb) < 0 || !S_ISDIR(sb.st_mode))
				ret = 0;
			*cp = '/';
		}
		cp++;
	}
	if (lstat(dirp, &sb) < 0 ||
	    (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode)))
		ret = 0;
	return (ret);
}
