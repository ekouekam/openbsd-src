/*	$OpenBSD: pmap.h,v 1.14 2001/12/02 02:01:52 millert Exp $	*/

#ifndef	_MAC68K_PMAP_H_
#define	_MAC68K_PMAP_H_

#include <m68k/pmap_motorola.h>

#ifdef	_KERNEL

void mac68k_set_pte __P((vaddr_t va, paddr_t pge));

void pmap_init_md __P((void));
#define	PMAP_INIT_MD()	pmap_init_md()

#endif	/* _KERNEL */

#endif	/* _MAC68K_PMAP_H_ */
