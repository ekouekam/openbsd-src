/*	$OpenBSD: hack.mfndpos.h,v 1.2 2001/01/28 23:41:44 niklas Exp $*/

/*
 * Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985.
 *
 *	$NetBSD: hack.mfndpos.h,v 1.3 1995/03/23 08:30:41 cgd Exp $
 */

#define	ALLOW_TRAPS	0777
#define	ALLOW_U		01000
#define	ALLOW_M		02000
#define	ALLOW_TM	04000
#define	ALLOW_ALL	(ALLOW_U | ALLOW_M | ALLOW_TM | ALLOW_TRAPS)
#define	ALLOW_SSM	010000
#define	ALLOW_ROCK	020000
#define	NOTONL		040000
#define	NOGARLIC	0100000
