/* pathnames.h.  Generated from pathnames.h.in by configure.  */
/*
 * Copyright (c) 1996, 1998, 1999, 2001, 2004
 *	Todd C. Miller <Todd.Miller@courtesan.com>.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 *
 * $Sudo: pathnames.h.in,v 1.51.2.4 2008/02/09 14:44:48 millert Exp $
 */

/*
 *  Pathnames to programs and files used by sudo.
 */

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */

#ifndef _PATH_DEV
#define _PATH_DEV		"/dev/"
#endif /* _PATH_DEV */

#ifndef _PATH_TTY
#define _PATH_TTY		"/dev/tty"
#endif /* _PATH_TTY */

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL		"/dev/null"
#endif /* _PATH_DEVNULL */

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH		"/usr/bin:/bin"
#endif /* _PATH_DEFPATH */

/*
 * NOTE: _PATH_SUDOERS is usually overriden by the Makefile.
 */
#ifndef _PATH_SUDOERS
#define _PATH_SUDOERS		"/etc/sudoers"
#endif /* _PATH_SUDOERS */

/*
 * NOTE:  _PATH_SUDOERS_TMP is usually overriden by the Makefile.
 *        _PATH_SUDOERS_TMP *MUST* be on the same partition
 *        as _PATH_SUDOERS!
 */
#ifndef _PATH_SUDOERS_TMP
#define _PATH_SUDOERS_TMP	"/etc/sudoers.tmp"
#endif /* _PATH_SUDOERS_TMP */

/*
 * The following paths are controlled via the configure script.
 */

/*
 * Where to put the timestamp files.  Defaults to /var/run/sudo,
 * /var/adm/sudo or /usr/adm/sudo depending on what exists.
 */
#ifndef _PATH_SUDO_TIMEDIR
#define _PATH_SUDO_TIMEDIR "/var/run/sudo"
#endif /* _PATH_SUDO_TIMEDIR */

/*
 * Where to put the sudo log file when logging to a file.  Defaults to
 * /var/log/sudo.log if /var/log exists, else /var/adm/sudo.log.
 */
#ifndef _PATH_SUDO_LOGFILE
#define _PATH_SUDO_LOGFILE "/var/log/sudo.log"
#endif /* _PATH_SUDO_LOGFILE */

#ifndef _PATH_SUDO_SENDMAIL
#define _PATH_SUDO_SENDMAIL "/usr/sbin/sendmail"
#endif /* _PATH_SUDO_SENDMAIL */

#ifndef _PATH_SUDO_NOEXEC
#define _PATH_SUDO_NOEXEC "/usr/local/libexec/sudo_noexec.so"
#endif /* _PATH_SUDO_NOEXEC */

#ifndef _PATH_VI
#define _PATH_VI "/usr/bin/vi"
#endif /* _PATH_VI */

#ifndef _PATH_MV
#define _PATH_MV "/bin/mv"
#endif /* _PATH_MV */

#ifndef _PATH_BSHELL
#define _PATH_BSHELL "/bin/sh"
#endif /* _PATH_BSHELL */

#ifndef _PATH_SUDO_SESH
#define	_PATH_SUDO_SESH "/usr/local/libexec/sesh"
#endif /* _PATH_SUDO_SESH */

#ifndef _PATH_TMP
#define	_PATH_TMP	"/tmp/"
#endif /* _PATH_TMP */

#ifndef _PATH_VARTMP
#define	_PATH_VARTMP	"/var/tmp/"
#endif /* _PATH_VARTMP */

#ifndef _PATH_USRTMP
#define	_PATH_USRTMP	"/usr/tmp/"
#endif /* _PATH_USRTMP */

#ifndef _PATH_LDAP_CONF
#define	_PATH_LDAP_CONF	"/etc/ldap.conf"
#endif /* _PATH_LDAP_CONF */

#ifndef _PATH_LDAP_SECRET
#define _PATH_LDAP_SECRET	"/etc/ldap.secret"
#endif /* _PATH_LDAP_SECRET */
