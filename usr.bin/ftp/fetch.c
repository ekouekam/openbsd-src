/*	$OpenBSD: fetch.c,v 1.64 2006/05/25 03:45:25 ray Exp $	*/
/*	$NetBSD: fetch.c,v 1.14 1997/08/18 10:20:20 lukem Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason Thorpe and Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(lint) && !defined(SMALL)
static const char rcsid[] = "$OpenBSD: fetch.c,v 1.64 2006/05/25 03:45:25 ray Exp $";
#endif /* not lint and not SMALL */

/*
 * FTP User Program -- Command line file retrieval
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#ifndef SMALL
#include <openssl/ssl.h>
#include <openssl/err.h>
#else
#define SSL void
#endif

#include "ftp_var.h"

static int	url_get(const char *, const char *, const char *);
void		aborthttp(int);
void		abortfile(int);
char		hextochar(const char *);
char		*urldecode(const char *);
int		ftp_printf(FILE *, SSL *, const char *, ...) __attribute__((format(printf, 3, 4)));
char		*ftp_readline(FILE *, SSL *, size_t *);
int		ftp_read(FILE *, SSL *, char *, size_t);
#ifndef SMALL
int		proxy_connect(int, char *);
int		SSL_vprintf(SSL *, const char *, va_list);
char		*SSL_readline(SSL *, size_t *);
#endif

#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */
#define	HTTPS_URL	"https://"	/* https URL prefix */
#define	FILE_URL	"file:"		/* file URL prefix */
#define FTP_PROXY	"ftp_proxy"	/* env var with ftp proxy location */
#define HTTP_PROXY	"http_proxy"	/* env var with http proxy location */


#define EMPTYSTRING(x)	((x) == NULL || (*(x) == '\0'))

static const char *at_encoding_warning =
    "Extra `@' characters in usernames and passwords should be encoded as %%40";

jmp_buf	httpabort;

static int	redirect_loop;

/*
 * Retrieve URL, via the proxy in $proxyvar if necessary.
 * Modifies the string argument given.
 * Returns -1 on failure, 0 on success
 */
static int
url_get(const char *origline, const char *proxyenv, const char *outfile)
{
	char pbuf[NI_MAXSERV], hbuf[NI_MAXHOST], *cp, *portnum, *path;
	char *hosttail, *cause = "unknown", *newline, *host, *port, *buf = NULL;
	int error, i, isftpurl = 0, isfileurl = 0, isredirect = 0, rval = -1;
	struct addrinfo hints, *res0, *res;
	const char * volatile savefile;
	char * volatile proxyurl = NULL;
	volatile int s = -1, out;
	volatile sig_t oldintr;
	FILE *fin = NULL;
	off_t hashbytes;
	const char *errstr;
	size_t len, wlen;
#ifndef SMALL
	char *sslpath = NULL, *sslhost = NULL;
	int ishttpsurl = 0;
	SSL_CTX *ssl_ctx = NULL;
#endif
	SSL *ssl = NULL;

	newline = strdup(origline);
	if (newline == NULL)
		errx(1, "Can't allocate memory to parse URL");
	if (strncasecmp(newline, HTTP_URL, sizeof(HTTP_URL) - 1) == 0)
		host = newline + sizeof(HTTP_URL) - 1;
	else if (strncasecmp(newline, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
		host = newline + sizeof(FTP_URL) - 1;
		isftpurl = 1;
	} else if (strncasecmp(newline, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
		host = newline + sizeof(FILE_URL) - 1;
		isfileurl = 1;
#ifndef SMALL
	} else if (strncasecmp(newline, HTTPS_URL, sizeof(HTTPS_URL) - 1) == 0) {
		host = newline + sizeof(HTTPS_URL) - 1;
		ishttpsurl = 1;
#endif
	} else
		errx(1, "url_get: Invalid URL '%s'", newline);

	if (isfileurl) {
		path = host;
	} else {
		path = strchr(host, '/');		/* find path */
		if (EMPTYSTRING(path)) {
			if (isftpurl)
				goto noftpautologin;
			warnx("Invalid URL (no `/' after host): %s", origline);
			goto cleanup_url_get;
		}
		*path++ = '\0';
		if (EMPTYSTRING(path)) {
			if (isftpurl)
				goto noftpautologin;
			warnx("Invalid URL (no file after host): %s", origline);
			goto cleanup_url_get;
		}
	}

	if (outfile)
		savefile = outfile;
	else
		savefile = basename(path);

	if (EMPTYSTRING(savefile)) {
		if (isftpurl)
			goto noftpautologin;
		warnx("Invalid URL (no file after directory): %s", origline);
		goto cleanup_url_get;
	}

	if (!isfileurl && proxyenv != NULL) {		/* use proxy */
#ifndef SMALL
		if (ishttpsurl) {
			sslpath = strdup(path);
			sslhost = strdup(host);
			if (! sslpath || ! sslhost)
				errx(1, "Can't allocate memory for https path/host.");
		}
#endif
		proxyurl = strdup(proxyenv);
		if (proxyurl == NULL)
			errx(1, "Can't allocate memory for proxy URL.");
		if (strncasecmp(proxyurl, HTTP_URL, sizeof(HTTP_URL) - 1) == 0)
			host = proxyurl + sizeof(HTTP_URL) - 1;
		else if (strncasecmp(proxyurl, FTP_URL, sizeof(FTP_URL) - 1) == 0)
			host = proxyurl + sizeof(FTP_URL) - 1;
		else {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		if (EMPTYSTRING(host)) {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		*--path = '/';			/* add / back to real path */
		path = strchr(host, '/');	/* remove trailing / on host */
		if (!EMPTYSTRING(path))
			*path++ = '\0';
		path = newline;
	}

	if (isfileurl) {
		struct stat st;

		s = open(path, O_RDONLY);
		if (s == -1) {
			warn("Can't open file %s", path);
			goto cleanup_url_get;
		}

		if (fstat(s, &st) == -1)
			filesize = -1;
		else
			filesize = st.st_size;

		/* Open the output file.  */
		if (strcmp(savefile, "-") != 0) {
			out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC,
			    0666);
			if (out < 0) {
				warn("Can't open %s", savefile);
				goto cleanup_url_get;
			}
		} else
			out = fileno(stdout);

		/* Trap signals */
		oldintr = NULL;
		if (setjmp(httpabort)) {
			if (oldintr)
				(void)signal(SIGINT, oldintr);
			goto cleanup_url_get;
		}
		oldintr = signal(SIGINT, abortfile);

		bytes = 0;
		hashbytes = mark;
		progressmeter(-1);

		if ((buf = malloc(4096)) == NULL)
			errx(1, "Can't allocate memory for transfer buffer");

		/* Finally, suck down the file. */
		i = 0;
		while ((len = read(s, buf, 4096)) > 0) {
			bytes += len;
			for (cp = buf; len > 0; len -= i, cp += i) {
				if ((i = write(out, cp, len)) == -1) {
					warn("Writing %s", savefile);
					goto cleanup_url_get;
				}
				else if (i == 0)
					break;
			}
			if (hash && !progress) {
				while (bytes >= hashbytes) {
					(void)putc('#', ttyout);
					hashbytes += mark;
				}
				(void)fflush(ttyout);
			}
		}
		if (hash && !progress && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (len != 0) {
			warn("Reading from file");
			goto cleanup_url_get;
		}
		progressmeter(1);
		if (verbose)
			fputs("Successfully retrieved file.\n", ttyout);
		(void)signal(SIGINT, oldintr);

		rval = 0;
		goto cleanup_url_get;
	}

	if (*host == '[' && (hosttail = strrchr(host, ']')) != NULL &&
	    (hosttail[1] == '\0' || hosttail[1] == ':')) {
		host++;
		*hosttail++ = '\0';
	} else
		hosttail = host;

	portnum = strrchr(hosttail, ':');		/* find portnum */
	if (portnum != NULL)
		*portnum++ = '\0';

	if (debug)
		fprintf(ttyout, "host %s, port %s, path %s, save as %s.\n",
		    host, portnum, path, savefile);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
#ifndef SMALL
	port = portnum ? portnum : (ishttpsurl ? httpsport : httpport);
#else
	port = portnum ? portnum : httpport;
#endif
	error = getaddrinfo(host, port, &hints, &res0);
	/*
	 * If the services file is corrupt/missing, fall back
	 * on our hard-coded defines.
	 */
	if (error == EAI_SERVICE && port == httpport) {
		snprintf(pbuf, sizeof(pbuf), "%d", HTTP_PORT);
		error = getaddrinfo(host, pbuf, &hints, &res0);
#ifndef SMALL
	} else if (error == EAI_SERVICE && port == httpsport) {
		snprintf(pbuf, sizeof(pbuf), "%d", HTTPS_PORT);
		error = getaddrinfo(host, pbuf, &hints, &res0);
#endif
	}
	if (error) {
		warnx("%s: %s", gai_strerror(error), host);
		goto cleanup_url_get;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "(unknown)", sizeof(hbuf));
		if (verbose)
			fprintf(ttyout, "Trying %s...\n", hbuf);

		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

again:
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			int save_errno;

			if (errno == EINTR)
				goto again;
			save_errno = errno;
			close(s);
			errno = save_errno;
			s = -1;
			cause = "connect";
			continue;
		}

		/* get port in numeric */
		if (getnameinfo(res->ai_addr, res->ai_addrlen, NULL, 0,
		    pbuf, sizeof(pbuf), NI_NUMERICSERV) == 0)
			port = pbuf;
		else
			port = NULL;

#ifndef SMALL
		if (proxyenv && sslhost)
			proxy_connect(s, sslhost);
#endif
		break;
	}
	freeaddrinfo(res0);
	if (s < 0) {
		warn("%s", cause);
		goto cleanup_url_get;
	}

#ifndef SMALL
	if (ishttpsurl) {
		if (proxyenv && sslpath) {
			ishttpsurl = 0;
			proxyurl = NULL;
			path = sslpath;
		}
		SSL_library_init();
		SSL_load_error_strings();
		SSLeay_add_ssl_algorithms();
		ssl_ctx = SSL_CTX_new(SSLv23_client_method());
		ssl = SSL_new(ssl_ctx);
		if (ssl == NULL || ssl_ctx == NULL) {
			ERR_print_errors_fp(ttyout);
			goto cleanup_url_get;
		}
		if (SSL_set_fd(ssl, s) == 0) {
			ERR_print_errors_fp(ttyout);
			goto cleanup_url_get;
		}
		if (SSL_connect(ssl) <= 0) {
			ERR_print_errors_fp(ttyout);
			goto cleanup_url_get;;
		}
	} else {
		fin = fdopen(s, "r+");
	}
#else
	fin = fdopen(s, "r+");
#endif

	if (verbose)
		fprintf(ttyout, "Requesting %s", origline);
	/*
	 * Construct and send the request. Proxy requests don't want leading /.
	 */
	if (proxyurl) {
		if (verbose)
			fprintf(ttyout, " (via %s)\n", proxyenv);
		/*
		 * Host: directive must use the destination host address for
		 * the original URI (path).  We do not attach it at this moment.
		 */
		ftp_printf(fin, ssl, "GET %s HTTP/1.0\r\n%s\r\n\r\n", path,
		    HTTP_USER_AGENT);
	} else {
		ftp_printf(fin, ssl, "GET /%s HTTP/1.0\r\nHost: ", path);
		if (strchr(host, ':')) {
			char *h, *p;

			/*
			 * strip off scoped address portion, since it's
			 * local to node
			 */
			h = strdup(host);
			if (h == NULL)
				errx(1, "Can't allocate memory.");
			if ((p = strchr(h, '%')) != NULL)
				*p = '\0';
			ftp_printf(fin, ssl, "[%s]", h);
			free(h);
		} else
			ftp_printf(fin, ssl, "%s", host);

		/*
		 * Send port number only if it's specified and does not equal
		 * 80. Some broken HTTP servers get confused if you explicitly
		 * send them the port number.
		 */
#ifndef SMALL
		if (port && strcmp(port, (ishttpsurl ? "443" : "80")) != 0)
			ftp_printf(fin, ssl, ":%s", port);
#else
		if (port && strcmp(port, "80") != 0)
			ftp_printf(fin, ssl, ":%s", port);
#endif
		ftp_printf(fin, ssl, "\r\n%s\r\n\r\n", HTTP_USER_AGENT);
		if (verbose)
			fprintf(ttyout, "\n");
	}
	if (fin != NULL && fflush(fin) == EOF) {
		warn("Writing HTTP request");
		goto cleanup_url_get;
	}
	if ((buf = ftp_readline(fin, ssl, &len)) == NULL) {
		warn("Receiving HTTP reply");
		goto cleanup_url_get;
	}

	while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
		buf[--len] = '\0';
	if (debug)
		fprintf(ttyout, "received '%s'\n", buf);

	cp = strchr(buf, ' ');
	if (cp == NULL)
		goto improper;
	else
		cp++;
	if (strncmp(cp, "301", 3) == 0 || strncmp(cp, "302", 3) == 0) {
		isredirect++;
		if (redirect_loop++ > 10) {
			warnx("Too many redirections requested");
			goto cleanup_url_get;
		}
	} else if (strncmp(cp, "200", 3)) {
		warnx("Error retrieving file: %s", cp);
		goto cleanup_url_get;
	}

	/*
	 * Read the rest of the header.
	 */
	free(buf);
	filesize = -1;

	for (;;) {
		if ((buf = ftp_readline(fin, ssl, &len)) == NULL) {
			warn("Receiving HTTP reply");
			goto cleanup_url_get;
		}

		while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
			buf[--len] = '\0';
		if (len == 0)
			break;
		if (debug)
			fprintf(ttyout, "received '%s'\n", buf);

		/* Look for some headers */
		cp = buf;
#define CONTENTLEN "Content-Length: "
		if (strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0) {
			cp += sizeof(CONTENTLEN) - 1;
			filesize = strtonum(cp, 0, LLONG_MAX, &errstr);
			if (errstr != NULL)
				goto improper;
#define LOCATION "Location: "
		} else if (isredirect &&
		    strncasecmp(cp, LOCATION, sizeof(LOCATION) - 1) == 0) {
			cp += sizeof(LOCATION) - 1;
			if (verbose)
				fprintf(ttyout, "Redirected to %s\n", cp);
			if (fin != NULL)
				fclose(fin);
			else if (s != -1)
				close(s);
			if (proxyurl)
				free(proxyurl);
			free(newline);
			rval = url_get(cp, proxyenv, outfile);
			if (buf)
				free(buf);
			return (rval);
		}
	}

	/* Open the output file.  */
	if (strcmp(savefile, "-") != 0) {
		out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if (out < 0) {
			warn("Can't open %s", savefile);
			goto cleanup_url_get;
		}
	} else
		out = fileno(stdout);

	/* Trap signals */
	oldintr = NULL;
	if (setjmp(httpabort)) {
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		goto cleanup_url_get;
	}
	oldintr = signal(SIGINT, aborthttp);

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1);

	free(buf);

	/* Finally, suck down the file. */
	if ((buf = malloc(4096)) == NULL)
		errx(1, "Can't allocate memory for transfer buffer");
	i = 0;
	len = 1;
	while (len > 0) {
		len = ftp_read(fin, ssl, buf, 4096);
		bytes += len;
		for (cp = buf, wlen = len; wlen > 0; wlen -= i, cp += i) {
			if ((i = write(out, cp, wlen)) == -1) {
				warn("Writing %s", savefile);
				goto cleanup_url_get;
			}
			else if (i == 0)
				break;
		}
		if (hash && !progress) {
			while (bytes >= hashbytes) {
				(void)putc('#', ttyout);
				hashbytes += mark;
			}
			(void)fflush(ttyout);
		}
	}
	if (hash && !progress && bytes > 0) {
		if (bytes < mark)
			(void)putc('#', ttyout);
		(void)putc('\n', ttyout);
		(void)fflush(ttyout);
	}
	if (len != 0) {
		warn("Reading from socket");
		goto cleanup_url_get;
	}
	progressmeter(1);
	if (filesize != -1 && len == 0 && bytes != filesize) {
		if (verbose)
			fputs("Read short file.\n", ttyout);
		goto cleanup_url_get;
	}

	if (verbose)
		fputs("Successfully retrieved file.\n", ttyout);
	(void)signal(SIGINT, oldintr);

	rval = 0;
	goto cleanup_url_get;

noftpautologin:
	warnx(
	    "Auto-login using ftp URLs isn't supported when using $ftp_proxy");
	goto cleanup_url_get;

improper:
	warnx("Improper response from %s", host);

cleanup_url_get:
#ifndef SMALL
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}
#endif
	if (fin != NULL)
		fclose(fin);
	else if (s != -1)
		close(s);
	if (buf)
		free(buf);
	if (proxyurl)
		free(proxyurl);
	free(newline);
	return (rval);
}

/*
 * Abort a http retrieval
 */
/* ARGSUSED */
void
aborthttp(int signo)
{

	alarmtimer(0);
	fputs("\nhttp fetch aborted.\n", ttyout);
	(void)fflush(ttyout);
	longjmp(httpabort, 1);
}

/*
 * Abort a http retrieval
 */
/* ARGSUSED */
void
abortfile(int signo)
{

	alarmtimer(0);
	fputs("\nfile fetch aborted.\n", ttyout);
	(void)fflush(ttyout);
	longjmp(httpabort, 1);
}

/*
 * Retrieve multiple files from the command line, transferring
 * files of the form "host:path", "ftp://host/path" using the
 * ftp protocol, and files of the form "http://host/path" using
 * the http protocol.
 * If path has a trailing "/", then return (-1);
 * the path will be cd-ed into and the connection remains open,
 * and the function will return -1 (to indicate the connection
 * is alive).
 * If an error occurs the return value will be the offset+1 in
 * argv[] of the file that caused a problem (i.e, argv[x]
 * returns x+1)
 * Otherwise, 0 is returned if all files retrieved successfully.
 */
int
auto_fetch(int argc, char *argv[], char *outfile)
{
	char *xargv[5];
	char *cp, *url, *host, *dir, *file, *portnum;
	char *username, *pass, *pathstart;
	char *ftpproxy, *httpproxy;
	int rval, xargc;
	volatile int argpos;
	int dirhasglob, filehasglob, oautologin;
	char rempath[MAXPATHLEN];

	argpos = 0;

	if (setjmp(toplevel)) {
		if (connected)
			disconnect(0, NULL);
		return (argpos + 1);
	}
	(void)signal(SIGINT, (sig_t)intr);
	(void)signal(SIGPIPE, (sig_t)lostpeer);

	if ((ftpproxy = getenv(FTP_PROXY)) != NULL && *ftpproxy == '\0')
		ftpproxy = NULL;
	if ((httpproxy = getenv(HTTP_PROXY)) != NULL && *httpproxy == '\0')
		httpproxy = NULL;

	/*
	 * Loop through as long as there's files to fetch.
	 */
	for (rval = 0; (rval == 0) && (argpos < argc); free(url), argpos++) {
		if (strchr(argv[argpos], ':') == NULL)
			break;
		host = dir = file = portnum = username = pass = NULL;

		/*
		 * We muck with the string, so we make a copy.
		 */
		url = strdup(argv[argpos]);
		if (url == NULL)
			errx(1, "Can't allocate memory for auto-fetch.");

		/*
		 * Try HTTP URL-style arguments first.
		 */
		if (strncasecmp(url, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
#ifndef SMALL
		    /* even if we compiled without SSL, url_get will check */
		    strncasecmp(url, HTTPS_URL, sizeof(HTTPS_URL) -1) == 0 ||
#endif
		    strncasecmp(url, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
			redirect_loop = 0;
			if (url_get(url, httpproxy, outfile) == -1)
				rval = argpos + 1;
			continue;
		}

		/*
		 * Try FTP URL-style arguments next. If ftpproxy is
		 * set, use url_get() instead of standard ftp.
		 * Finally, try host:file.
		 */
		host = url;
		if (strncasecmp(url, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
			char *passend, *passagain, *userend;

			if (ftpproxy) {
				if (url_get(url, ftpproxy, outfile) == -1)
					rval = argpos + 1;
				continue;
			}
			host += sizeof(FTP_URL) - 1;
			dir = strchr(host, '/');

			/* Look for [user:pass@]host[:port] */

			/* check if we have "user:pass@" */
			userend = strchr(host, ':');
			passend = strchr(host, '@');
			if (passend && userend && userend < passend &&
			    (!dir || passend < dir)) {
				username = host;
				pass = userend + 1;
				host = passend + 1;
				*userend = *passend = '\0';
				passagain = strchr(host, '@');
				if (strchr(pass, '@') != NULL ||
				    (passagain != NULL && passagain < dir)) {
					warnx(at_encoding_warning);
					goto bad_ftp_url;
				}

				if (EMPTYSTRING(username) || EMPTYSTRING(pass)) {
bad_ftp_url:
					warnx("Invalid URL: %s", argv[argpos]);
					rval = argpos + 1;
					continue;
				}
				username = urldecode(username);
				pass = urldecode(pass);
			}

#ifdef INET6
			/* check [host]:port, or [host] */
			if (host[0] == '[') {
				cp = strchr(host, ']');
				if (cp && (!dir || cp < dir)) {
					if (cp + 1 == dir || cp[1] == ':') {
						host++;
						*cp++ = '\0';
					} else
						cp = NULL;
				} else
					cp = host;
			} else
				cp = host;
#else
			cp = host;
#endif

			/* split off host[:port] if there is */
			if (cp) {
				portnum = strchr(cp, ':');
				pathstart = strchr(cp, '/');
				/* : in path is not a port # indicator */
				if (portnum && pathstart &&
				    pathstart < portnum)
					portnum = NULL;

				if (!portnum)
					;
				else {
					if (!dir)
						;
					else if (portnum + 1 < dir) {
						*portnum++ = '\0';
						/*
						 * XXX should check if portnum
						 * is decimal number
						 */
					} else {
						/* empty portnum */
						goto bad_ftp_url;
					}
				}
			} else
				portnum = NULL;
		} else {			/* classic style `host:file' */
			dir = strchr(host, ':');
		}
		if (EMPTYSTRING(host)) {
			rval = argpos + 1;
			continue;
		}

		/*
		 * If dir is NULL, the file wasn't specified
		 * (URL looked something like ftp://host)
		 */
		if (dir != NULL)
			*dir++ = '\0';

		/*
		 * Extract the file and (if present) directory name.
		 */
		if (!EMPTYSTRING(dir)) {
			cp = strrchr(dir, '/');
			if (cp != NULL) {
				*cp++ = '\0';
				file = cp;
			} else {
				file = dir;
				dir = NULL;
			}
		}
		if (debug)
			fprintf(ttyout,
			    "user %s:%s host %s port %s dir %s file %s\n",
			    username, pass, host, portnum, dir, file);

		/*
		 * Set up the connection.
		 */
		if (connected)
			disconnect(0, NULL);
		xargv[0] = __progname;
		xargv[1] = host;
		xargv[2] = NULL;
		xargc = 2;
		if (!EMPTYSTRING(portnum)) {
			xargv[2] = portnum;
			xargv[3] = NULL;
			xargc = 3;
		}
		oautologin = autologin;
		if (username != NULL)
			autologin = 0;
		setpeer(xargc, xargv);
		autologin = oautologin;
		if ((connected == 0) ||
		    ((connected == 1) && !ftp_login(host, username, pass))) {
			warnx("Can't connect or login to host `%s'", host);
			rval = argpos + 1;
			continue;
		}

		/* Always use binary transfers. */
		setbinary(0, NULL);

		dirhasglob = filehasglob = 0;
		if (doglob) {
			if (!EMPTYSTRING(dir) &&
			    strpbrk(dir, "*?[]{}") != NULL)
				dirhasglob = 1;
			if (!EMPTYSTRING(file) &&
			    strpbrk(file, "*?[]{}") != NULL)
				filehasglob = 1;
		}

		/* Change directories, if necessary. */
		if (!EMPTYSTRING(dir) && !dirhasglob) {
			xargv[0] = "cd";
			xargv[1] = dir;
			xargv[2] = NULL;
			cd(2, xargv);
			if (!dirchange) {
				rval = argpos + 1;
				continue;
			}
		}

		if (EMPTYSTRING(file)) {
			rval = -1;
			continue;
		}

		if (verbose)
			fprintf(ttyout, "Retrieving %s/%s\n", dir ? dir : "", file);

		if (dirhasglob) {
			snprintf(rempath, sizeof(rempath), "%s/%s", dir, file);
			file = rempath;
		}

		/* Fetch the file(s). */
		xargc = 2;
		xargv[0] = "get";
		xargv[1] = file;
		xargv[2] = NULL;
		if (dirhasglob || filehasglob) {
			int ointeractive;

			ointeractive = interactive;
			interactive = 0;
			xargv[0] = "mget";
			mget(xargc, xargv);
			interactive = ointeractive;
		} else {
			if (outfile != NULL) {
				xargv[2] = outfile;
				xargv[3] = NULL;
				xargc++;
			}
			get(xargc, xargv);
		}

		if ((code / 100) != COMPLETE)
			rval = argpos + 1;
	}
	if (connected && rval != -1)
		disconnect(0, NULL);
	return (rval);
}

char *
urldecode(const char *str)
{
	char *ret, c;
	int i, reallen;

	if (str == NULL)
		return NULL;
	if ((ret = malloc(strlen(str)+1)) == NULL)
		err(1, "Can't allocate memory for URL decoding");
	for (i = 0, reallen = 0; str[i] != '\0'; i++, reallen++, ret++) {
		c = str[i];
		if (c == '+') {
			*ret = ' ';
			continue;
		}

		/* Cannot use strtol here because next char
		 * after %xx may be a digit.
		 */
		if (c == '%' && isxdigit(str[i+1]) && isxdigit(str[i+2])) {
			*ret = hextochar(&str[i+1]);
			i+=2;
			continue;
		}
		*ret = c;
	}
	*ret = '\0';

	return ret-reallen;
}

char
hextochar(const char *str)
{
	char c, ret;

	c = str[0];
	ret = c;
	if (isalpha(c))
		ret -= isupper(c) ? 'A' - 10 : 'a' - 10;
	else
		ret -= '0';
	ret *= 16;

	c = str[1];
	ret += c;
	if (isalpha(c))
		ret -= isupper(c) ? 'A' - 10 : 'a' - 10;
	else
		ret -= '0';
	return ret;
}

int
isurl(const char *p)
{

	if (strncasecmp(p, FTP_URL, sizeof(FTP_URL) - 1) == 0 ||
	    strncasecmp(p, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
#ifndef SMALL
	    strncasecmp(p, HTTPS_URL, sizeof(HTTPS_URL) - 1) == 0 ||
#endif
	    strncasecmp(p, FILE_URL, sizeof(FILE_URL) - 1) == 0 ||
	    strstr(p, ":/"))
		return (1);
	return (0);
}

char *
ftp_readline(FILE *fp, SSL *ssl, size_t *lenp)
{
	if (fp != NULL)
		return fparseln(fp, lenp, NULL, "\0\0\0", 0);
#ifndef SMALL
	else if (ssl != NULL)
		return SSL_readline(ssl, lenp);
#endif
	else
		return NULL;
}

int
ftp_read(FILE *fp, SSL *ssl, char *buf, size_t len)
{
	int ret;
	if (fp != NULL)
		ret = fread(buf, sizeof(char), len, fp);
#ifndef SMALL
	else if (ssl != NULL)
		ret = SSL_read(ssl, buf, (int)len);
#endif
	else
		ret = 0;
	return (ret);
}

int
ftp_printf(FILE *fp, SSL *ssl, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);

	if (fp != NULL)
		ret = vfprintf(fp, fmt, ap);
#ifndef SMALL
	else if (ssl != NULL)
		ret = SSL_vprintf((SSL*)ssl, fmt, ap);
#endif
	else
		ret = NULL;

	va_end(ap);
	return (ret);
}

#ifndef SMALL
int
SSL_vprintf(SSL *ssl, const char *fmt, va_list ap)
{
	int ret;
	char *string;

	if ((ret = vasprintf(&string, fmt, ap)) == -1)
		return ret;
	ret = SSL_write(ssl, string, ret);
	free(string);
	return ret;
}

char *
SSL_readline(SSL *ssl, size_t *lenp)
{
	size_t i, len;
	char *buf, *q, c;

	len = 128;
	if ((buf = malloc(len)) == NULL)
		errx(1, "Can't allocate memory for transfer buffer");
	for (i = 0; ; i++) {
		if (i >= len - 1) {
			if ((q = realloc(buf, 2 * len)) == NULL)
				errx(1, "Can't expand transfer buffer");
			buf = q;
			len *= 2;
		}
		if (SSL_read(ssl, &c, 1) <= 0)
			break;
		buf[i] = c;
		if (c == '\n')
			break;
	}
	*lenp = i;
	return (buf);
}

int
proxy_connect(int socket, char *host)
{
	char buf[1024];
	char *connstr, *hosttail, *port;

	if (*host == '[' && (hosttail = strrchr(host, ']')) != NULL &&
		(hosttail[1] == '\0' || hosttail[1] == ':')) {
		host++;
		*hosttail++ = '\0';
	} else
		hosttail = host;

	port = strrchr(hosttail, ':');               /* find portnum */
	if (port != NULL)
		*port++ = '\0';
	if (!port)
		port = "443";

	if (debug)
		printf("CONNECT %s:%s HTTP/1.1\n\n", host, port);
	asprintf(&connstr, "CONNECT %s:%s HTTP/1.1\n\n", host, port);
	if (!connstr)
		errx(1, "Could not allocate memory to assemble connect string!");
	if (write(socket, connstr, strlen(connstr)) != strlen(connstr))
		 errx(1, "Could not send connect string: %s", strerror(errno));
	read(socket, &buf, sizeof(buf)); /* only proxy header XXX: error handling? */
	return(200);
}
#endif
