#	$OpenBSD: Makefile,v 1.34 2012/11/22 19:10:27 miod Exp $

# For ``COMPILER_VERSION'' and ``ELF_TOOLCHAIN''
.include <bsd.own.mk>

LIB=pthread
LIBCSRCDIR=	${.CURDIR}/../libc

CFLAGS+=-Wall -g -Werror -Wshadow
.if ${COMPILER_VERSION:L} == "gcc4" || \
    (${COMPILER_VERSION:L} == "gcc3" && ${ELF_TOOLCHAIN:L} == "yes")
CFLAGS+=-Wmissing-prototypes
.endif
CFLAGS+=-Wstrict-prototypes
CFLAGS+=-Wsign-compare
CFLAGS+=-I${LIBCSRCDIR}/arch/${MACHINE_CPU} -I${LIBCSRCDIR}/include
CDIAGFLAGS=
LDADD = -Wl,-znodelete,-zinitfirst,-znodlopen

.PATH: ${.CURDIR}/arch/${MACHINE_CPU}
SRCS=	rthread.c \
	rthread_attr.c \
	rthread_barrier.c \
	rthread_barrier_attr.c \
	rthread_cancel.c \
	rthread_condattr.c \
	rthread_debug.c \
	rthread_file.c \
	rthread_fork.c \
	rthread_libc.c \
	rthread_mutex_prio.c \
	rthread_mutexattr.c \
	rthread_np.c \
	rthread_once.c \
	rthread_rwlock.c \
	rthread_rwlockattr.c \
	rthread_sched.c \
	rthread_sem.c \
	rthread_sig.c \
	rthread_stack.c \
	rthread_spin_lock.c \
	rthread_sync.c \
	rthread_tls.c \
	sched_prio.c

OBJS+= _atomic_lock.o cerror.o

MAN+=	__tfork_thread.3
MLINKS+=__tfork_thread.3 __tfork.2

SRCDIR= ${.CURDIR}/../libpthread
.include "${SRCDIR}/include/Makefile.inc"
.include "${SRCDIR}/man/Makefile.inc"
.include <bsd.lib.mk>

