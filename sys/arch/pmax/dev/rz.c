/*	$NetBSD: rz.c,v 1.23 1997/02/04 05:24:55 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory and Ralph Campbell.
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
 *
 *	@(#)rz.c	8.1 (Berkeley) 7/29/93
 */

/*
 * SCSI CCS (Command Command Set) disk driver.
 * NOTE: The name was changed from "sd" to "rz" for DEC naming compatibility.
 * I guess I can't avoid confusion someplace.
 */
#include "rz.h"
#if NRZ > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <ufs/ffs/fs.h>

#include <pmax/dev/device.h>
#include <pmax/dev/scsi.h>

#include <machine/pte.h>

#include <sys/conf.h>
#include <machine/conf.h>

int	rzprobe __P((void /*register struct pmax_scsi_device*/ *sd));
void	rzstart __P((int unit));
void	rzdone __P((int unit, int error, int resid, int status));
void	rzgetinfo __P((dev_t dev));
int	rzsize __P((dev_t dev));



struct	pmax_driver rzdriver = {
	"rz", rzprobe,
	(void	(*) __P((struct ScsiCmd *cmd))) rzstart,
	rzdone,
};

struct	size {
	u_long	strtblk;
	u_long	nblocks;
};

/*
 * Since the SCSI standard tends to hide the disk structure, we define
 * partitions in terms of DEV_BSIZE blocks.  The default partition table
 * (for an unlabeled disk) reserves 8K for a boot area, has an 8 meg
 * root and 32 meg of swap.  The rest of the space on the drive goes in
 * the G partition.  As usual, the C partition covers the entire disk
 * (including the boot area).
 */
static struct size rzdefaultpart[MAXPARTITIONS] = {
	{       0,   16384 },	/* A */
	{   16384,   65536 },	/* B */
	{       0,       0 },	/* C */
	{   17408,       0 },	/* D */
	{  115712,       0 },	/* E */
	{  218112,       0 },	/* F */
	{   81920,       0 },	/* G */
	{  115712,       0 }	/* H */
};

extern char *
readdisklabel __P((dev_t dev, void (*strat) __P((struct buf *bp)),
		   struct disklabel *lp, struct cpu_disklabel *osdep));

/*
 * Ultrix disklabel declarations
 */
 #ifdef COMPAT_ULTRIX
#include <pmax/stand/dec_boot.h>

extern char *
compat_label __P((dev_t dev, void (*strat) __P((struct buf *bp)),
		  struct disklabel *lp, struct cpu_disklabel *osdep));
#endif


#define	RAWPART		2	/* 'c' partition */	/* XXX */

struct rzstats {
	long	rzresets;
	long	rztransfers;
	long	rzpartials;
};

struct	rz_softc {
	struct	device sc_dev;		/* new config glue */
	struct	pmax_scsi_device *sc_sd;	/* physical unit info */
	pid_t	sc_format_pid;		/* process using "format" mode */
	short	sc_flags;		/* see below */
	short	sc_type;		/* drive type from INQUIRY cmd */
	u_int	sc_blks;		/* number of blocks on device */
	int	sc_blksize;		/* device block size in bytes */
	struct	disk sc_dkdev;		/* generic disk device info */
#define	sc_label	sc_dkdev.dk_label	/* XXX compat */
#define	sc_openpart	sc_dkdev.dk_openmask	/* XXX compat */
#define	sc_bopenpart	sc_dkdev.dk_bopenmask	/* XXX compat */
#define	sc_copenpart	sc_dkdev.dk_copenmask	/* XXX compat */
#define	sc_bshift	sc_dkdev.dk_blkshift	/* XXX compat */
	struct	rzstats sc_stats;	/* statisic counts */
	struct	buf sc_tab;		/* queue of pending operations */
	struct	buf sc_buf;		/* buf for doing I/O */
	struct	buf sc_errbuf;		/* buf for doing REQUEST_SENSE */
	struct	ScsiCmd sc_cmd;		/* command for controller */
	ScsiGroup1Cmd sc_rwcmd;		/* SCSI cmd if not in "format" mode */
	struct	scsi_fmt_cdb sc_cdb;	/* SCSI cmd if in "format" mode */
	struct	scsi_fmt_sense sc_sense;	/* sense data from last cmd */
	u_char	sc_capbuf[8];		/* buffer for SCSI_READ_CAPACITY */
} rz_softc[NRZ];

/* sc_flags values */
#define	RZF_ALIVE		0x0001	/* drive found and ready */
#define	RZF_SENSEINPROGRESS	0x0002	/* REQUEST_SENSE command in progress */
#define	RZF_ALTCMD		0x0004	/* alternate command in progress */
#define	RZF_HAVELABEL		0x0008	/* valid label found on disk */
#define	RZF_WLABEL		0x0010	/* label is writeable */
#define	RZF_WAIT		0x0020	/* waiting for sc_tab to drain */
#define	RZF_REMOVEABLE		0x0040	/* disk is removable */
#define	RZF_TRYSYNC		0x0080	/* try synchronous operation */
#define	RZF_NOERR		0x0100	/* don't print error messages */

#ifdef DEBUG
#define RZB_ERROR	0x01
#define RZB_PARTIAL	0x02
#define RZB_PRLABEL	0x04
int	rzdebug = RZB_ERROR;
#endif

#define	rzunit(x)	(minor(x) >> 3)
#define rzpart(x)	(minor(x) & 0x7)
#define	b_cylin		b_resid

/*
 * Table of scsi commands users are allowed to access via "format" mode.
 *  0 means not legal.
 *  1 means legal.
 */
static char legal_cmds[256] = {
/*****  0   1   2   3   4   5   6   7     8   9   A   B   C   D   E   F */
/*00*/	0,  0,  0,  0,  1,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*10*/	0,  0,  1,  0,  0,  1,  0,  0,    0,  0,  1,  0,  0,  0,  0,  0,
/*20*/	0,  0,  0,  0,  0,  1,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*30*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*40*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*50*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*60*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*70*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*80*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*90*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*a0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*b0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*c0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*d0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*e0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*f0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
};

/*
 * Private forward declarations
 */

static	int rzready __P((register struct rz_softc *sc));
static void rzlblkstrat __P((register struct buf *bp, register int bsize));

/*
 * Test to see if the unit is ready and if not, try to make it ready.
 * Also, find the drive capacity.
 */
static int
rzready(sc)
	register struct rz_softc *sc;
{
	register int tries, i;
	ScsiClass7Sense *sp;

	/* don't print SCSI errors */
	sc->sc_flags |= RZF_NOERR;

	/* see if the device is ready */
	for (tries = 10; ; ) {
		sc->sc_cdb.len = sizeof(ScsiGroup0Cmd);
		scsiGroup0Cmd(SCSI_TEST_UNIT_READY, sc->sc_rwcmd.unitNumber,
			0, 0, (ScsiGroup0Cmd *)sc->sc_cdb.cdb);
		sc->sc_buf.b_flags = B_BUSY | B_PHYS | B_READ;
		sc->sc_buf.b_bcount = 0;
		sc->sc_buf.b_un.b_addr = (caddr_t)0;
		sc->sc_buf.b_actf = (struct buf *)0;
		sc->sc_tab.b_actf = &sc->sc_buf;

		sc->sc_cmd.cmd = sc->sc_cdb.cdb;
		sc->sc_cmd.cmdlen = sc->sc_cdb.len;
		sc->sc_cmd.buf = (caddr_t)0;
		sc->sc_cmd.buflen = 0;
		/* setup synchronous data transfers if the device supports it */
		if (tries == 10 && (sc->sc_flags & RZF_TRYSYNC))
			sc->sc_cmd.flags = SCSICMD_USE_SYNC;
		else
			sc->sc_cmd.flags = 0;

		disk_busy(&sc->sc_dkdev);	/* XXX */
		(*sc->sc_sd->sd_cdriver->d_start)(&sc->sc_cmd);
		if (!biowait(&sc->sc_buf))
			break;
		if (--tries < 0)
			return (0);
		if (!(sc->sc_sense.status & SCSI_STATUS_CHECKCOND))
			goto again;
		sp = (ScsiClass7Sense *)sc->sc_sense.sense;
		if (sp->error7 != 0x70)
			goto again;
		if (sp->key == SCSI_CLASS7_UNIT_ATTN && tries != 9) {
			/* drive recalibrating, give it a while */
			DELAY(1000000);
			continue;
		}
		if (sp->key == SCSI_CLASS7_NOT_READY) {
			ScsiStartStopCmd *cp;

			/* try to spin-up disk with start/stop command */
			sc->sc_cdb.len = sizeof(ScsiGroup0Cmd);
			cp = (ScsiStartStopCmd *)sc->sc_cdb.cdb;
			cp->command = SCSI_START_STOP;
			cp->unitNumber = sc->sc_rwcmd.unitNumber;
			cp->immed = 0;
			cp->loadEject = 0;
			cp->start = 1;
			cp->pad1 = 0;
			cp->pad2 = 0;
			cp->pad3 = 0;
			cp->pad4 = 0;
			cp->control = 0;
			sc->sc_buf.b_flags = B_BUSY | B_PHYS | B_READ;
			sc->sc_buf.b_bcount = 0;
			sc->sc_buf.b_un.b_addr = (caddr_t)0;
			sc->sc_buf.b_actf = (struct buf *)0;
			sc->sc_tab.b_actf = &sc->sc_buf;
			rzstart(sc->sc_cmd.unit);
			if (biowait(&sc->sc_buf))
				return (0);
			continue;
		}
	again:
		DELAY(1000);
	}

	/* print SCSI errors */
	sc->sc_flags &= ~RZF_NOERR;

	/* find out how big a disk this is */
	sc->sc_cdb.len = sizeof(ScsiGroup1Cmd);
	scsiGroup1Cmd(SCSI_READ_CAPACITY, sc->sc_rwcmd.unitNumber, 0, 0,
		(ScsiGroup1Cmd *)sc->sc_cdb.cdb);
	sc->sc_buf.b_flags = B_BUSY | B_PHYS | B_READ;
	sc->sc_buf.b_bcount = sizeof(sc->sc_capbuf);
	sc->sc_buf.b_un.b_addr = (caddr_t)sc->sc_capbuf;
	sc->sc_buf.b_actf = (struct buf *)0;
	sc->sc_tab.b_actf = &sc->sc_buf;
	sc->sc_flags |= RZF_ALTCMD;
	rzstart(sc->sc_cmd.unit);
	sc->sc_flags &= ~RZF_ALTCMD;
	if (biowait(&sc->sc_buf) || sc->sc_buf.b_resid != 0)
		return (0);
	sc->sc_blks = ((sc->sc_capbuf[0] << 24) | (sc->sc_capbuf[1] << 16) |
		(sc->sc_capbuf[2] << 8) | sc->sc_capbuf[3]) + 1;
	sc->sc_blksize = (sc->sc_capbuf[4] << 24) | (sc->sc_capbuf[5] << 16) |
		(sc->sc_capbuf[6] << 8) | sc->sc_capbuf[7];

	sc->sc_bshift = 0;
	for (i = sc->sc_blksize; i > DEV_BSIZE; i >>= 1)
		++sc->sc_bshift;
	sc->sc_blks <<= sc->sc_bshift;

	return (1);
}

/*
 * Test to see if device is present.
 * Return true if found and initialized ok.
 */
int
rzprobe(xxxsd)
	void *xxxsd;
{
	register struct pmax_scsi_device *sd = xxxsd;
	register struct rz_softc *sc = &rz_softc[sd->sd_unit];
	register int i;
	ScsiInquiryData inqbuf;

	if (sd->sd_unit >= NRZ)
		return (0);

	/* init some parameters that don't change */
	sc->sc_sd = sd;
	sc->sc_cmd.sd = sd;
	sc->sc_cmd.unit = sd->sd_unit;
	sc->sc_rwcmd.unitNumber = sd->sd_slave;

	/* XXX set up the external name */
	bzero(&sc->sc_dev, sizeof(sc->sc_dev));			/* XXX */
	sprintf(sc->sc_dev.dv_xname, "rz%d", sd->sd_unit);	/* XXX */
	sc->sc_dev.dv_unit = sd->sd_unit;			/* XXX */
	sc->sc_dev.dv_class = DV_DISK;				/* XXX */

	/* Initialize the disk structure. */
	bzero(&sc->sc_dkdev, sizeof(sc->sc_dkdev));
	sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;

	/* try to find out what type of device this is */
	sc->sc_format_pid = 1;		/* force use of sc_cdb */
	sc->sc_flags = RZF_NOERR;	/* don't print SCSI errors */
	sc->sc_cdb.len = sizeof(ScsiGroup0Cmd);
	scsiGroup0Cmd(SCSI_INQUIRY, sd->sd_slave, 0, sizeof(inqbuf),
		(ScsiGroup0Cmd *)sc->sc_cdb.cdb);
	sc->sc_buf.b_flags = B_BUSY | B_PHYS | B_READ;
	sc->sc_buf.b_bcount = sizeof(inqbuf);
	sc->sc_buf.b_un.b_addr = (caddr_t)&inqbuf;
	sc->sc_buf.b_actf = (struct buf *)0;
	sc->sc_tab.b_actf = &sc->sc_buf;
	rzstart(sd->sd_unit);

/*XXX*/	/*printf("probe rz%d\n", sd->sd_unit);*/

	if (biowait(&sc->sc_buf) ||
	    (i = sizeof(inqbuf) - sc->sc_buf.b_resid) < 5)
		goto bad;
	switch (inqbuf.type) {
	case SCSI_DISK_TYPE:		/* disk */
	case SCSI_WORM_TYPE:		/* WORM */
	case SCSI_ROM_TYPE:		/* CD-ROM */
	case SCSI_OPTICAL_MEM_TYPE:	/* Magneto-optical */
		break;

	default:			/* not a disk */
		printf("rz%d: unknown media code 0x%x\n",
		       sd->sd_unit, inqbuf.type);
		goto bad;
	}
	sc->sc_type = inqbuf.type;
	if (inqbuf.flags & SCSI_SYNC)
		sc->sc_flags |= RZF_TRYSYNC;

	if (!inqbuf.rmb) {
		if (!rzready(sc))
			goto bad;
	}

	printf("rz%d at %s%d drive %d slave %d", sd->sd_unit,
		sd->sd_cdriver->d_name, sd->sd_ctlr, sd->sd_drive,
		sd->sd_slave);
	if (inqbuf.version < 1 || i < 36)
		printf(" type 0x%x, qual 0x%x, ver %d",
			inqbuf.type, inqbuf.qualifier, inqbuf.version);
	else {
		char vid[9], pid[17], revl[5];

		bcopy((caddr_t)inqbuf.vendorID, (caddr_t)vid, 8);
		bcopy((caddr_t)inqbuf.productID, (caddr_t)pid, 16);
		bcopy((caddr_t)inqbuf.revLevel, (caddr_t)revl, 4);
		for (i = 8; --i > 0; )
			if (vid[i] != ' ')
				break;
		vid[i+1] = 0;
		for (i = 16; --i > 0; )
			if (pid[i] != ' ')
				break;
		pid[i+1] = 0;
		for (i = 4; --i > 0; )
			if (revl[i] != ' ')
				break;
		revl[i+1] = 0;
		printf(" <%s %s rev %s>\n", vid, pid, revl);
	}
	if (sc->sc_blks) printf("rz%d: %dMB, %d %d byte blocks\n",
		sd->sd_unit, (sc->sc_blks * sc->sc_blksize) / 1048576,
		sc->sc_blks, sc->sc_blksize);
	if (!inqbuf.rmb && sc->sc_blksize != DEV_BSIZE) {
		if (sc->sc_blksize < DEV_BSIZE) {
			printf("rz%d: need %d byte blocks - drive ignored\n",
				sd->sd_unit, DEV_BSIZE);
			goto bad;
		}
	}

	/* Attach the disk. */
	disk_attach(&sc->sc_dkdev);

	sc->sc_format_pid = 0;
	sc->sc_flags |= RZF_ALIVE;
	if (inqbuf.rmb)
		sc->sc_flags |= RZF_REMOVEABLE;
	sc->sc_buf.b_flags = 0;

	sd->sd_devp = &sc->sc_dev;				/* XXX */
	TAILQ_INSERT_TAIL(&alldevs, &sc->sc_dev, dv_list);	/* XXX */

	return (1);

bad:
	/* doesn't exist or not a CCS device */
	sc->sc_format_pid = 0;
	sc->sc_buf.b_flags = 0;
	return (0);
}

/*
 * This routine is called for partial block transfers and non-aligned
 * transfers (the latter only being possible on devices with a block size
 * larger than DEV_BSIZE).  The operation is performed in three steps
 * using a locally allocated buffer:
 *	1. transfer any initial partial block
 *	2. transfer full blocks
 *	3. transfer any final partial block
 */
static void
rzlblkstrat(bp, bsize)
	register struct buf *bp;
	register int bsize;
{
	register struct buf *cbp;
	caddr_t cbuf;
	register int bn, resid;
	register caddr_t addr;

	cbp = (struct buf *)malloc(sizeof(struct buf), M_DEVBUF, M_WAITOK);
	cbuf = (caddr_t)malloc(bsize, M_DEVBUF, M_WAITOK);
	bzero((caddr_t)cbp, sizeof(*cbp));
	cbp->b_proc = curproc;
	cbp->b_dev = bp->b_dev;
	bn = bp->b_blkno;
	resid = bp->b_bcount;
	addr = bp->b_un.b_addr;
#ifdef DEBUG
	if (rzdebug & RZB_PARTIAL)
		printf("rzlblkstrat: bp %p flags %lx bn %x resid %x addr %p\n",
		       bp, bp->b_flags, bn, resid, addr);
#endif

	while (resid > 0) {
		register int boff = dbtob(bn) & (bsize - 1);
		register int count;

		if (boff || resid < bsize) {
			rz_softc[rzunit(bp->b_dev)].sc_stats.rzpartials++;
			count = min(resid, bsize - boff);
			cbp->b_flags = B_BUSY | B_PHYS | B_READ;
			cbp->b_blkno = bn - btodb(boff);
			cbp->b_un.b_addr = cbuf;
			cbp->b_bcount = bsize;
#ifdef DEBUG
			if (rzdebug & RZB_PARTIAL)
				printf(" readahead: bn %x cnt %x off %x addr %p\n",
				       cbp->b_blkno, count, boff, addr);
#endif
			rzstrategy(cbp);
			biowait(cbp);
			if (cbp->b_flags & B_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_error = cbp->b_error;
				break;
			}
			if (bp->b_flags & B_READ) {
				bcopy(&cbuf[boff], addr, count);
				goto done;
			}
			bcopy(addr, &cbuf[boff], count);
#ifdef DEBUG
			if (rzdebug & RZB_PARTIAL)
				printf(" writeback: bn %x cnt %x off %x addr %p\n",
				       cbp->b_blkno, count, boff, addr);
#endif
		} else {
			count = resid & ~(bsize - 1);
			cbp->b_blkno = bn;
			cbp->b_un.b_addr = addr;
			cbp->b_bcount = count;
#ifdef DEBUG
			if (rzdebug & RZB_PARTIAL)
				printf(" fulltrans: bn %x cnt %x addr %p\n",
				       cbp->b_blkno, count, addr);
#endif
		}
		cbp->b_flags = B_BUSY | B_PHYS | (bp->b_flags & B_READ);
		rzstrategy(cbp);
		biowait(cbp);
		if (cbp->b_flags & B_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = cbp->b_error;
			break;
		}
done:
		bn += btodb(count);
		resid -= count;
		addr += count;
#ifdef DEBUG
		if (rzdebug & RZB_PARTIAL)
			printf(" done: bn %x resid %x addr %p\n",
			       bn, resid, addr);
#endif
	}
	free(cbuf, M_DEVBUF);
	free(cbp, M_DEVBUF);
}

void
rzstrategy(bp)
	register struct buf *bp;
{
	register int unit = rzunit(bp->b_dev);
	register int part = rzpart(bp->b_dev);
	register struct rz_softc *sc = &rz_softc[unit];
	register struct partition *pp = &sc->sc_label->d_partitions[part];
	register daddr_t bn;
	register long sz, s;

	if (sc->sc_format_pid) {
		if (sc->sc_format_pid != curproc->p_pid) {
			bp->b_error = EPERM;
			goto bad;
		}
		bp->b_cylin = 0;
	} else {
		bn = bp->b_blkno;
		sz = howmany(bp->b_bcount, DEV_BSIZE);
		if ((unsigned)bn + sz > pp->p_size) {
			sz = pp->p_size - bn;
			/* if exactly at end of disk, return an EOF */
			if (sz == 0) {
				bp->b_resid = bp->b_bcount;
				goto done;
			}
			/* if none of it fits, error */
			if (sz < 0) {
				bp->b_error = EINVAL;
				goto bad;
			}
			/* otherwise, truncate */
			bp->b_bcount = dbtob(sz);
		}
		/* check for write to write protected label */
		if (bn + pp->p_offset <= LABELSECTOR &&
#if LABELSECTOR != 0
		    bn + pp->p_offset + sz > LABELSECTOR &&
#endif
		    !(bp->b_flags & B_READ) && !(sc->sc_flags & RZF_WLABEL)) {
			bp->b_error = EROFS;
			goto bad;
		}
		/*
		 * Non-aligned or partial-block transfers handled specially.
		 */
		s = sc->sc_blksize - 1;
		if ((dbtob(bn) & s) || (bp->b_bcount & s)) {
			rzlblkstrat(bp, sc->sc_blksize);
			goto done;
		}
		bp->b_cylin = (bn + pp->p_offset) >> sc->sc_bshift;
	}
	/* don't let disksort() see sc_errbuf */
	while (sc->sc_flags & RZF_SENSEINPROGRESS)
		printf("SENSE\n"); /* XXX */
	s = splbio();
	disksort(&sc->sc_tab, bp);
	if (sc->sc_tab.b_active == 0) {
		sc->sc_tab.b_active = 1;
		rzstart(unit);
	}
	splx(s);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	biodone(bp);
}

void
rzstart(unit)
	int unit;
{
	register struct rz_softc *sc = &rz_softc[unit];
	register struct buf *bp = sc->sc_tab.b_actf;
	register int n;

	sc->sc_cmd.buf = bp->b_un.b_addr;
	sc->sc_cmd.buflen = bp->b_bcount;

	if (sc->sc_format_pid ||
	    (sc->sc_flags & (RZF_SENSEINPROGRESS | RZF_ALTCMD))) {
		sc->sc_cmd.flags = !(bp->b_flags & B_READ) ?
			SCSICMD_DATA_TO_DEVICE : 0;
		sc->sc_cmd.cmd = sc->sc_cdb.cdb;
		sc->sc_cmd.cmdlen = sc->sc_cdb.len;
	} else {
		if (bp->b_flags & B_READ) {
			sc->sc_cmd.flags = 0;
			sc->sc_rwcmd.command = SCSI_READ_EXT;
		} else {
			sc->sc_cmd.flags = SCSICMD_DATA_TO_DEVICE;
			sc->sc_rwcmd.command = SCSI_WRITE_EXT;
		}
		sc->sc_cmd.cmd = (u_char *)&sc->sc_rwcmd;
		sc->sc_cmd.cmdlen = sizeof(sc->sc_rwcmd);
		n = bp->b_cylin;
		sc->sc_rwcmd.highAddr = n >> 24;
		sc->sc_rwcmd.midHighAddr = n >> 16;
		sc->sc_rwcmd.midLowAddr = n >> 8;
		sc->sc_rwcmd.lowAddr = n;
		n = howmany(bp->b_bcount, sc->sc_blksize);
		sc->sc_rwcmd.highBlockCount = n >> 8;
		sc->sc_rwcmd.lowBlockCount = n;
#ifdef DEBUG
		if ((bp->b_bcount & (sc->sc_blksize - 1)) != 0)
			printf("rz%d: partial block xfer -- %lx bytes\n",
				unit, bp->b_bcount);
#endif
		sc->sc_stats.rztransfers++;
	}


	/* Instrumentation. */
	disk_busy(&sc->sc_dkdev);
	sc->sc_dkdev.dk_seek++;		/* XXX */

	/* tell controller to start this command */
	(*sc->sc_sd->sd_cdriver->d_start)(&sc->sc_cmd);
}

/*
 * This is called by the controller driver when the command is done.
 */
void
rzdone(unit, error, resid, status)
	register int unit;
	int error;		/* error number from errno.h */
	int resid;		/* amount not transfered */
	int status;		/* SCSI status byte */
{
	register struct rz_softc *sc = &rz_softc[unit];
	register struct buf *bp = sc->sc_tab.b_actf;
	register struct pmax_scsi_device *sd = sc->sc_sd;

	if (bp == NULL) {
		printf("rz%d: bp == NULL\n", unit);
		return;
	}

	disk_unbusy(&sc->sc_dkdev, (bp->b_bcount - resid));

	if (sc->sc_flags & RZF_SENSEINPROGRESS) {
		sc->sc_flags &= ~RZF_SENSEINPROGRESS;
		sc->sc_tab.b_actf = bp = bp->b_actf;	/* remove sc_errbuf */

		if (error || (status & SCSI_STATUS_CHECKCOND)) {
#ifdef DEBUG
			if (rzdebug & RZB_ERROR)
				printf("rz%d: error reading sense data: error %d scsi status 0x%x\n",
					unit, error, status);
#endif
			/*
			 * We got an error during the REQUEST_SENSE,
			 * fill in no sense for data.
			 */
			sc->sc_sense.sense[0] = 0x70;
			sc->sc_sense.sense[2] = SCSI_CLASS7_NO_SENSE;
		} else if (!(sc->sc_flags & RZF_NOERR)) {
			printf("rz%d: ", unit);
			scsiPrintSense((ScsiClass7Sense *)sc->sc_sense.sense,
				sizeof(sc->sc_sense.sense) - resid);
		}
	} else if (error || (status & SCSI_STATUS_CHECKCOND)) {
#ifdef DEBUG
		if (!(sc->sc_flags & RZF_NOERR) && (rzdebug & RZB_ERROR))
			printf("rz%d: error %d scsi status 0x%x\n",
				unit, error, status);
#endif
		/* save error info */
		sc->sc_sense.status = status;
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
		bp->b_resid = resid;

		if (status & SCSI_STATUS_CHECKCOND) {
			/*
			 * Start a REQUEST_SENSE command.
			 * Since we are called at interrupt time, we can't
			 * wait for the command to finish; that's why we use
			 * the sc_flags field.
			 */
			sc->sc_flags |= RZF_SENSEINPROGRESS;
			sc->sc_cdb.len = sizeof(ScsiGroup0Cmd);
			scsiGroup0Cmd(SCSI_REQUEST_SENSE, sd->sd_slave, 0,
				sizeof(sc->sc_sense.sense),
				(ScsiGroup0Cmd *)sc->sc_cdb.cdb);
			sc->sc_errbuf.b_flags = B_BUSY | B_PHYS | B_READ;
			sc->sc_errbuf.b_bcount = sizeof(sc->sc_sense.sense);
			sc->sc_errbuf.b_un.b_addr = (caddr_t)sc->sc_sense.sense;
			sc->sc_errbuf.b_actf = bp;
			sc->sc_tab.b_actf = &sc->sc_errbuf;
			rzstart(unit);
			return;
		}
	} else {
		sc->sc_sense.status = status;
		bp->b_resid = resid;
	}

	sc->sc_tab.b_actf = bp->b_actf;
	biodone(bp);
	if (sc->sc_tab.b_actf)
		rzstart(unit);
	else {
		sc->sc_tab.b_active = 0;
		/* finish close protocol */
		if (sc->sc_openpart == 0)
			wakeup((caddr_t)&sc->sc_tab);
	}
}


/*
 * Read or constuct a disklabel
 */
void
rzgetinfo(dev)
	dev_t dev;
{
	register int unit = rzunit(dev);
	register struct rz_softc *sc = &rz_softc[unit];
	register struct disklabel *lp = sc->sc_label;
	register int i;
	char *msg;
	int part;
	struct cpu_disklabel cd;

	part = rzpart(dev);
	sc->sc_flags |= RZF_HAVELABEL;

	if (sc->sc_type == SCSI_ROM_TYPE) {
		lp->d_type = DTYPE_SCSI;
		lp->d_secsize = sc->sc_blksize;
		lp->d_nsectors = 100;
		lp->d_ntracks = 1;
		lp->d_ncylinders = (sc->sc_blks / 100) + 1;
		lp->d_secpercyl	= 100;
		lp->d_secperunit = sc->sc_blks;
		lp->d_rpm = 300;
		lp->d_interleave = 1;
		lp->d_flags = D_REMOVABLE;
		lp->d_npartitions = 1;
		lp->d_partitions[0].p_offset = 0;
		lp->d_partitions[0].p_size = sc->sc_blks;
		lp->d_partitions[0].p_fstype = FS_ISO9660;
		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		lp->d_checksum = dkcksum(lp);
		return;
	}

	lp->d_type = DTYPE_SCSI;
	lp->d_secsize = DEV_BSIZE;
	lp->d_secpercyl = 1 << sc->sc_bshift;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_partitions[part].p_offset = 0;
	lp->d_partitions[part].p_size = sc->sc_blks;

	/*
	 * Now try to read the disklabel
	 */
	msg = readdisklabel(dev, rzstrategy, lp, &cd);
	if (msg == NULL)
		return;
	printf("rz%d: WARNING: %s\n", unit, msg);

#ifdef	COMPAT_ULTRIX
	/*
	 * No native label, try and substitute  Ultrix label
	 */
	msg = compat_label(dev, rzstrategy, lp, &cd);
	if (msg == NULL) {
	  	printf("rz%d: WARNING: using ULTRIX partition information",
		       unit);
		return;
	}
	printf("rz%d: WARNING: Ultrix label, %s\n", unit, msg);
#endif
	/*
	 * No label found. Concoct one from compile-time default.
	 */
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_type = DTYPE_SCSI;
	lp->d_subtype = 0;
	lp->d_typename[0] = '\0';
	lp->d_secsize = DEV_BSIZE;
	lp->d_secperunit = sc->sc_blks;
	lp->d_npartitions = MAXPARTITIONS;
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;
	for (i = 0; i < MAXPARTITIONS; i++) {
		lp->d_partitions[i].p_size = rzdefaultpart[i].nblocks;
		lp->d_partitions[i].p_offset = rzdefaultpart[i].strtblk;
	}

	lp->d_partitions[RAWPART].p_size = sc->sc_blks;
}

int
rzopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register int unit = rzunit(dev);
	register struct rz_softc *sc = &rz_softc[unit];
	register struct disklabel *lp;
	register int i;
	int part;
	int mask;

	if (unit >= NRZ || !(sc->sc_flags & RZF_ALIVE))
		return (ENXIO);

	/* make sure disk is ready */
	if (sc->sc_flags & RZF_REMOVEABLE) {
		if (!rzready(sc))
			return (ENXIO);
	}

	/* try to read disk label and partition table information */
	part = rzpart(dev);
	if (!(sc->sc_flags & RZF_HAVELABEL))
		rzgetinfo(dev);

	lp = sc->sc_label;
	if (part >= lp->d_npartitions || lp->d_partitions[part].p_size == 0)
	{
		printf("rzopen: ENXIO on rz%d%c unit %d part %d\n",
			unit, "abcdefg"[part],  unit, part);
		printf("# partions %d, size of %d = %d\n",
		       lp->d_npartitions, part,
		       lp->d_partitions[part].p_size);
		return (ENXIO);
	}

	/*
	 * Warn if a partition is opened that overlaps another
	 * already open, unless either is the `raw' partition
	 * (whole disk).
	 */
	mask = 1 << part;
	if ((sc->sc_openpart & mask) == 0 && part != RAWPART) {
		register struct partition *pp;
		u_long start, end;

		pp = &lp->d_partitions[part];
		start = pp->p_offset;
		end = pp->p_offset + pp->p_size;
		for (pp = lp->d_partitions, i = 0;
		     i < lp->d_npartitions; pp++, i++) {
			if (pp->p_offset + pp->p_size <= start ||
			    pp->p_offset >= end || i == RAWPART)
				continue;
			if (sc->sc_openpart & (1 << i))
				log(LOG_WARNING,
				    "rz%d%c: overlaps open partition (%c)\n",
				    unit, part + 'a', i + 'a');
		}
	}
	switch (mode) {
	case S_IFCHR:
		sc->sc_copenpart |= mask;
		break;
	case S_IFBLK:
		sc->sc_bopenpart |= mask;
		break;
	}
	sc->sc_openpart |= mask;

	return (0);
}

int
rzclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register struct rz_softc *sc = &rz_softc[rzunit(dev)];
	int mask = (1 << rzpart(dev));
	int s;

	switch (mode) {
	case S_IFCHR:
		sc->sc_copenpart &= ~mask;
		break;
	case S_IFBLK:
		sc->sc_bopenpart &= ~mask;
		break;
	}
	sc->sc_openpart = sc->sc_copenpart | sc->sc_bopenpart;

	/*
	 * Should wait for I/O to complete on this partition even if
	 * others are open, but wait for work on blkflush().
	 */
	if (sc->sc_openpart == 0) {
		s = splbio();
		while (sc->sc_tab.b_actf)
			sleep((caddr_t)&sc->sc_tab, PZERO - 1);
		splx(s);
		sc->sc_flags &= ~RZF_WLABEL;
	}
	return (0);
}

int
rzread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	register struct rz_softc *sc = &rz_softc[rzunit(dev)];

	if (sc->sc_type == SCSI_ROM_TYPE)
		return (EROFS);

	if (sc->sc_format_pid && sc->sc_format_pid != curproc->p_pid)
		return (EPERM);

	return (physio(rzstrategy, (struct buf *)0, dev,
		B_READ, minphys, uio));
}

int
rzwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	register struct rz_softc *sc = &rz_softc[rzunit(dev)];

	if (sc->sc_format_pid && sc->sc_format_pid != curproc->p_pid)
		return (EPERM);

	return (physio(rzstrategy, (struct buf *)0, dev,
		B_WRITE, minphys, uio));
}

int
rzioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register struct rz_softc *sc = &rz_softc[rzunit(dev)];
	int error;
	int flags;
	struct cpu_disklabel cd;

	switch (cmd) {
	default:
		return (EINVAL);

	case SDIOCSFORMAT:
		/* take this device into or out of "format" mode */
		if (suser(p->p_ucred, &p->p_acflag))
			return (EPERM);

		if (*(int *)data) {
			if (sc->sc_format_pid)
				return (EPERM);
			sc->sc_format_pid = p->p_pid;
		} else
			sc->sc_format_pid = 0;
		return (0);

	case SDIOCGFORMAT:
		/* find out who has the device in format mode */
		*(int *)data = sc->sc_format_pid;
		return (0);

	case SDIOCSCSICOMMAND:
		/*
		 * Save what user gave us as SCSI cdb to use with next
		 * read or write to the char device.
		 */
		if (sc->sc_format_pid != p->p_pid)
			return (EPERM);
		if (legal_cmds[((struct scsi_fmt_cdb *)data)->cdb[0]] == 0)
			return (EINVAL);
		bcopy(data, (caddr_t)&sc->sc_cdb, sizeof(sc->sc_cdb));
		return (0);

	case SDIOCSENSE:
		/*
		 * return the SCSI sense data saved after the last
		 * operation that completed with "check condition" status.
		 */
		bcopy((caddr_t)&sc->sc_sense, data, sizeof(sc->sc_sense));
		return (0);

	case DIOCGDINFO:
		/* get the current disk label */
		*(struct disklabel *)data = *(sc->sc_label);
		return (0);

	case DIOCSDINFO:
		/* set the current disk label */
		if (!(flag & FWRITE))
			return (EBADF);
		error = setdisklabel(sc->sc_label,
				     (struct disklabel *)data,
				     (sc->sc_flags & RZF_WLABEL) ? 0 :
				     sc->sc_openpart, &cd);
		return (error);

	case DIOCGPART:
		/* return the disk partition data */
		((struct partinfo *)data)->disklab = sc->sc_label;
		((struct partinfo *)data)->part =
			&sc->sc_label->d_partitions[rzpart(dev)];
		return (0);

	case DIOCWLABEL:
		if (!(flag & FWRITE))
			return (EBADF);
		if (*(int *)data)
			sc->sc_flags |= RZF_WLABEL;
		else
			sc->sc_flags &= ~RZF_WLABEL;
		return (0);

	case DIOCWDINFO:
		/* write the disk label to disk */
		if (!(flag & FWRITE))
			return (EBADF);
		error = setdisklabel(sc->sc_label,
				     (struct disklabel *)data,
				     (sc->sc_flags & RZF_WLABEL) ? 0 :
				     sc->sc_openpart,
				     &cd);
		if (error)
			return (error);

		/* simulate opening partition 0 so write succeeds */
		flags = sc->sc_flags;
		sc->sc_flags = RZF_ALIVE | RZF_WLABEL;
		error = writedisklabel(dev, rzstrategy, sc->sc_label, &cd);
		sc->sc_flags = flags;
		return (error);
	}
	/*NOTREACHED*/
}

int
rzsize(dev)
	dev_t dev;
{
	register int unit = rzunit(dev);
	register int part = rzpart(dev);
	register struct rz_softc *sc = &rz_softc[unit];

	if (unit >= NRZ || !(sc->sc_flags & RZF_ALIVE))
		return (-1);

	/*
	 * We get called very early on (via swapconf)
	 * without the device being open so we need to
	 * read the disklabel here.
	 */
	if (!(sc->sc_flags & RZF_HAVELABEL))
		rzgetinfo(dev);

	if (part >= sc->sc_label->d_npartitions)
		return (-1);
	return (sc->sc_label->d_partitions[part].p_size);
}

/*
 * Non-interrupt driven, non-dma dump routine.
 * XXX 
 *  Still an old-style dump function:  arguments after "dev" are ignored.
 */
int
rzdump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	int part = rzpart(dev);
	int unit = rzunit(dev);
	register struct rz_softc *sc = &rz_softc[unit];
	register daddr_t baddr;
	register int maddr;
	register int pages, i;
	extern int lowram;
#ifdef later
	register struct pmax_scsi_device *sd = sc->sc_sd;
	int stat;
#endif

	/*
	 * Hmm... all vax drivers dump maxfree pages which is physmem minus
	 * the message buffer.  Is there a reason for not dumping the
	 * message buffer?  Savecore expects to read 'dumpsize' pages of
	 * dump, where dumpsys() sets dumpsize to physmem!
	 */
	pages = physmem;

	/* is drive ok? */
	if (unit >= NRZ || (sc->sc_flags & RZF_ALIVE) == 0)
		return (ENXIO);
	/* dump parameters in range? */
	if (dumplo < 0 || dumplo >= sc->sc_label->d_partitions[part].p_size)
		return (EINVAL);
	if (dumplo + ctod(pages) > sc->sc_label->d_partitions[part].p_size)
		pages = dtoc(sc->sc_label->d_partitions[part].p_size - dumplo);
	maddr = lowram;
	baddr = dumplo + sc->sc_label->d_partitions[part].p_offset;

#ifdef notdef	/*XXX -- bogus code, from Mach perhaps? */
	/* scsi bus idle? */
	if (!scsireq(&sc->sc_dq)) {
		scsireset(sd->sd_ctlr);
		sc->sc_stats.rzresets++;
		printf("[ drive %d reset ] ", unit);
	}
#else
	if (!rzready(sc)) {
		printf("[ drive %d did not reset ] ", unit);
		return(ENXIO);
	}
#endif
	printf("[..untested..] dumping %d pages\n", pages);


	for (i = 0; i < pages; i++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many Mbs we have dumped */
		if (i && (i % NPGMB) == 0)
			printf("%d ", i / NPGMB);
#undef NPBMG
#ifdef later
	        /*XXX*/
		/*mapin(mmap, (u_int)vmmap, btop(maddr), PG_URKR|PG_CI|PG_V);*/
		pmap_enter(pmap_kernel(), (vm_offset_t)vmmap, maddr,
		   VM_PROT_READ, TRUE);

		stat = scsi_tt_write(sd->sd_ctlr, sd->sd_drive, sd->sd_slave,
				     vmmap, NBPG, baddr, sc->sc_bshift);
		if (stat) {
			printf("rzdump: scsi write error 0x%x\n", stat);
			return (EIO);
		}
#endif

		maddr += NBPG;
		baddr += ctod(1);
	}
	return (0);
}
#endif
