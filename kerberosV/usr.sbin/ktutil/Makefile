# $OpenBSD: Makefile,v 1.17 2012/12/19 09:54:05 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	ktutil-commands.c \
	list.c \
	purge.c \
	remove.c \
	rename.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/admin
.PATH:	${KRB5SRC}/lib/sl

slc:	slc-gram.o slc-lex.o ${PRINTVER}
	$(CC) -o slc $> -lkrb5 -lcrypto

slc-lex.o: y.tab.h

y.tab.h slc-gram.o: 
	${YACC} -d ${KRB5SRC}/lib/sl/slc-gram.y
	$(CC) -c ${CFLAGS} -o slc-gram.o y.tab.c
	rm -f y.tab.c
	touch $@

SLC=	slc

depend: ktutil-commands.h

ktutil-commands.c ktutil-commands.h:	ktutil-commands.in $(SLC)
	./$(SLC) $(KRB5SRC)/admin/ktutil-commands.in

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5 -I.

DPADD+=	${LIBEDIT} ${LIBCURSES} ${LIBKADM5CLNT} ${LIBKRB5} \
	${LIBCRYPTO}
LDADD+=	-ledit -lcurses -lkadm5clnt -lkrb5 \
	-lcrypto

CLEANFILES+=	ktutil-commands.c slc slc-lex.o \
		ktutil-commands.h slc-gram.o stamp_grammar

.include <bsd.prog.mk>
