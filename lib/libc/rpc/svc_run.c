/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: svc_run.c,v 1.5 1996/08/19 08:31:55 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */
#include <rpc/rpc.h>
#include <sys/errno.h>
#include <unistd.h>

extern int __svc_fdsetsize;
extern fd_set *__svc_fdset;

void
svc_run()
{
	fd_set *fds;

	for (;;) {
		fds = (fd_set *)malloc(howmany(__svc_fdsetsize, NBBY));
		memcpy(fds, __svc_fdset, howmany(__svc_fdsetsize, NBBY));
		switch (select(svc_maxfd+1, fds, 0, 0, (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				free(fds);
				continue;
			}
			perror("svc_run: - select failed");
			free(fds);
			return;
		case 0:
			free(fds);
			continue;
		default:
			svc_getreqset2(fds, svc_maxfd+1);
			free(fds);
		}
	}
}
