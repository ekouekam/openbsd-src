/*	$OpenBSD: yp_passwd.c,v 1.15 2000/12/12 02:19:59 millert Exp $	*/

/*
 * Copyright (c) 1988 The Regents of the University of California.
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
/*static char sccsid[] = "from: @(#)yp_passwd.c	1.0 2/2/93";*/
static char rcsid[] = "$OpenBSD: yp_passwd.c,v 1.15 2000/12/12 02:19:59 millert Exp $";
#endif /* not lint */

#ifdef	YP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#define passwd yp_passwd_rec
#include <rpcsvc/yppasswd.h>
#undef passwd

#ifndef _PASSWORD_LEN
#define _PASSWORD_LEN PASS_MAX
#endif

extern	int pwd_gensalt __P(( char *, int, struct passwd *, char));
extern	int pwd_check __P((struct passwd *, char *));
extern	int pwd_gettries __P((struct passwd *));

char *ypgetnewpasswd __P((struct passwd *, char **));
struct passwd *ypgetpwnam __P((char *));

char *domain;

static int
pw_error(name, err, eval)
	char *name;
	int err, eval;
{
	if (err) 
		warn("%s", name);

	warnx("YP passwd database unchanged.");
	exit(eval);
}

int
yp_passwd(username)
	char *username;
{
	char *master;
	int r, rpcport, status;
	uid_t uid;
	struct yppasswd yppasswd;
	struct passwd *pw;
	struct timeval tv;
	CLIENT *client;

	/*
	 * Get local domain
	 */
	if ((r = yp_get_default_domain(&domain)) != 0) {
		warnx("can't get local YP domain. Reason: %s", yperr_string(r));
		return(1);
	}

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		warnx("can't find the master YP server. Reason: %s",
		    yperr_string(r));
		return(1);
	}

	/*
	 * Ask the portmapper for the port of the daemon.
	 */
	if ((rpcport = getrpcport(master, YPPASSWDPROG,
	    YPPASSWDPROC_UPDATE, IPPROTO_UDP)) == 0) {
		warnx("master YP server not running yppasswd daemon.");
		warnx("Can't change password.");
		return(1);
	}

	/*
	 * Be sure the port is priviledged
	 */
	if (rpcport >= IPPORT_RESERVED) {
		warnx("yppasswd daemon is on an invalid port.");
		return(1);
	}

	/* Get user's login identity */
	if (!(pw = ypgetpwnam(username))) {
		warnx("unknown user %s.", username);
		return(1);
	}
		
	uid = getuid();
	if (uid && uid != pw->pw_uid) {
		warnx("you may only change your own password: %s", strerror(EACCES));
		return(1);
	}

	/* prompt for new password */
	yppasswd.newpw.pw_passwd = ypgetnewpasswd(pw, &yppasswd.oldpass);

	/* tell rpc.yppasswdd */
	yppasswd.newpw.pw_name	= pw->pw_name;
	yppasswd.newpw.pw_uid 	= pw->pw_uid;
	yppasswd.newpw.pw_gid	= pw->pw_gid;
	yppasswd.newpw.pw_gecos = pw->pw_gecos;
	yppasswd.newpw.pw_dir	= pw->pw_dir;
	yppasswd.newpw.pw_shell	= pw->pw_shell;
	
	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client==NULL) {
		warnx("cannot contact yppasswdd on %s: Reason: %s",
		    master, yperr_string(YPERR_YPBIND));
		free(yppasswd.newpw.pw_passwd);
		return(YPERR_YPBIND);
	}
	client->cl_auth = authunix_create_default();
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	r = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &yppasswd, xdr_int, &status, tv);
	if (r)
		warnx("rpc to yppasswdd failed.");
	else if (status) {
		printf("Couldn't change YP password.\n");
		free(yppasswd.newpw.pw_passwd);
		return(1);
	}
	printf("The YP password has been changed on %s, the master YP passwd server.\n",
	    master);
	free(yppasswd.newpw.pw_passwd);
	return(0);
}

char *
ypgetnewpasswd(pw, old_pass)
	struct passwd *pw;
	char **old_pass;
{
	static char buf[_PASSWORD_LEN+1];
	char *p;
	int tries, pwd_tries;
	char salt[_PASSWORD_LEN];
	
	printf("Changing YP password for %s.\n", pw->pw_name);
	if (old_pass) {
		*old_pass = NULL;
	
		if (pw->pw_passwd[0]) {
			p = getpass("Old password:");
			if (strcmp(crypt(p, pw->pw_passwd), pw->pw_passwd)) {
				errno = EACCES;
				pw_error(NULL, 1, 1);
			}
		} else
			p = "";
		*old_pass = strdup(p);
	}

	pwd_tries = pwd_gettries(pw);

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (strcmp(p, "s/key") == 0) {
			printf("That password collides with a system feature. Choose another.\n");
			continue;
		}
		if ((tries++ < pwd_tries || pwd_tries == 0) 
		    && pwd_check(pw, p) == 0)
			continue;
		strncpy(buf, p, sizeof buf-1);
		buf[sizeof buf-1] = '\0';
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
        if( !pwd_gensalt( salt, _PASSWORD_LEN, pw, 'y' )) {
                (void)printf("Couldn't generate salt.\n");
                pw_error(NULL, 0, 0);
        }
	return(strdup(crypt(buf, salt)));
}

static char *
pwskip(char *p)
{
	while (*p && *p != ':' && *p != '\n')
		++p;
	if (*p)
		*p++ = 0;
	return (p);
}

struct passwd *
interpret(struct passwd *pwent, char *line)
{
	char	*p = line;

	pwent->pw_passwd = "*";
	pwent->pw_uid = 0;
	pwent->pw_gid = 0;
	pwent->pw_gecos = "";
	pwent->pw_dir = "";
	pwent->pw_shell = "";
	pwent->pw_change = 0;
	pwent->pw_expire = 0;
	pwent->pw_class = "";
	
	/* line without colon separators is no good, so ignore it */
	if(!strchr(p, ':'))
		return(NULL);

	pwent->pw_name = p;
	p = pwskip(p);
	pwent->pw_passwd = p;
	p = pwskip(p);
	pwent->pw_uid = (uid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gid = (gid_t)strtoul(p, NULL, 10);
	p = pwskip(p);
	pwent->pw_gecos = p;
	p = pwskip(p);
	pwent->pw_dir = p;
	p = pwskip(p);
	pwent->pw_shell = p;
	while (*p && *p != '\n')
		p++;
	*p = '\0';
	return (pwent);
}

static char *__yplin;

struct passwd *
ypgetpwnam(nam)
	char *nam;
{
	static struct passwd pwent;
	char *val;
	int reason, vallen;
	
	reason = yp_match(domain, "passwd.byname", nam, strlen(nam),
	    &val, &vallen);
	switch(reason) {
	case 0:
		break;
	default:
		return (NULL);
		break;
	}
	val[vallen] = '\0';
	if (__yplin)
		free(__yplin);
	__yplin = (char *)malloc(vallen + 1);
	strncpy(__yplin, val, vallen);
	__yplin[vallen] = '\0';
	free(val);

	return(interpret(&pwent, __yplin));
}

#endif	/* YP */
