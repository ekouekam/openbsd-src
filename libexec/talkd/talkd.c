/*	$OpenBSD: talkd.c,v 1.9 2001/07/08 21:18:12 deraadt Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)talkd.c	5.8 (Berkeley) 2/26/91";*/
static char rcsid[] = "$Id: talkd.c,v 1.9 2001/07/08 21:18:12 deraadt Exp $";
#endif /* not lint */

/*
 * The top level of the daemon, the format is heavily borrowed
 * from rwhod.c. Basically: find out who and where you are; 
 * disconnect all descriptors and ttys, and then endless
 * loop on waiting for and processing requests
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <protocols/talkd.h>
#include <signal.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include "talkd.h"

int	sockt;
int	debug = 0;
void	timeout();
long	lastmsgtime;

char	hostname[MAXHOSTNAMELEN];

#define TIMEOUT 30
#define MAXIDLE 120

int
main(argc, argv)
	int argc;
	char *argv[];
{
	if (getuid()) {
		fprintf(stderr, "%s: getuid: not super-user", argv[0]);
		exit(1);
	}
	openlog("talkd", LOG_PID, LOG_DAEMON);
	if (gethostname(hostname, sizeof (hostname) - 1) < 0) {
		syslog(LOG_ERR, "gethostname: %m");
		_exit(1);
	}
	if (chdir(_PATH_DEV) < 0) {
		syslog(LOG_ERR, "chdir: %s: %m", _PATH_DEV);
		_exit(1);
	}
	if (argc > 1 && strcmp(argv[1], "-d") == 0)
		debug = 1;
	init_table();
	signal(SIGALRM, timeout);
	alarm(TIMEOUT);
	for (;;) {
		CTL_MSG		request;
		CTL_RESPONSE	response;
		int		cc;
		int		len = sizeof(response.addr);

		cc = recvfrom(0, (char *)&request, sizeof (request), 0,
			(struct sockaddr *)&response.addr, &len);
		if (cc != sizeof (request)) {
			if (cc < 0 && errno != EINTR)
				syslog(LOG_WARNING, "recvfrom: %m");
			continue;
		}
		/* Force NUL termination */
		request.l_name[NAME_SIZE-1] = '\0';
		request.r_name[NAME_SIZE-1] = '\0';
		request.r_tty[TTY_SIZE-1] = '\0';

		lastmsgtime = time(0);
		process_request(&request, &response);
		/* can block here, is this what I want? */
		cc = sendto(sockt, (char *)&response,
		    sizeof (response), 0, (struct sockaddr *)&request.ctl_addr,
		    sizeof (request.ctl_addr));
		if (cc != sizeof (response))
			syslog(LOG_WARNING, "sendto: %m");
	}
}

void
timeout()
{
	int save_errno = errno;

	if (time(0) - lastmsgtime >= MAXIDLE)
		_exit(0);
	alarm(TIMEOUT);
	errno = save_errno;
}
