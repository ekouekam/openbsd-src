#	$OpenBSD: Makefile,v 1.18 2013/01/31 14:17:48 eric Exp $

.PATH:		${.CURDIR}/..

PROG=	makemap
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/libexec/smtpd
MAN=	makemap.8 newaliases.8
# do not override default manpages for now
NOMAN=	noman

CFLAGS+=	-g3 -ggdb -I${.CURDIR}/..
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded
CFLAGS+=	-DNO_IO

SRCS=	parse.y makemap.c aliases.c expand.c log.c util.c table.c dict.c tree.c to.c

SRCS+=	table_static.c
SRCS+=	table_db.c
SRCS+=	table_getpwnam.c
SRCS+=	table_ldap.c
SRCS+=	ber.c
SRCS+=	aldap.c
SRCS+=	table_sqlite.c

DPADD+= ${LIBUTIL} ${LIBCRYPTO} ${LIBSQLITE3}
LDADD+= -lutil -lcrypto -lsqlite3
.include <bsd.prog.mk>
