/*	$OpenBSD: ibcs2_ipc.c,v 1.4 1996/10/17 19:15:47 niklas Exp $	*/
/*	$NetBSD: ibcs2_ipc.c,v 1.6 1996/05/03 17:05:23 christos Exp $	*/

/*
 * Copyright (c) 1995 Scott Bartram
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_signal.h>
#include <compat/ibcs2/ibcs2_syscallargs.h>
#include <compat/ibcs2/ibcs2_util.h>

#define IBCS2_IPC_RMID	0
#define IBCS2_IPC_SET	1
#define IBCS2_IPC_STAT	2

#ifdef SYSVMSG
/*
 * iBCS2 msgsys call
 */

struct ibcs2_msqid_ds {
	struct ipc_perm msg_perm;
	struct msg *msg_first;
	struct msg *msg_last;
	u_short msg_cbytes;
	u_short msg_qnum;
	u_short msg_qbytes;
	u_short msg_lspid;
	u_short msg_lrpid;
	ibcs2_time_t msg_stime;
	ibcs2_time_t msg_rtime;
	ibcs2_time_t msg_ctime;
};

void cvt_msqid2imsqid __P((struct msqid_ds *, struct ibcs2_msqid_ds *));
void cvt_imsqid2msqid __P((struct ibcs2_msqid_ds *, struct msqid_ds *));

void
cvt_msqid2imsqid(bp, ibp)
	struct msqid_ds *bp;
	struct ibcs2_msqid_ds *ibp;
{
	ibp->msg_perm = bp->msg_perm;
	ibp->msg_first = bp->msg_first;
	ibp->msg_last = bp->msg_last;
	ibp->msg_cbytes = (u_short)bp->msg_cbytes;
	ibp->msg_qnum = (u_short)bp->msg_qnum;
	ibp->msg_qbytes = (u_short)bp->msg_qbytes;
	ibp->msg_lspid = (u_short)bp->msg_lspid;
	ibp->msg_lrpid = (u_short)bp->msg_lrpid;
	ibp->msg_stime = bp->msg_stime;
	ibp->msg_rtime = bp->msg_rtime;
	ibp->msg_ctime = bp->msg_ctime;
	return;
}

void
cvt_imsqid2msqid(ibp, bp)
	struct ibcs2_msqid_ds *ibp;
	struct msqid_ds *bp;
{
	bp->msg_perm = ibp->msg_perm;
	bp->msg_first = ibp->msg_first;
	bp->msg_last = ibp->msg_last;
	bp->msg_cbytes = ibp->msg_cbytes;
	bp->msg_qnum = ibp->msg_qnum;
	bp->msg_qbytes = ibp->msg_qbytes;
	bp->msg_lspid = ibp->msg_lspid;
	bp->msg_lrpid = ibp->msg_lrpid;
	bp->msg_stime = ibp->msg_stime;
	bp->msg_rtime = ibp->msg_rtime;
	bp->msg_ctime = ibp->msg_ctime;
	return;
}

int
ibcs2_sys_msgsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_msgsys_args /* {
		syscallarg(int) which;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(int) a4;
		syscallarg(int) a5;
		syscallarg(int) a6;
	} */ *uap = v;

	switch (SCARG(uap, which)) {
	case 0:				/* msgget */
		SCARG(uap, which) = 1;
		return compat_10_sys_msgsys(p, uap, retval);
	case 1: {			/* msgctl */
		int error;
		struct compat_10_sys_msgsys_args margs;
		caddr_t sg = stackgap_init(p->p_emul);

		SCARG(&margs, which) = 0;
		SCARG(&margs, a2) = SCARG(uap, a2);
		SCARG(&margs, a4) =
		    (int)stackgap_alloc(&sg, sizeof(struct msqid_ds));
		SCARG(&margs, a3) = SCARG(uap, a3);
		switch (SCARG(&margs, a3)) {
		case IBCS2_IPC_STAT:
			error = compat_10_sys_msgsys(p, &margs, retval);
			if (!error)
				cvt_msqid2imsqid((struct msqid_ds *)
				    SCARG(&margs, a4),
				    (struct ibcs2_msqid_ds *)SCARG(uap, a4));
			return error;
		case IBCS2_IPC_SET:
			cvt_imsqid2msqid((struct ibcs2_msqid_ds *)SCARG(uap,
									a4),
					 (struct msqid_ds *) SCARG(&margs, a4));
			return compat_10_sys_msgsys(p, &margs, retval);
		case IBCS2_IPC_RMID:
			return compat_10_sys_msgsys(p, &margs, retval);
		}
		return EINVAL;
	}
	case 2:				/* msgrcv */
		SCARG(uap, which) = 3;
		return compat_10_sys_msgsys(p, uap, retval);
	case 3:				/* msgsnd */
		SCARG(uap, which) = 2;
		return compat_10_sys_msgsys(p, uap, retval);
	default:
		return EINVAL;
	}
}
#endif


#ifdef SYSVSEM
/*
 * iBCS2 semsys call
 */

struct ibcs2_semid_ds {
        struct ipc_perm sem_perm;
	struct ibcs2_sem *sem_base;
	u_short sem_nsems;
	int pad1;
	ibcs2_time_t sem_otime;
	ibcs2_time_t sem_ctime;
};

struct ibcs2_sem {
        u_short semval;
	ibcs2_pid_t sempid;
	u_short semncnt;
	u_short semzcnt;
};

void cvt_semid2isemid __P((struct semid_ds *, struct ibcs2_semid_ds *));
void cvt_isemid2semid __P((struct ibcs2_semid_ds *, struct semid_ds *));
#ifdef notdef
void cvt_sem2isem __P((struct sem *, struct ibcs2_sem *));
void cvt_isem2sem __P((struct ibcs2_sem *, struct sem *));

void
cvt_sem2isem(bp, ibp)
	struct sem *bp;
	struct ibcs2_sem *ibp;
{
	ibp->semval = bp->semval;
	ibp->sempid = bp->sempid;
	ibp->semncnt = bp->semncnt;
	ibp->semzcnt = bp->semzcnt;
	return;
}

void
cvt_isem2sem(ibp, bp)
	struct ibcs2_sem *ibp;
	struct sem *bp;
{
	bp->semval = ibp->semval;
	bp->sempid = ibp->sempid;
	bp->semncnt = ibp->semncnt;
	bp->semzcnt = ibp->semzcnt;
	return;
}
#endif

void
cvt_semid2isemid(bp, ibp)
	struct semid_ds *bp;
	struct ibcs2_semid_ds *ibp;
{
	ibp->sem_perm = bp->sem_perm;
	ibp->sem_base = (struct ibcs2_sem *)bp->sem_base;
	ibp->sem_nsems = bp->sem_nsems;
	ibp->sem_otime = bp->sem_otime;
	ibp->sem_ctime = bp->sem_ctime;
	return;
}

void
cvt_isemid2semid(ibp, bp)
	struct ibcs2_semid_ds *ibp;
	struct semid_ds *bp;
{
	bp->sem_perm = ibp->sem_perm;
	bp->sem_base = (struct sem *)ibp->sem_base;
	bp->sem_nsems = ibp->sem_nsems;
	bp->sem_otime = ibp->sem_otime;
	bp->sem_ctime = ibp->sem_ctime;
	return;
}

int
ibcs2_sys_semsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_semsys_args /* {
		syscallarg(int) which;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(int) a4;
		syscallarg(int) a5;
	} */ *uap = v;
	int error;

	switch (SCARG(uap, which)) {
	case 0:					/* semctl */
		switch(SCARG(uap, a4)) {
		case IBCS2_IPC_STAT:
		    {
			struct ibcs2_semid_ds *isp;
			struct semid_ds *sp;
			caddr_t sg = stackgap_init(p->p_emul);

			isp = (struct ibcs2_semid_ds *)SCARG(uap, a5);
			sp = stackgap_alloc(&sg, sizeof(struct semid_ds));
			SCARG(uap, a5) = (int)sp;
			error = compat_10_sys_semsys(p, uap, retval);
			if (!error) {
				SCARG(uap, a5) = (int)isp;
				isp = stackgap_alloc(&sg, sizeof(*isp));
				cvt_semid2isemid(sp, isp);
				error = copyout((caddr_t)isp,
						(caddr_t)SCARG(uap, a5),
						sizeof(*isp));
			}
			return error;
		    }
		case IBCS2_IPC_SET:
		    {
			struct ibcs2_semid_ds *isp;
			struct semid_ds *sp;
			caddr_t sg = stackgap_init(p->p_emul);

			isp = stackgap_alloc(&sg, sizeof(*isp));
			sp = stackgap_alloc(&sg, sizeof(*sp));
			error = copyin((caddr_t)SCARG(uap, a5), (caddr_t)isp,
				       sizeof(*isp));
			if (error)
				return error;
			cvt_isemid2semid(isp, sp);
			SCARG(uap, a5) = (int)sp;
			return compat_10_sys_semsys(p, uap, retval);
		    }
		}
		return compat_10_sys_semsys(p, uap, retval);

	case 1:				/* semget */
		return compat_10_sys_semsys(p, uap, retval);

	case 2:				/* semop */
		return compat_10_sys_semsys(p, uap, retval);
	}
	return EINVAL;
}
#endif


#ifdef SYSVSHM
/*
 * iBCS2 shmsys call
 */

struct ibcs2_shmid_ds {
        struct ipc_perm shm_perm;
	int shm_segsz;
	int pad1;
	char pad2[4];
	u_short shm_lpid;
	u_short shm_cpid;
	u_short shm_nattch;
	u_short shm_cnattch;
	ibcs2_time_t shm_atime;
	ibcs2_time_t shm_dtime;
	ibcs2_time_t shm_ctime;
};

void cvt_shmid2ishmid __P((struct shmid_ds *, struct ibcs2_shmid_ds *));
void cvt_ishmid2shmid __P((struct ibcs2_shmid_ds *, struct shmid_ds *));

void
cvt_shmid2ishmid(bp, ibp)
	struct shmid_ds *bp;
	struct ibcs2_shmid_ds *ibp;
{
	ibp->shm_perm = bp->shm_perm;
	ibp->shm_segsz = bp->shm_segsz;
	ibp->shm_lpid = bp->shm_lpid;
	ibp->shm_cpid = bp->shm_cpid;
	ibp->shm_nattch = bp->shm_nattch;
	ibp->shm_cnattch = 0;			/* ignored anyway */
	ibp->shm_atime = bp->shm_atime;
	ibp->shm_dtime = bp->shm_dtime;
	ibp->shm_ctime = bp->shm_ctime;
	return;
}

void
cvt_ishmid2shmid(ibp, bp)
	struct ibcs2_shmid_ds *ibp;
	struct shmid_ds *bp;
{
	bp->shm_perm = ibp->shm_perm;
	bp->shm_segsz = ibp->shm_segsz;
	bp->shm_lpid = ibp->shm_lpid;
	bp->shm_cpid = ibp->shm_cpid;
	bp->shm_nattch = ibp->shm_nattch;
	bp->shm_atime = ibp->shm_atime;
	bp->shm_dtime = ibp->shm_dtime;
	bp->shm_ctime = ibp->shm_ctime;
	bp->shm_internal = (void *)0;		/* ignored anyway */
	return;
}

int
ibcs2_sys_shmsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_shmsys_args /* {
		syscallarg(int) which;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(int) a4;
	} */ *uap = v;
	int error;

	switch (SCARG(uap, which)) {
	case 0:						/* shmat */
		return compat_10_sys_shmsys(p, uap, retval);

	case 1:						/* shmctl */
		switch(SCARG(uap, a3)) {
		case IBCS2_IPC_STAT:
		    {
			struct ibcs2_shmid_ds *isp;
			struct shmid_ds *sp;
			caddr_t sg = stackgap_init(p->p_emul);

			isp = (struct ibcs2_shmid_ds *)SCARG(uap, a4);
			sp = stackgap_alloc(&sg, sizeof(*sp));
			SCARG(uap, a4) = (int)sp;
			error = compat_10_sys_shmsys(p, uap, retval);
			if (!error) {
				SCARG(uap, a4) = (int)isp;
				isp = stackgap_alloc(&sg, sizeof(*isp));
				cvt_shmid2ishmid(sp, isp);
				error = copyout((caddr_t)isp,
						(caddr_t)SCARG(uap, a4),
						sizeof(*isp));
			}
			return error;
		    }
		case IBCS2_IPC_SET:
		    {
			struct ibcs2_shmid_ds *isp;
			struct shmid_ds *sp;
			caddr_t sg = stackgap_init(p->p_emul);

			isp = stackgap_alloc(&sg, sizeof(*isp));
			sp = stackgap_alloc(&sg, sizeof(*sp));
			error = copyin((caddr_t)SCARG(uap, a4), (caddr_t)isp,
				       sizeof(*isp));
			if (error)
				return error;
			cvt_ishmid2shmid(isp, sp);
			SCARG(uap, a4) = (int)sp;
			return compat_10_sys_shmsys(p, uap, retval);
		    }
		}
		return compat_10_sys_shmsys(p, uap, retval);

	case 2:						/* shmdt */
		return compat_10_sys_shmsys(p, uap, retval);

	case 3:						/* shmget */
		return compat_10_sys_shmsys(p, uap, retval);
	}
	return EINVAL;
}
#endif
