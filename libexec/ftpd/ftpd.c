/*	$OpenBSD: ftpd.c,v 1.79 2000/09/15 07:13:45 deraadt Exp $	*/
/*	$NetBSD: ftpd.c,v 1.15 1995/06/03 22:46:47 mycroft Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1985, 1988, 1990, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)ftpd.c	8.4 (Berkeley) 4/16/94";
#else
static char rcsid[] = "$NetBSD: ftpd.c,v 1.15 1995/06/03 22:46:47 mycroft Exp $";
#endif
#endif /* not lint */

/*
 * FTP server.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#define	FTP_NAMES
#include <arpa/ftp.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <limits.h>
#include <login_cap.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <vis.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>

#if defined(TCPWRAPPERS)
#include <tcpd.h>
#endif	/* TCPWRAPPERS */

#if defined(SKEY)
#include <skey.h>
#endif

#include "pathnames.h"
#include "extern.h"

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static char version[] = "Version 6.5/OpenBSD";

extern	off_t restart_point;
extern	char cbuf[];

union sockunion server_addr;
union sockunion ctrl_addr;
union sockunion data_source;
union sockunion data_dest;
union sockunion his_addr;
union sockunion pasv_addr;

int	daemon_mode = 0;
int	data;
jmp_buf	errcatch, urgcatch;
int	logged_in;
struct	passwd *pw;
int	debug = 0;
int	timeout = 900;    /* timeout after 15 minutes of inactivity */
int	maxtimeout = 7200;/* don't allow idle time to be set beyond 2 hours */
int	logging;
int	high_data_ports = 0;
int	anon_only = 0;
int	multihome = 0;
int	guest;
int	stats;
int	statfd = -1;
int	portcheck = 1;
int	dochroot;
int	type;
int	form;
int	stru;			/* avoid C keyword */
int	mode;
int	doutmp = 0;		/* update utmp file */
int	usedefault = 1;		/* for data transfers */
int	pdata = -1;		/* for passive mode */
int	family = AF_INET;
sig_atomic_t transflag;
off_t	file_size;
off_t	byte_count;
#if !defined(CMASK) || CMASK == 0
#undef CMASK
#define CMASK 027
#endif
int	defumask = CMASK;		/* default umask value */
int	umaskchange = 1;		/* allow user to change umask value. */
char	tmpline[7];
char	hostname[MAXHOSTNAMELEN];
char	remotehost[MAXHOSTNAMELEN];
char	dhostname[MAXHOSTNAMELEN];
char	*guestpw;
static char ttyline[20];
char	*tty = ttyline;		/* for klogin */
static struct utmp utmp;	/* for utmp */
static  login_cap_t *lc;

#if defined(TCPWRAPPERS)
int	allow_severity = LOG_INFO;
int	deny_severity = LOG_NOTICE;
#endif	/* TCPWRAPPERS */

#if defined(KERBEROS)
int	notickets = 1;
char	*krbtkfile_env = NULL;
#endif

char	*ident = NULL;


int epsvall = 0;

/*
 * Timeout intervals for retrying connections
 * to hosts that don't accept PORT cmds.  This
 * is a kludge, but given the problems with TCP...
 */
#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

#ifdef HASSETPROCTITLE
char	proctitle[BUFSIZ];	/* initial part of title */
#endif /* HASSETPROCTITLE */

#define LOGCMD(cmd, file) \
	if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s", cmd, \
		*(file) == '/' ? "" : curdir(), file);
#define LOGCMD2(cmd, file1, file2) \
	 if (logging > 1) \
	    syslog(LOG_INFO,"%s %s%s %s%s", cmd, \
		*(file1) == '/' ? "" : curdir(), file1, \
		*(file2) == '/' ? "" : curdir(), file2);
#define LOGBYTES(cmd, file, cnt) \
	if (logging > 1) { \
		if (cnt == (off_t)-1) \
		    syslog(LOG_INFO,"%s %s%s", cmd, \
			*(file) == '/' ? "" : curdir(), file); \
		else \
		    syslog(LOG_INFO, "%s %s%s = %qd bytes", \
			cmd, (*(file) == '/') ? "" : curdir(), file, cnt); \
	}

static void	 ack __P((char *));
static void	 myoob __P((int));
static int	 checkuser __P((char *, char *));
static FILE	*dataconn __P((char *, off_t, char *));
static void	 dolog __P((struct sockaddr *));
static char	*curdir __P((void));
static void	 end_login __P((void));
static FILE	*getdatasock __P((char *));
static int	guniquefd __P((char *, char **));
static void	 lostconn __P((int));
static void	 sigquit __P((int));
static int	 receive_data __P((FILE *, FILE *));
static void	 replydirname __P((const char *, const char *));
static void	 send_data __P((FILE *, FILE *, off_t, off_t, int));
static struct passwd *
		 sgetpwnam __P((char *));
static char	*sgetsave __P((char *));
static void	 reapchild __P((int));
static int	 check_host __P((struct sockaddr *));
static void	 usage __P((void));

void	 logxfer __P((char *, off_t, time_t));

static char *
curdir()
{
	static char path[MAXPATHLEN+1];	/* path + '/' */

	if (getcwd(path, sizeof(path)-1) == NULL)
		return ("");
	if (path[1] != '\0')		/* special case for root dir. */
		strcat(path, "/");
	/* For guest account, skip / since it's chrooted */
	return (guest ? path+1 : path);
}

char *argstr = "AdDhlMSt:T:u:UvP46";

static void
usage()
{
	syslog(LOG_ERR,
	    "usage: ftpd [-AdDhlMSUv] [-t timeout] [-T maxtimeout] [-u mask]");
	exit(2);
}

int
main(argc, argv, envp)
	int argc;
	char *argv[];
	char **envp;
{
	int addrlen, ch, on = 1, tos;
	char *cp, line[LINE_MAX];
	FILE *fp;
	struct hostent *hp;

	tzset();	/* in case no timezone database in ~ftp */

	while ((ch = getopt(argc, argv, argstr)) != -1) {
		switch (ch) {
		case 'A':
			anon_only = 1;
			break;

		case 'd':
			debug = 1;
			break;

		case 'D':
			daemon_mode = 1;
			break;

		case 'P':
			portcheck = 0;
			break;

		case 'h':
			high_data_ports = 1;
			break;

		case 'l':
			logging++;	/* > 1 == extra logging */
			break;

		case 'M':
			multihome = 1;
			break;

		case 'S':
			stats = 1;
			break;

		case 't':
			timeout = atoi(optarg);
			if (maxtimeout < timeout)
				maxtimeout = timeout;
			break;

		case 'T':
			maxtimeout = atoi(optarg);
			if (timeout > maxtimeout)
				timeout = maxtimeout;
			break;

		case 'u':
		    {
			long val = 0;
			char *p;
			umaskchange = 0;

			val = strtol(optarg, &p, 8);
			if (*p != '\0' || val < 0 || (val & ~ACCESSPERMS)) {
				syslog(LOG_ERR,
				    "ftpd: %s is a bad value for -u, aborting..",
				    optarg);
				exit(2);
			} else
				defumask = val;
			break;
		    }

		case 'U':
			doutmp = 1;
			break;

		case 'v':
			debug = 1;
			break;

		case '4':
			family = AF_INET;
			break;

		case '6':
			family = AF_INET6;
			break;

		default:
			usage();
			break;
		}
	}

	(void) freopen(_PATH_DEVNULL, "w", stderr);

	/*
	 * LOG_NDELAY sets up the logging connection immediately,
	 * necessary for anonymous ftp's that chroot and can't do it later.
	 */
	openlog("ftpd", LOG_PID | LOG_NDELAY, LOG_FTP);

	if (daemon_mode) {
		int ctl_sock, fd;
		struct servent *sv;

		/*
		 * Detach from parent.
		 */
		if (daemon(1, 1) < 0) {
			syslog(LOG_ERR, "failed to become a daemon");
			exit(1);
		}
		(void) signal(SIGCHLD, reapchild);
		/*
		 * Get port number for ftp/tcp.
		 */
		sv = getservbyname("ftp", "tcp");
		if (sv == NULL) {
			syslog(LOG_ERR, "getservbyname for ftp failed");
			exit(1);
		}
		/*
		 * Open a socket, bind it to the FTP port, and start
		 * listening.
		 */
		ctl_sock = socket(family, SOCK_STREAM, 0);
		if (ctl_sock < 0) {
			syslog(LOG_ERR, "control socket: %m");
			exit(1);
		}
		if (setsockopt(ctl_sock, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "control setsockopt: %m");
		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.su_sin.sin_family = family;
		switch (family) {
		case AF_INET:
			server_addr.su_len = sizeof(struct sockaddr_in);
			server_addr.su_sin.sin_port = sv->s_port;
			break;
		case AF_INET6:
			server_addr.su_len = sizeof(struct sockaddr_in6);
			server_addr.su_sin6.sin6_port = sv->s_port;
			break;
		}
		if (bind(ctl_sock, (struct sockaddr *)&server_addr,
			 server_addr.su_len)) {
			syslog(LOG_ERR, "control bind: %m");
			exit(1);
		}
		if (listen(ctl_sock, 32) < 0) {
			syslog(LOG_ERR, "control listen: %m");
			exit(1);
		}
		/* Stash pid in pidfile */
		if ((fp = fopen(_PATH_FTPDPID, "w")) == NULL)
			syslog(LOG_ERR, "can't open %s: %m", _PATH_FTPDPID);
		else {
			fprintf(fp, "%d\n", getpid());
			fchmod(fileno(fp), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			fclose(fp);
		}
		/*
		 * Loop forever accepting connection requests and forking off
		 * children to handle them.
		 */
		while (1) {
			addrlen = sizeof(his_addr);
			fd = accept(ctl_sock, (struct sockaddr *)&his_addr,
				    &addrlen);
			if (fork() == 0) {
				/* child */
				(void) dup2(fd, 0);
				(void) dup2(fd, 1);
				close(ctl_sock);
				break;
			}
			close(fd);
		}

#if defined(TCPWRAPPERS)
		/* ..in the child. */
		if (!check_host((struct sockaddr *)&his_addr))
			exit(1);
#endif	/* TCPWRAPPERS */
	} else {
		addrlen = sizeof(his_addr);
		if (getpeername(0, (struct sockaddr *)&his_addr,
				&addrlen) < 0) {
			/* syslog(LOG_ERR, "getpeername (%s): %m", argv[0]); */
			exit(1);
		}
	}

	/* set this here so klogin can use it... */
	(void)snprintf(ttyline, sizeof(ttyline), "ftp%d", getpid());

	(void) signal(SIGHUP, sigquit);
	(void) signal(SIGINT, sigquit);
	(void) signal(SIGQUIT, sigquit);
	(void) signal(SIGTERM, sigquit);
	(void) signal(SIGPIPE, lostconn);
	(void) signal(SIGCHLD, SIG_IGN);
	if (signal(SIGURG, myoob) == SIG_ERR)
		syslog(LOG_ERR, "signal: %m");

	addrlen = sizeof(ctrl_addr);
	if (getsockname(0, (struct sockaddr *)&ctrl_addr, &addrlen) < 0) {
		syslog(LOG_ERR, "getsockname (%s): %m", argv[0]);
		exit(1);
	}
	if (his_addr.su_family == AF_INET6
	 && IN6_IS_ADDR_V4MAPPED(&his_addr.su_sin6.sin6_addr)) {
#if 1
		/*
		 * IPv4 control connection arrived to AF_INET6 socket.
		 * I hate to do this, but this is the easiest solution.
		 */
		union sockunion tmp_addr;
		const int off = sizeof(struct in6_addr) - sizeof(struct in_addr);

		tmp_addr = his_addr;
		memset(&his_addr, 0, sizeof(his_addr));
		his_addr.su_sin.sin_family = AF_INET;
		his_addr.su_sin.sin_len = sizeof(his_addr.su_sin);
		memcpy(&his_addr.su_sin.sin_addr,
		    &tmp_addr.su_sin6.sin6_addr.s6_addr[off],
		    sizeof(his_addr.su_sin.sin_addr));
		his_addr.su_sin.sin_port = tmp_addr.su_sin6.sin6_port;

		tmp_addr = ctrl_addr;
		memset(&ctrl_addr, 0, sizeof(ctrl_addr));
		ctrl_addr.su_sin.sin_family = AF_INET;
		ctrl_addr.su_sin.sin_len = sizeof(ctrl_addr.su_sin);
		memcpy(&ctrl_addr.su_sin.sin_addr,
		    &tmp_addr.su_sin6.sin6_addr.s6_addr[off],
		    sizeof(ctrl_addr.su_sin.sin_addr));
		ctrl_addr.su_sin.sin_port = tmp_addr.su_sin6.sin6_port;
#else
		while (fgets(line, sizeof(line), fd) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(530, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fd);
		reply(530,
			"Connection from IPv4 mapped address is not supported.");
		exit(0);
#endif
	}
#ifdef IP_TOS
	if (his_addr.su_family == AF_INET) {
		tos = IPTOS_LOWDELAY;
		if (setsockopt(0, IPPROTO_IP, IP_TOS, (char *)&tos,
		    sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
	data_source.su_port = htons(ntohs(ctrl_addr.su_port) - 1);

	/* Try to handle urgent data inline */
#ifdef SO_OOBINLINE
	if (setsockopt(0, SOL_SOCKET, SO_OOBINLINE, (char *)&on, sizeof(on)) < 0)
		syslog(LOG_ERR, "setsockopt: %m");
#endif

#ifdef	F_SETOWN
	if (fcntl(fileno(stdin), F_SETOWN, getpid()) == -1)
		syslog(LOG_ERR, "fcntl F_SETOWN: %m");
#endif
	dolog((struct sockaddr *)&his_addr);
	/*
	 * Set up default state
	 */
	data = -1;
	type = TYPE_A;
	form = FORM_N;
	stru = STRU_F;
	mode = MODE_S;
	tmpline[0] = '\0';

	/* If logins are disabled, print out the message. */
	if ((fp = fopen(_PATH_NOLOGIN, "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(530, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fp);
		reply(530, "System not available.");
		exit(0);
	}
	if ((fp = fopen(_PATH_FTPWELCOME, "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(220, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fp);
		/* reply(220,) must follow */
	}
	(void) gethostname(hostname, sizeof(hostname));

	/* Make sure hostname is fully qualified. */
	hp = gethostbyname(hostname);
	if (hp != NULL)
		strcpy(hostname, hp->h_name);

	if (multihome) {
		getnameinfo((struct sockaddr *)&ctrl_addr, ctrl_addr.su_len,
		    dhostname, sizeof(dhostname), NULL, 0, 0);
	}

	reply(220, "%s FTP server (%s) ready.",
	    (multihome ? dhostname : hostname), version);
	(void) setjmp(errcatch);
	for (;;)
		(void) yyparse();
	/* NOTREACHED */
}

/*
 * Signal handlers.
 */

static void
lostconn(signo)
	int signo;
{

	if (debug)
		syslog(LOG_DEBUG, "lost connection");
	dologout(1);
}

static void
sigquit(signo)
	int signo;
{
	syslog(LOG_ERR, "got signal %s", strerror(signo));

	dologout(1);
}

/*
 * Helper function for sgetpwnam().
 */
static char *
sgetsave(s)
	char *s;
{
	char *new = malloc((unsigned) strlen(s) + 1);

	if (new == NULL) {
		perror_reply(421, "Local resource failure: malloc");
		dologout(1);
		/* NOTREACHED */
	}
	(void) strcpy(new, s);
	return (new);
}

/*
 * Save the result of a getpwnam.  Used for USER command, since
 * the data returned must not be clobbered by any other command
 * (e.g., globbing).
 */
static struct passwd *
sgetpwnam(name)
	char *name;
{
	static struct passwd save;
	struct passwd *p;

	if ((p = getpwnam(name)) == NULL)
		return (p);
	if (save.pw_name) {
		free(save.pw_name);
		memset(save.pw_passwd, 0, strlen(save.pw_passwd));
		free(save.pw_passwd);
		free(save.pw_class);
		free(save.pw_gecos);
		free(save.pw_dir);
		free(save.pw_shell);
	}
	save = *p;
	save.pw_name = sgetsave(p->pw_name);
	save.pw_passwd = sgetsave(p->pw_passwd);
	save.pw_class = sgetsave(p->pw_class);
	save.pw_gecos = sgetsave(p->pw_gecos);
	save.pw_dir = sgetsave(p->pw_dir);
	save.pw_shell = sgetsave(p->pw_shell);
	return (&save);
}

static int login_attempts;	/* number of failed login attempts */
static int askpasswd;		/* had user command, ask for passwd */
static char curname[16];	/* current USER name */

/*
 * USER command.
 * Sets global passwd pointer pw if named account exists and is acceptable;
 * sets askpasswd if a PASS command is expected.  If logged in previously,
 * need to reset state.  If name is "ftp" or "anonymous", the name is not in
 * _PATH_FTPUSERS, and ftp account exists, set guest and pw, then just return.
 * If account doesn't exist, ask for passwd anyway.  Otherwise, check user
 * requesting login privileges.  Disallow anyone who does not have a standard
 * shell as returned by getusershell().  Disallow anyone mentioned in the file
 * _PATH_FTPUSERS to allow people such as root and uucp to be avoided.
 */
void
user(name)
	char *name;
{
	char *cp, *shell;

	if (lc) {
		login_close(lc);
		lc = NULL;
	}

	if (logged_in) {
		if (guest) {
			reply(530, "Can't change user from guest login.");
			return;
		} else if (dochroot) {
			reply(530, "Can't change user from chroot user.");
			return;
		}
		end_login();
	}

	guest = 0;
	if (strcmp(name, "ftp") == 0 || strcmp(name, "anonymous") == 0) {
		if (checkuser(_PATH_FTPUSERS, "ftp") ||
		    checkuser(_PATH_FTPUSERS, "anonymous"))
			reply(530, "User %s access denied.", name);
		else if ((pw = sgetpwnam("ftp")) != NULL) {
			guest = 1;
			askpasswd = 1;
			lc = login_getclass(pw->pw_class);
			reply(331,
			    "Guest login ok, type your name as password.");
		} else
			reply(530, "User %s unknown.", name);
		if (!askpasswd && logging)
			syslog(LOG_NOTICE,
			    "ANONYMOUS FTP LOGIN REFUSED FROM %s", remotehost);
		return;
	}
	if (anon_only && !checkuser(_PATH_FTPCHROOT, name)) {
		reply(530, "Sorry, only anonymous ftp allowed.");
		return;
	}

	if ((pw = sgetpwnam(name))) {
		if ((shell = pw->pw_shell) == NULL || *shell == 0)
			shell = _PATH_BSHELL;
		while ((cp = getusershell()) != NULL)
			if (strcmp(cp, shell) == 0)
				break;
		endusershell();

		if (cp == NULL || checkuser(_PATH_FTPUSERS, name)) {
			reply(530, "User %s access denied.", name);
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN REFUSED FROM %s, %s",
				    remotehost, name);
			pw = (struct passwd *) NULL;
			return;
		}
		lc = login_getclass(pw->pw_class);
	}
	if (logging) {
		strncpy(curname, name, sizeof(curname)-1);
		curname[sizeof(curname)-1] = '\0';
	}
#ifdef SKEY
	if (!skey_haskey(name)) {
		char *myskey, *skey_keyinfo __P((char *name));

		myskey = skey_keyinfo(name);
		reply(331, "Password [ %s ] for %s required.",
		    myskey ? myskey : "error getting challenge", name);
	} else
#endif
		reply(331, "Password required for %s.", name);

	askpasswd = 1;
	/*
	 * Delay before reading passwd after first failed
	 * attempt to slow down passwd-guessing programs.
	 */
	if (login_attempts)
		sleep((unsigned) login_attempts);
}

/*
 * Check if a user is in the file "fname"
 */
static int
checkuser(fname, name)
	char *fname;
	char *name;
{
	FILE *fp;
	int found = 0;
	char *p, line[BUFSIZ];

	if ((fp = fopen(fname, "r")) != NULL) {
		while (fgets(line, sizeof(line), fp) != NULL)
			if ((p = strchr(line, '\n')) != NULL) {
				*p = '\0';
				if (line[0] == '#')
					continue;
				if (strcmp(line, name) == 0) {
					found = 1;
					break;
				}
			}
		(void) fclose(fp);
	}
	return (found);
}

/*
 * Terminate login as previous user, if any, resetting state;
 * used when USER command is given or login fails.
 */
static void
end_login()
{
	sigset_t allsigs;
	sigfillset (&allsigs);
	sigprocmask (SIG_BLOCK, &allsigs, NULL);
	(void) seteuid((uid_t)0);
	if (logged_in) {
		ftpdlogwtmp(ttyline, "", "");
		if (doutmp)
			logout(utmp.ut_line);
	}
	pw = NULL;
	setusercontext(NULL, getpwuid(0), (uid_t)0,
	    LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);
	logged_in = 0;
	guest = 0;
	dochroot = 0;
}

void
pass(passwd)
	char *passwd;
{
	int rval;
	FILE *fp;
	static char homedir[MAXPATHLEN];
	char *dir, rootdir[MAXPATHLEN];
	sigset_t allsigs;

	if (logged_in || askpasswd == 0) {
		reply(503, "Login with USER first.");
		return;
	}
	askpasswd = 0;
	if (!guest) {		/* "ftp" is only account allowed no password */
		if (pw == NULL) {
			useconds_t us;

			/* Sleep between 1 and 3 seconds to emulate a crypt. */
			us = arc4random() % 3000000;
			usleep(us);
			rval = 1;	/* failure below */
			goto skip;
		}
#if defined(KERBEROS)
		rval = klogin(pw, "", hostname, passwd);
		if (rval == 0)
			goto skip;
#endif
#ifdef SKEY
		if (skey_haskey(pw->pw_name) == 0 &&
		   (skey_passcheck(pw->pw_name, passwd) != -1)) {
			rval = 0;
			goto skip;
		}
#endif
		/* the strcmp does not catch null passwords! */
		if (strcmp(crypt(passwd, pw->pw_passwd), pw->pw_passwd) ||
		    *pw->pw_passwd == '\0') {
			rval = 1;	 /* failure */
			goto skip;
		}
		rval = 0;

skip:
		/*
		 * If rval == 1, the user failed the authentication check
		 * above.  If rval == 0, either Kerberos or local authentication
		 * succeeded.
		 */
		if (rval) {
			reply(530, "Login incorrect.");
			if (logging)
				syslog(LOG_NOTICE,
				    "FTP LOGIN FAILED FROM %s, %s",
				    remotehost, curname);
			pw = NULL;
			if (login_attempts++ >= 5) {
				syslog(LOG_NOTICE,
				    "repeated login failures from %s",
				    remotehost);
				exit(0);
			}
			return;
		}
	} else {
		/* Save anonymous' password. */
		guestpw = strdup(passwd);
		if (guestpw == (char *)NULL)
			fatal("Out of memory");
	}
	login_attempts = 0;		/* this time successful */
	if (setegid((gid_t)pw->pw_gid) < 0) {
		reply(550, "Can't set gid.");
		return;
	}
	(void) umask(defumask);		/* may be overridden by login.conf */
	setusercontext(lc, pw, (uid_t)0,
	    LOGIN_SETGROUP|LOGIN_SETPRIORITY|LOGIN_SETRESOURCES|LOGIN_SETUMASK);

	/* open wtmp before chroot */
	ftpdlogwtmp(ttyline, pw->pw_name, remotehost);

	/* open utmp before chroot */
	if (doutmp) {
		memset((void *)&utmp, 0, sizeof(utmp));
		(void)time(&utmp.ut_time);
		(void)strncpy(utmp.ut_name, pw->pw_name, sizeof(utmp.ut_name));
		(void)strncpy(utmp.ut_host, remotehost, sizeof(utmp.ut_host));
		(void)strncpy(utmp.ut_line, ttyline, sizeof(utmp.ut_line));
		login(&utmp);
	}

	/* open stats file before chroot */
	if (guest && (stats == 1) && (statfd < 0))
		if ((statfd = open(_PATH_FTPDSTATFILE, O_WRONLY|O_APPEND)) < 0)
			stats = 0;

	logged_in = 1;

	dochroot = login_getcapbool(lc, "ftp-chroot", 0) ||
	    checkuser(_PATH_FTPCHROOT, pw->pw_name);
	if ((dir = login_getcapstr(lc, "ftp-dir", NULL, NULL))) {
		free(pw->pw_dir);
		pw->pw_dir = sgetsave(dir);
	}
	if (guest || dochroot) {
		if (multihome && guest) {
			struct stat ts;

			/* Compute root directory. */
			snprintf(rootdir, sizeof(rootdir), "%s/%s",
				  pw->pw_dir, dhostname);
			if (stat(rootdir, &ts) < 0) {
				snprintf(rootdir, sizeof(rootdir), "%s/%s",
					  pw->pw_dir, hostname);
			}
		} else
			strcpy(rootdir, pw->pw_dir);
	}
	if (guest) {
		/*
		 * We MUST do a chdir() after the chroot. Otherwise
		 * the old current directory will be accessible as "."
		 * outside the new root!
		 */
		if (chroot(rootdir) < 0 || chdir("/") < 0) {
			reply(550, "Can't set guest privileges.");
			goto bad;
		}
		strcpy(pw->pw_dir, "/");
		if (setenv("HOME", "/", 1) == -1) {
			reply(550, "Can't setup environment.");
			goto bad;
		}
	} else if (dochroot) {
		if (chroot(rootdir) < 0 || chdir("/") < 0) {
			reply(550, "Can't change root.");
			goto bad;
		}
		strcpy(pw->pw_dir, "/");
		if (setenv("HOME", "/", 1) == -1) {
			reply(550, "Can't setup environment.");
			goto bad;
		}
	} else if (chdir(pw->pw_dir) < 0) {
		if (chdir("/") < 0) {
			reply(530, "User %s: can't change directory to %s.",
			    pw->pw_name, pw->pw_dir);
			goto bad;
		} else
			lreply(230, "No directory! Logging in with home=/");
	}
	if (seteuid((uid_t)pw->pw_uid) < 0) {
		reply(550, "Can't set uid.");
		goto bad;
	}
	sigfillset(&allsigs);
	sigprocmask(SIG_UNBLOCK, &allsigs, NULL);

	/*
	 * Set home directory so that use of ~ (tilde) works correctly.
	 */
	if (getcwd(homedir, MAXPATHLEN) != NULL) {
		if (setenv("HOME", homedir, 1) == -1) {
			reply(550, "Can't setup environment.");
			goto bad;
		}
	}

	/*
	 * Display a login message, if it exists.
	 * N.B. reply(230,) must follow the message.
	 */
	if ((fp = fopen(_PATH_FTPLOGINMESG, "r")) != NULL) {
		char *cp, line[LINE_MAX];

		while (fgets(line, sizeof(line), fp) != NULL) {
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			lreply(230, "%s", line);
		}
		(void) fflush(stdout);
		(void) fclose(fp);
	}
	if (guest) {
		if (ident != NULL)
			free(ident);
		ident = strdup(passwd);
		if (ident == NULL)
			fatal("Ran out of memory.");
		reply(230, "Guest login ok, access restrictions apply.");
#ifdef HASSETPROCTITLE
		snprintf(proctitle, sizeof(proctitle),
		    "%s: anonymous/%.*s", remotehost,
		    (int)(sizeof(proctitle) - sizeof(remotehost) -
		    sizeof(": anonymous/")), passwd);
		setproctitle("%s", proctitle);
#endif /* HASSETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "ANONYMOUS FTP LOGIN FROM %s, %s",
			    remotehost, passwd);
	} else {
		reply(230, "User %s logged in.", pw->pw_name);
#ifdef HASSETPROCTITLE
		snprintf(proctitle, sizeof(proctitle),
		    "%s: %s", remotehost, pw->pw_name);
		setproctitle("%s", proctitle);
#endif /* HASSETPROCTITLE */
		if (logging)
			syslog(LOG_INFO, "FTP LOGIN FROM %s as %s",
			    remotehost, pw->pw_name);
	}
	login_close(lc);
	lc = NULL;
	return;
bad:
	/* Forget all about it... */
	login_close(lc);
	lc = NULL;
	end_login();
}

void
retrieve(cmd, name)
	char *cmd, *name;
{
	FILE *fin, *dout;
	struct stat st;
	int (*closefunc) __P((FILE *));
	time_t start;

	if (cmd == 0) {
		fin = fopen(name, "r"), closefunc = fclose;
		st.st_size = 0;
	} else {
		char line[BUFSIZ];

		(void) snprintf(line, sizeof(line), cmd, name);
		name = line;
		fin = ftpd_popen(line, "r"), closefunc = ftpd_pclose;
		st.st_size = -1;
		st.st_blksize = BUFSIZ;
	}
	if (fin == NULL) {
		if (errno != 0) {
			perror_reply(550, name);
			if (cmd == 0) {
				LOGCMD("get", name);
			}
		}
		return;
	}
	byte_count = -1;
	if (cmd == 0 && (fstat(fileno(fin), &st) < 0 || !S_ISREG(st.st_mode))) {
		reply(550, "%s: not a plain file.", name);
		goto done;
	}
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i, n;
			int c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fin)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
		} else if (lseek(fileno(fin), restart_point, SEEK_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	dout = dataconn(name, st.st_size, "w");
	if (dout == NULL)
		goto done;
	time(&start);
	send_data(fin, dout, st.st_blksize, st.st_size,
		  (restart_point == 0 && cmd == 0 && S_ISREG(st.st_mode)));
	if ((cmd == 0) && stats)
		logxfer(name, st.st_size, start);
	(void) fclose(dout);
	data = -1;
done:
	if (pdata >= 0)
		(void) close(pdata);
	pdata = -1;
	if (cmd == 0)
		LOGBYTES("get", name, byte_count);
	(*closefunc)(fin);
}

void
store(name, mode, unique)
	char *name, *mode;
	int unique;
{
	FILE *fout, *din;
	int (*closefunc) __P((FILE *));
	struct stat st;
	int fd;

	if (restart_point && *mode != 'a')
		mode = "r+";

	if (unique && stat(name, &st) == 0) {
		char *nam;

		fd = guniquefd(name, &nam);
		if (fd == -1) {
			LOGCMD(*mode == 'w' ? "put" : "append", name);
			return;
		}
		name = nam;
		fout = fdopen(fd, mode);
	} else
		fout = fopen(name, mode);

	closefunc = fclose;
	if (fout == NULL) {
		perror_reply(553, name);
		LOGCMD(*mode == 'w' ? "put" : "append", name);
		return;
	}
	byte_count = -1;
	if (restart_point) {
		if (type == TYPE_A) {
			off_t i, n;
			int c;

			n = restart_point;
			i = 0;
			while (i++ < n) {
				if ((c=getc(fout)) == EOF) {
					perror_reply(550, name);
					goto done;
				}
				if (c == '\n')
					i++;
			}
			/*
			 * We must do this seek to "current" position
			 * because we are changing from reading to
			 * writing.
			 */
			if (fseek(fout, 0L, SEEK_CUR) < 0) {
				perror_reply(550, name);
				goto done;
			}
		} else if (lseek(fileno(fout), restart_point, SEEK_SET) < 0) {
			perror_reply(550, name);
			goto done;
		}
	}
	din = dataconn(name, (off_t)-1, "r");
	if (din == NULL)
		goto done;
	if (receive_data(din, fout) == 0) {
		if (unique)
			reply(226, "Transfer complete (unique file name:%s).",
			    name);
		else
			reply(226, "Transfer complete.");
	}
	(void) fclose(din);
	data = -1;
	pdata = -1;
done:
	LOGBYTES(*mode == 'w' ? "put" : "append", name, byte_count);
	(*closefunc)(fout);
}

static FILE *
getdatasock(mode)
	char *mode;
{
	int on = 1, s, t, tries;
	sigset_t allsigs;

	if (data >= 0)
		return (fdopen(data, mode));
	sigfillset(&allsigs);
	sigprocmask (SIG_BLOCK, &allsigs, NULL);
	(void) seteuid((uid_t)0);
	s = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (s < 0)
		goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
	    (char *) &on, sizeof(on)) < 0)
		goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source = ctrl_addr;
	data_source.su_port = htons(20); /* ftp-data port */
	for (tries = 1; ; tries++) {
		if (bind(s, (struct sockaddr *)&data_source,
		    data_source.su_len) >= 0)
			break;
		if (errno != EADDRINUSE || tries > 10)
			goto bad;
		sleep(tries);
	}
	(void) seteuid((uid_t)pw->pw_uid);
	sigfillset(&allsigs);
	sigprocmask (SIG_UNBLOCK, &allsigs, NULL);

#ifdef IP_TOS
	if (ctrl_addr.su_family == AF_INET) {
		on = IPTOS_THROUGHPUT;
		if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on,
		    sizeof(int)) < 0)
			syslog(LOG_WARNING, "setsockopt (IP_TOS): %m");
	}
#endif
#ifdef TCP_NOPUSH
	/*
	 * Turn off push flag to keep sender TCP from sending short packets
	 * at the boundaries of each write().  Should probably do a SO_SNDBUF
	 * to set the send buffer size as well, but that may not be desirable
	 * in heavy-load situations.
	 */
	on = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NOPUSH, (char *)&on, sizeof on) < 0)
		syslog(LOG_WARNING, "setsockopt (TCP_NOPUSH): %m");
#endif
#ifdef SO_SNDBUF
	on = 65536;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&on, sizeof on) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_SNDBUF): %m");
#endif

	return (fdopen(s, mode));
bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
	(void) seteuid((uid_t)pw->pw_uid);
	sigfillset (&allsigs);
	sigprocmask (SIG_UNBLOCK, &allsigs, NULL);
	(void) close(s);
	errno = t;
	return (NULL);
}

static FILE *
dataconn(name, size, mode)
	char *name;
	off_t size;
	char *mode;
{
	char sizebuf[32];
	FILE *file;
	int retry = 0;
	in_port_t *p;
	char *fa, *ha;
	int alen;

	file_size = size;
	byte_count = 0;
	if (size != (off_t) -1) {
		(void) snprintf(sizebuf, sizeof(sizebuf), " (%qd bytes)", 
				size);
	} else
		sizebuf[0] = '\0';
	if (pdata >= 0) {
		union sockunion from;
		int s, fromlen = sizeof(from);

		signal (SIGALRM, toolong);
		(void) alarm ((unsigned) timeout);
		s = accept(pdata, (struct sockaddr *)&from, &fromlen);
		(void) alarm (0);
		if (s < 0) {
			reply(425, "Can't open data connection.");
			(void) close(pdata);
			pdata = -1;
			return (NULL);
		}
		switch (from.su_family) {
		case AF_INET:
			p = (in_port_t *)&from.su_sin.sin_port;
			fa = (u_char *)&from.su_sin.sin_addr;
			ha = (u_char *)&his_addr.su_sin.sin_addr;
			alen = sizeof(struct in_addr);
			break;
		case AF_INET6:
			p = (in_port_t *)&from.su_sin6.sin6_port;
			fa = (u_char *)&from.su_sin6.sin6_addr;
			ha = (u_char *)&his_addr.su_sin6.sin6_addr;
			alen = sizeof(struct in6_addr);
			break;
		default:
			perror_reply(425, "Can't build data connection");
			(void) close(pdata);
			(void) close(s);
			pdata = -1;
			return (NULL);
		}
		if (from.su_family != his_addr.su_family ||
		    ntohs(*p) < IPPORT_RESERVED) {
			perror_reply(425, "Can't build data connection");
			(void) close(pdata);
			(void) close(s);
			pdata = -1;
			return (NULL);
		}
		if (memcmp(fa, ha, alen) != 0) {
			perror_reply(435, "Can't build data connection"); 
			(void) close(pdata);
			(void) close(s);
			pdata = -1;
			return (NULL);
		}
		(void) close(pdata);
		pdata = s;
		reply(150, "Opening %s mode data connection for '%s'%s.",
		    type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
		return (fdopen(pdata, mode));
	}
	if (data >= 0) {
		reply(125, "Using existing data connection for '%s'%s.",
		    name, sizebuf);
		usedefault = 1;
		return (fdopen(data, mode));
	}
	if (usedefault)
		data_dest = his_addr;
	usedefault = 1;
	file = getdatasock(mode);
	if (file == NULL) {
		char hbuf[MAXHOSTNAMELEN], pbuf[10];

		getnameinfo((struct sockaddr *)&data_source, data_source.su_len,
		    hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
		    NI_NUMERICHOST | NI_NUMERICSERV);
		reply(425, "Can't create data socket (%s,%s): %s.",
		    hbuf, pbuf, strerror(errno));
		return (NULL);
	}
	data = fileno(file);

	/*
	 * attempt to connect to reserved port on client machine;
	 * this looks like an attack
	 */
	switch (data_dest.su_family) {
	case AF_INET:
		p = (in_port_t *)&data_dest.su_sin.sin_port;
		fa = (u_char *)&data_dest.su_sin.sin_addr;
		ha = (u_char *)&his_addr.su_sin.sin_addr;
		alen = sizeof(struct in_addr);
		break;
	case AF_INET6:
		p = (in_port_t *)&data_dest.su_sin6.sin6_port;
		fa = (u_char *)&data_dest.su_sin6.sin6_addr;
		ha = (u_char *)&his_addr.su_sin6.sin6_addr;
		alen = sizeof(struct in6_addr);
		break;
	default:
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		pdata = -1;
		return (NULL);
	}
	if (data_dest.su_family != his_addr.su_family ||
	    ntohs(*p) < IPPORT_RESERVED || ntohs(*p) == 2049) {	/* XXX */
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return NULL;
	}
	if (memcmp(fa, ha, alen) != 0) {
		perror_reply(435, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return NULL;
	}
	while (connect(data, (struct sockaddr *)&data_dest,
	    data_dest.su_len) < 0) {
		if (errno == EADDRINUSE && retry < swaitmax) {
			sleep((unsigned) swaitint);
			retry += swaitint;
			continue;
		}
		perror_reply(425, "Can't build data connection");
		(void) fclose(file);
		data = -1;
		return (NULL);
	}
	reply(150, "Opening %s mode data connection for '%s'%s.",
	    type == TYPE_A ? "ASCII" : "BINARY", name, sizebuf);
	return (file);
}

/*
 * Tranfer the contents of "instr" to "outstr" peer using the appropriate
 * encapsulation of the data subject to Mode, Structure, and Type.
 *
 * NB: Form isn't handled.
 */
static void
send_data(instr, outstr, blksize, filesize, isreg)
	FILE *instr, *outstr;
	off_t blksize;
	off_t filesize;
	int isreg;
{
	int c, cnt, filefd, netfd;
	char *buf, *bp;
	size_t len;

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return;
	}
	switch (type) {

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n') {
				if (ferror(outstr))
					goto data_err;
				(void) putc('\r', outstr);
			}
			(void) putc(c, outstr);
		}
		fflush(outstr);
		transflag = 0;
		if (ferror(instr))
			goto file_err;
		if (ferror(outstr))
			goto data_err;
		reply(226, "Transfer complete.");
		return;

	case TYPE_I:
	case TYPE_L:
		/*
		 * isreg is only set if we are not doing restart and we
		 * are sending a regular file
		 */
		netfd = fileno(outstr);
		filefd = fileno(instr);

		if (isreg && filesize < (off_t)16 * 1024 * 1024) {
			buf = mmap(0, filesize, PROT_READ, MAP_SHARED, filefd,
				   (off_t)0);
			if (!buf) {
				syslog(LOG_WARNING, "mmap(%lu): %m",
				    (unsigned long)filesize);
				goto oldway;
			}
			bp = buf;
			len = filesize;
			do {
				cnt = write(netfd, bp, len);
				len -= cnt;
				bp += cnt;
				if (cnt > 0) byte_count += cnt;
			} while(cnt > 0 && len > 0);

			transflag = 0;
			munmap(buf, (size_t)filesize);
			if (cnt < 0)
				goto data_err;
			reply(226, "Transfer complete.");
			return;
		}

oldway:
		if ((buf = malloc((u_int)blksize)) == NULL) {
			transflag = 0;
			perror_reply(451, "Local resource failure: malloc");
			return;
		}

		while ((cnt = read(filefd, buf, (u_int)blksize)) > 0 &&
		    write(netfd, buf, cnt) == cnt)
			byte_count += cnt;
		transflag = 0;
		(void)free(buf);
		if (cnt != 0) {
			if (cnt < 0)
				goto file_err;
			goto data_err;
		}
		reply(226, "Transfer complete.");
		return;
	default:
		transflag = 0;
		reply(550, "Unimplemented TYPE %d in send_data", type);
		return;
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data connection");
	return;

file_err:
	transflag = 0;
	perror_reply(551, "Error on input file");
}

/*
 * Transfer data from peer to "outstr" using the appropriate encapulation of
 * the data subject to Mode, Structure, and Type.
 *
 * N.B.: Form isn't handled.
 */
static int
receive_data(instr, outstr)
	FILE *instr, *outstr;
{
	int c;
	int cnt, bare_lfs = 0;
	char buf[BUFSIZ];

	transflag++;
	if (setjmp(urgcatch)) {
		transflag = 0;
		return (-1);
	}
	switch (type) {

	case TYPE_I:
	case TYPE_L:
		signal (SIGALRM, lostconn);

		do {
			(void) alarm ((unsigned) timeout);
			cnt = read(fileno(instr), buf, sizeof(buf));
			(void) alarm (0);

			if (cnt > 0) {
				if (write(fileno(outstr), buf, cnt) != cnt)
					goto file_err;
				byte_count += cnt;
			}
		} while (cnt > 0);
		if (cnt < 0)
			goto data_err;
		transflag = 0;
		return (0);

	case TYPE_E:
		reply(553, "TYPE E not implemented.");
		transflag = 0;
		return (-1);

	case TYPE_A:
		while ((c = getc(instr)) != EOF) {
			byte_count++;
			if (c == '\n')
				bare_lfs++;
			while (c == '\r') {
				if (ferror(outstr))
					goto data_err;
				if ((c = getc(instr)) != '\n') {
					(void) putc ('\r', outstr);
					if (c == '\0' || c == EOF)
						goto contin2;
				}
			}
			(void) putc(c, outstr);
	contin2:	;
		}
		fflush(outstr);
		if (ferror(instr))
			goto data_err;
		if (ferror(outstr))
			goto file_err;
		transflag = 0;
		if (bare_lfs) {
			lreply(226,
			    "WARNING! %d bare linefeeds received in ASCII mode",
			    bare_lfs);
			printf("   File may not have transferred correctly.\r\n");
		}
		return (0);
	default:
		reply(550, "Unimplemented TYPE %d in receive_data", type);
		transflag = 0;
		return (-1);
	}

data_err:
	transflag = 0;
	perror_reply(426, "Data Connection");
	return (-1);

file_err:
	transflag = 0;
	perror_reply(452, "Error writing file");
	return (-1);
}

void
statfilecmd(filename)
	char *filename;
{
	FILE *fin;
	int c;
	int atstart;
	char line[LINE_MAX];

	(void)snprintf(line, sizeof(line), "/bin/ls -lgA %s", filename);
	fin = ftpd_popen(line, "r");
	lreply(211, "status of %s:", filename);
	atstart = 1;
	while ((c = getc(fin)) != EOF) {
		if (c == '\n') {
			if (ferror(stdout)){
				perror_reply(421, "control connection");
				(void) ftpd_pclose(fin);
				dologout(1);
				/* NOTREACHED */
			}
			if (ferror(fin)) {
				perror_reply(551, filename);
				(void) ftpd_pclose(fin);
				return;
			}
			(void) putc('\r', stdout);
		}
		if (atstart && isdigit(c))
			(void) putc(' ', stdout);
		(void) putc(c, stdout);
		atstart = (c == '\n');
	}
	(void) ftpd_pclose(fin);
	reply(211, "End of Status");
}

void
statcmd()
{
	union sockunion *su;
	u_char *a, *p;
	char hbuf[MAXHOSTNAMELEN];
	int ispassive;

	lreply(211, "%s FTP server status:", hostname, version);
	printf("     %s\r\n", version);
	getnameinfo((struct sockaddr *)&his_addr, his_addr.su_len,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
	printf("     Connected to %s", remotehost);
	if (strcmp(remotehost, hbuf) != 0)
		printf(" (%s)", hbuf);
	printf("\r\n");
	if (logged_in) {
		if (guest)
			printf("     Logged in anonymously\r\n");
		else
			printf("     Logged in as %s\r\n", pw->pw_name);
	} else if (askpasswd)
		printf("     Waiting for password\r\n");
	else
		printf("     Waiting for user name\r\n");
	printf("     TYPE: %s", typenames[type]);
	if (type == TYPE_A || type == TYPE_E)
		printf(", FORM: %s", formnames[form]);
	if (type == TYPE_L)
#if NBBY == 8
		printf(" %d", NBBY);
#else
		printf(" %d", bytesize);	/* need definition! */
#endif
	printf("; STRUcture: %s; transfer MODE: %s\r\n",
	    strunames[stru], modenames[mode]);
	ispassive = 0;
	if (data != -1)
		printf("     Data connection open\r\n");
	else if (pdata != -1) {
		printf("     in Passive mode\r\n");
		su = (union sockunion *)&pasv_addr;
		ispassive++;
		goto printaddr;
	} else if (usedefault == 0) {
		su = (union sockunion *)&data_dest;
printaddr:
		/* PASV/PORT */
		if (su->su_family == AF_INET) {
			if (ispassive)
				printf("211- PASV ");
			else
				printf("211- PORT ");
			a = (u_char *) &su->su_sin.sin_addr;
			p = (u_char *) &su->su_sin.sin_port;
			printf("(%u,%u,%u,%u,%u,%u)\r\n",
			    a[0], a[1], a[2], a[3],
			    p[0], p[1]);
		}

		/* LPSV/LPRT */
	    {
		int alen, af, i;

		alen = 0;
		switch (su->su_family) {
		case AF_INET:
			a = (u_char *) &su->su_sin.sin_addr;
			p = (u_char *) &su->su_sin.sin_port;
			alen = sizeof(su->su_sin.sin_addr);
			af = 4;
			break;
		case AF_INET6:
			a = (u_char *) &su->su_sin6.sin6_addr;
			p = (u_char *) &su->su_sin6.sin6_port;
			alen = sizeof(su->su_sin6.sin6_addr);
			af = 6;
			break;
		default:
			af = 0;
			break;
		}
		if (af) {
			if (ispassive)
				printf("211- LPSV ");
			else
				printf("211- LPRT ");
			printf("(%u,%u", af, alen);
			for (i = 0; i < alen; i++)
				printf(",%u", a[i]);
			printf(",%u,%u,%u)\r\n", 2, p[0], p[1]);
		}
	    }

		/* EPRT/EPSV */
epsvonly:
	    {
		u_char af;

		switch (su->su_family) {
		case AF_INET:
			af = 1;
			break;
		case AF_INET6:
			af = 2;
			break;
		default:
			af = 0;
			break;
		}
		if (af) {
			char hbuf[MAXHOSTNAMELEN], pbuf[10];
			if (getnameinfo((struct sockaddr *)su, su->su_len,
			    hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
			    NI_NUMERICHOST) == 0) {
				if (ispassive)
					printf("211- EPSV ");
				else
					printf("211- EPRT ");
				printf("(|%u|%s|%s|)\r\n",
					af, hbuf, pbuf);
			}
		}
	    }
	} else
		printf("     No data connection\r\n");
	reply(211, "End of status");
}

void
fatal(s)
	char *s;
{

	reply(451, "Error in server: %s\n", s);
	reply(221, "Closing connection due to server error.");
	dologout(0);
	/* NOTREACHED */
}

void
#ifdef __STDC__
reply(int n, const char *fmt, ...)
#else
reply(n, fmt, va_alist)
	int n;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)printf("%d ", n);
	(void)vprintf(fmt, ap);
	(void)printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d ", n);
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

void
#ifdef __STDC__
lreply(int n, const char *fmt, ...)
#else
lreply(n, fmt, va_alist)
	int n;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)printf("%d- ", n);
	(void)vprintf(fmt, ap);
	(void)printf("\r\n");
	(void)fflush(stdout);
	if (debug) {
		syslog(LOG_DEBUG, "<--- %d- ", n);
		vsyslog(LOG_DEBUG, fmt, ap);
	}
}

static void
ack(s)
	char *s;
{

	reply(250, "%s command successful.", s);
}

void
nack(s)
	char *s;
{

	reply(502, "%s command not implemented.", s);
}

/* ARGSUSED */
void
yyerror(s)
	char *s;
{
	char *cp;

	if ((cp = strchr(cbuf,'\n')))
		*cp = '\0';
	reply(500, "'%s': command not understood.", cbuf);
}

void
delete(name)
	char *name;
{
	struct stat st;

	LOGCMD("delete", name);
	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return;
	}
	if ((st.st_mode&S_IFMT) == S_IFDIR) {
		if (rmdir(name) < 0) {
			perror_reply(550, name);
			return;
		}
		goto done;
	}
	if (unlink(name) < 0) {
		perror_reply(550, name);
		return;
	}
done:
	ack("DELE");
}

void
cwd(path)
	char *path;
{
	FILE *message;

	if (chdir(path) < 0)
		perror_reply(550, path);
	else {
		if ((message = fopen(_PATH_CWDMESG, "r")) != NULL) {
			char *cp, line[LINE_MAX];

			while (fgets(line, sizeof(line), message) != NULL) {
				if ((cp = strchr(line, '\n')) != NULL)
					*cp = '\0';
				lreply(250, "%s", line);
			}
			(void) fflush(stdout);
			(void) fclose(message);
		}
		ack("CWD");
	}
}

void
replydirname(name, message)
	const char *name, *message;
{
	char npath[MAXPATHLEN];
	int i;

	for (i = 0; *name != '\0' && i < sizeof(npath) - 1; i++, name++) {
		npath[i] = *name;
		if (*name == '"')
			npath[++i] = '"';
	}
	npath[i] = '\0';
	reply(257, "\"%s\" %s", npath, message);
}

void
makedir(name)
	char *name;
{

	LOGCMD("mkdir", name);
	if (mkdir(name, 0777) < 0)
		perror_reply(550, name);
	else
		replydirname(name, "directory created.");
}

void
removedir(name)
	char *name;
{

	LOGCMD("rmdir", name);
	if (rmdir(name) < 0)
		perror_reply(550, name);
	else
		ack("RMD");
}

void
pwd()
{
	char path[MAXPATHLEN];

	if (getcwd(path, sizeof path) == (char *)NULL)
		reply(550, "Can't get current directory: %s.", strerror(errno));
	else
		replydirname(path, "is current directory.");
}

char *
renamefrom(name)
	char *name;
{
	struct stat st;

	if (stat(name, &st) < 0) {
		perror_reply(550, name);
		return ((char *)0);
	}
	reply(350, "File exists, ready for destination name");
	return (name);
}

void
renamecmd(from, to)
	char *from, *to;
{

	LOGCMD2("rename", from, to);
	if (rename(from, to) < 0)
		perror_reply(550, "rename");
	else
		ack("RNTO");
}

static void
dolog(sa)
	struct sockaddr *sa;
{
	char hbuf[sizeof(remotehost)];

	getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0, 0);
	(void) strncpy(remotehost, hbuf, sizeof(remotehost)-1);

	remotehost[sizeof(remotehost)-1] = '\0';
#ifdef HASSETPROCTITLE
	snprintf(proctitle, sizeof(proctitle), "%s: connected", remotehost);
	setproctitle("%s", proctitle);
#endif /* HASSETPROCTITLE */

	if (logging)
		syslog(LOG_INFO, "connection from %s", remotehost);
}

/*
 * Record logout in wtmp file
 * and exit with supplied status.
 */
void
dologout(status)
	int status;
{
	sigset_t allsigs;

	transflag = 0;

	if (logged_in) {
		sigfillset(&allsigs);
		sigprocmask(SIG_BLOCK, &allsigs, NULL);
		(void) seteuid((uid_t)0);
		ftpdlogwtmp(ttyline, "", "");
		if (doutmp)
			logout(utmp.ut_line);
#if defined(KERBEROS)
		if (!notickets && krbtkfile_env)
			unlink(krbtkfile_env);
#endif
	}
	/* beware of flushing buffers after a SIGPIPE */
	_exit(status);
}

static void
myoob(signo)
	int signo;
{
	char *cp;
	int save_errno = errno;

	/* only process if transfer occurring */
	if (!transflag)
		return;
	cp = tmpline;
	if (getline(cp, 7, stdin) == NULL) {
		reply(221, "You could at least say goodbye.");
		dologout(0);
	}
	upper(cp);
	if (strcmp(cp, "ABOR\r\n") == 0) {
		tmpline[0] = '\0';
		reply(426, "Transfer aborted. Data connection closed.");
		reply(226, "Abort successful");
		longjmp(urgcatch, 1);
	}
	if (strcmp(cp, "STAT\r\n") == 0) {
		tmpline[0] = '\0';
		if (file_size != (off_t) -1)
			reply(213, "Status: %qd of %qd bytes transferred",
			    byte_count, file_size);
		else
			reply(213, "Status: %qd bytes transferred", byte_count);
	}
	errno = save_errno;
}

/*
 * Note: a response of 425 is not mentioned as a possible response to
 *	the PASV command in RFC959. However, it has been blessed as
 *	a legitimate response by Jon Postel in a telephone conversation
 *	with Rick Adams on 25 Jan 89.
 */
void
passive()
{
	int len, on;
	u_char *p, *a;

	if (pw == NULL) {
		reply(530, "Please login with USER and PASS");
		return;
	}
	if (pdata >= 0)
		close(pdata);
	/*
	 * XXX
	 * At this point, it would be nice to have an algorithm that
	 * inserted a growing delay in an attack scenario.  Such a thing
	 * would look like continual passive sockets being opened, but
	 * nothing serious being done with them.  They're not used to
	 * move data; the entire attempt is just to use tcp FIN_WAIT
	 * resources.
	 */
	pdata = socket(AF_INET, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}

#ifdef IP_PORTRANGE
	on = high_data_ports ? IP_PORTRANGE_HIGH : IP_PORTRANGE_DEFAULT;
	if (setsockopt(pdata, IPPROTO_IP, IP_PORTRANGE,
	    (char *)&on, sizeof(on)) < 0)
		goto pasv_error;
#endif

	pasv_addr = ctrl_addr;
	pasv_addr.su_sin.sin_port = 0;
	if (bind(pdata, (struct sockaddr *)&pasv_addr,
		 pasv_addr.su_len) < 0)
		goto pasv_error;

	len = sizeof(pasv_addr);
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	a = (u_char *) &pasv_addr.su_sin.sin_addr;
	p = (u_char *) &pasv_addr.su_sin.sin_port;

	reply(227, "Entering Passive Mode (%u,%u,%u,%u,%u,%u)", a[0],
	    a[1], a[2], a[3], p[0], p[1]);
	return;

pasv_error:
	(void) seteuid((uid_t)pw->pw_uid);
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * 228 Entering Long Passive Mode (af, hal, h1, h2, h3,..., pal, p1, p2...)
 * 229 Entering Extended Passive Mode (|||port|)
 */
void
long_passive(char *cmd, int pf)
{
	int len, on;
	register u_char *p, *a;

	if (!logged_in) {
		syslog(LOG_NOTICE, "long passive but not logged in");
		reply(503, "Login with USER first.");
		return;
	}

	if (pf != PF_UNSPEC) {
		if (ctrl_addr.su_family != pf) {
			switch (ctrl_addr.su_family) {
			case AF_INET:
				pf = 1;
				break;
			case AF_INET6:
				pf = 2;
				break;
			default:
				pf = 0;
				break;
			}
			/*
			 * XXX
			 * only EPRT/EPSV ready clients will understand this
			 */
			if (strcmp(cmd, "EPSV") == 0 && pf) {
				reply(522, "Network protocol mismatch, "
				    "use (%d)", pf);
			} else
				reply(501, "Network protocol mismatch"); /*XXX*/

			return;
		}
	}
 		
	if (pdata >= 0)
		close(pdata);
	/*
	 * XXX
	 * At this point, it would be nice to have an algorithm that
	 * inserted a growing delay in an attack scenario.  Such a thing
	 * would look like continual passive sockets being opened, but
	 * nothing serious being done with them.  They not used to move
	 * data; the entire attempt is just to use tcp FIN_WAIT
	 * resources.
	 */
	pdata = socket(ctrl_addr.su_family, SOCK_STREAM, 0);
	if (pdata < 0) {
		perror_reply(425, "Can't open passive connection");
		return;
	}

	switch (ctrl_addr.su_family) {
	case AF_INET:
#ifdef IP_PORTRANGE
		on = high_data_ports ? IP_PORTRANGE_HIGH : IP_PORTRANGE_DEFAULT;
		if (setsockopt(pdata, IPPROTO_IP, IP_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			goto pasv_error;
#endif
		break;
	case AF_INET6:
#ifdef IPV6_PORTRANGE
		on = high_data_ports ? IPV6_PORTRANGE_HIGH
				     : IPV6_PORTRANGE_DEFAULT;
		if (setsockopt(pdata, IPPROTO_IPV6, IPV6_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			goto pasv_error;
#endif
		break;
	}

	pasv_addr = ctrl_addr;
	pasv_addr.su_port = 0;
	(void) seteuid((uid_t) 0);
	if (bind(pdata, (struct sockaddr *) &pasv_addr, pasv_addr.su_len) < 0) {
		(void) seteuid((uid_t) pw->pw_uid);
		goto pasv_error;
	}
	(void) seteuid((uid_t) pw->pw_uid);
	len = pasv_addr.su_len;
	if (getsockname(pdata, (struct sockaddr *) &pasv_addr, &len) < 0)
		goto pasv_error;
	if (listen(pdata, 1) < 0)
		goto pasv_error;
	p = (u_char *) &pasv_addr.su_port;

	if (strcmp(cmd, "LPSV") == 0) {
		switch (pasv_addr.su_family) {
		case AF_INET:
			a = (u_char *) &pasv_addr.su_sin.sin_addr;
			reply(228,
			    "Entering Long Passive Mode (%u,%u,%u,%u,%u,%u,%u,%u,%u)",
			    4, 4, a[0], a[1], a[2], a[3], 2, p[0], p[1]);
			return;
		case AF_INET6:
			a = (char *) &pasv_addr.su_sin6.sin6_addr;
			reply(228,
			    "Entering Long Passive Mode (%u,%u,%u,%u,%u,%u,"
			    "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u)",
				6, 16, a[0], a[1], a[2], a[3], a[4],
				a[5], a[6], a[7], a[8], a[9], a[10],
				a[11], a[12], a[13], a[14], a[15], 2,
				2, p[0], p[1]);
			return;
		}
	} else if (strcmp(cmd, "EPSV") == 0) {
		switch (pasv_addr.su_family) {
		case AF_INET:
		case AF_INET6:
			reply(229, "Entering Extended Passive Mode (|||%u|)",
			    ntohs(pasv_addr.su_port));
			return;
		}
	} else {
		/* more proper error code? */
	}

  pasv_error:
	(void) close(pdata);
	pdata = -1;
	perror_reply(425, "Can't open passive connection");
	return;
}

/*
 * Generate unique name for file with basename "local".
 * The file named "local" is already known to exist.
 * Generates failure reply on error.
 */
static int
guniquefd(local, nam)
	char *local;
	char **nam;
{
	static char new[MAXPATHLEN];
	struct stat st;
	int count, len, fd;
	char *cp;

	cp = strrchr(local, '/');
	if (cp)
		*cp = '\0';
	if (stat(cp ? local : ".", &st) < 0) {
		perror_reply(553, cp ? local : ".");
		return (-1);
	}
	if (cp)
		*cp = '/';
	(void) strncpy(new, local, sizeof(new)-1);
	new[sizeof(new)-1] = '\0';
	len = strlen(new);
	if (len+2+1 >= sizeof(new)-1)
		return (-1);
	cp = new + len;
	*cp++ = '.';
	for (count = 1; count < 100; count++) {
		(void)snprintf(cp, sizeof(new) - (cp - new), "%d", count);
		fd = open(new, O_RDWR|O_CREAT|O_EXCL, 0666);
		if (fd == -1)
			continue;
		if (nam)
			*nam = new;
		return (fd);
	}
	reply(452, "Unique file name cannot be created.");
	return (-1);
}

/*
 * Format and send reply containing system error number.
 */
void
perror_reply(code, string)
	int code;
	char *string;
{

	reply(code, "%s: %s.", string, strerror(errno));
}

static char *onefile[] = {
	"",
	0
};

void
send_file_list(whichf)
	char *whichf;
{
	struct stat st;
	DIR *dirp = NULL;
	struct dirent *dir;
	FILE *dout = NULL;
	char **dirlist, *dirname;
	int simple = 0;
	int freeglob = 0;
	glob_t gl;

	if (strpbrk(whichf, "~{[*?") != NULL) {
		int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;

		memset(&gl, 0, sizeof(gl));
		freeglob = 1;
		if (glob(whichf, flags, 0, &gl)) {
			reply(550, "not found");
			goto out;
		} else if (gl.gl_pathc == 0) {
			errno = ENOENT;
			perror_reply(550, whichf);
			goto out;
		}
		dirlist = gl.gl_pathv;
	} else {
		onefile[0] = whichf;
		dirlist = onefile;
		simple = 1;
	}

	if (setjmp(urgcatch)) {
		transflag = 0;
		goto out;
	}
	while ((dirname = *dirlist++)) {
		if (stat(dirname, &st) < 0) {
			/*
			 * If user typed "ls -l", etc, and the client
			 * used NLST, do what the user meant.
			 */
			if (dirname[0] == '-' && *dirlist == NULL &&
			    transflag == 0) {
				retrieve("/bin/ls %s", dirname);
				goto out;
			}
			perror_reply(550, whichf);
			if (dout != NULL) {
				(void) fclose(dout);
				transflag = 0;
				data = -1;
				pdata = -1;
			}
			goto out;
		}

		if (S_ISREG(st.st_mode)) {
			if (dout == NULL) {
				dout = dataconn("file list", (off_t)-1, "w");
				if (dout == NULL)
					goto out;
				transflag++;
			}
			fprintf(dout, "%s%s\n", dirname,
				type == TYPE_A ? "\r" : "");
			byte_count += strlen(dirname) + 1;
			continue;
		} else if (!S_ISDIR(st.st_mode))
			continue;

		if ((dirp = opendir(dirname)) == NULL)
			continue;

		while ((dir = readdir(dirp)) != NULL) {
			char nbuf[MAXPATHLEN];

			if (dir->d_name[0] == '.' && dir->d_namlen == 1)
				continue;
			if (dir->d_name[0] == '.' && dir->d_name[1] == '.' &&
			    dir->d_namlen == 2)
				continue;

			snprintf(nbuf, sizeof(nbuf), "%s/%s", dirname,
				 dir->d_name);

			/*
			 * We have to do a stat to insure it's
			 * not a directory or special file.
			 */
			if (simple || (stat(nbuf, &st) == 0 &&
			    S_ISREG(st.st_mode))) {
				if (dout == NULL) {
					dout = dataconn("file list", (off_t)-1,
						"w");
					if (dout == NULL)
						goto out;
					transflag++;
				}
				if (nbuf[0] == '.' && nbuf[1] == '/')
					fprintf(dout, "%s%s\n", &nbuf[2],
						type == TYPE_A ? "\r" : "");
				else
					fprintf(dout, "%s%s\n", nbuf,
						type == TYPE_A ? "\r" : "");
				byte_count += strlen(nbuf) + 1;
			}
		}
		(void) closedir(dirp);
	}

	if (dout == NULL)
		reply(550, "No files found.");
	else if (ferror(dout) != 0)
		perror_reply(550, "Data connection");
	else
		reply(226, "Transfer complete.");

	transflag = 0;
	if (dout != NULL)
		(void) fclose(dout);
	else {
		if (pdata >= 0)
			close(pdata);
	}
	data = -1;
	pdata = -1;
out:
	if (freeglob) {
		freeglob = 0;
		globfree(&gl);
	}
}

static void
reapchild(signo)
	int signo;
{
	int save_errno = errno;

	while (wait3(NULL, WNOHANG, NULL) > 0)
		;
	errno = save_errno;
}

void
logxfer(name, size, start)
	char *name;
	off_t size;
	time_t start;
{
	char buf[400 + MAXHOSTNAMELEN*4 + MAXPATHLEN*4];
	char dir[MAXPATHLEN], path[MAXPATHLEN], rpath[MAXPATHLEN];
	char vremotehost[MAXHOSTNAMELEN*4], vpath[MAXPATHLEN*4];
	char *vpw;
	time_t now;

	if ((statfd >= 0) && (getcwd(dir, sizeof(dir)) != NULL)) {
		time(&now);

		vpw = (char *)malloc(strlen((guest) ? guestpw : pw->pw_name)*4+1);
		if (vpw == NULL)
			return;

		snprintf(path, sizeof path, "%s/%s", dir, name);
		if (realpath(path, rpath) == NULL) {
			strncpy(rpath, path, sizeof rpath-1);
			rpath[sizeof rpath-1] = '\0';
		}
		strvis(vpath, rpath, VIS_SAFE|VIS_NOSLASH);

		strvis(vremotehost, remotehost, VIS_SAFE|VIS_NOSLASH);
		strvis(vpw, (guest) ? guestpw : pw->pw_name, VIS_SAFE|VIS_NOSLASH);

		snprintf(buf, sizeof(buf),
		    "%.24s %d %s %qd %s %c %s %c %c %s ftp %d %s %s\n",
		    ctime(&now), now - start + (now == start),
		    vremotehost, (long long) size, vpath,
		    ((type == TYPE_A) ? 'a' : 'b'), "*" /* none yet */,
		    'o', ((guest) ? 'a' : 'r'),
		    vpw, 0 /* none yet */,
		    ((guest) ? "*" : pw->pw_name), dhostname);
		write(statfd, buf, strlen(buf));
		free(vpw);
	}
}

#if defined(TCPWRAPPERS)
static int
check_host(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in *sin;
	struct hostent *hp;
	char *addr;

	if (sa->sa_family != AF_INET)
		return 1;	/*XXX*/

	sin = (struct sockaddr_in *)sa;
	hp = gethostbyaddr((char *)&sin->sin_addr,
	    sizeof(struct in_addr), AF_INET);
	addr = inet_ntoa(sin->sin_addr);
	if (hp) {
		if (!hosts_ctl("ftpd", hp->h_name, addr, STRING_UNKNOWN)) {
			syslog(LOG_NOTICE, "tcpwrappers rejected: %s [%s]",
			    hp->h_name, addr);
			return (0);
		}
	} else {
		if (!hosts_ctl("ftpd", STRING_UNKNOWN, addr, STRING_UNKNOWN)) {
			syslog(LOG_NOTICE, "tcpwrappers rejected: [%s]", addr);
			return (0);
		}
	}
	return (1);
}
#endif	/* TCPWRAPPERS */
