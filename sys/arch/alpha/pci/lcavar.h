/*	$OpenBSD: lcavar.h,v 1.6 1997/01/24 19:57:46 niklas Exp $	*/
/*	$NetBSD: lcavar.h,v 1.5 1996/11/25 03:49:38 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>

/*
 * LCA chipset's configuration.
 *
 * All of the information that the chipset-specific functions need to
 * do their dirty work (and more!).
 */
struct lca_config {
	int	lc_initted;

	bus_space_tag_t lc_iot, lc_memt;
	struct alpha_pci_chipset lc_pc;

	bus_addr_t lc_s_mem_w2_masked_base;

	struct extent *lc_io_ex, *lc_d_mem_ex, *lc_s_mem_ex;
	int	lc_mallocsafe;
};

struct lca_softc {
	struct	device sc_dev;

	struct	lca_config *sc_lcp;
};

void	lca_init __P((struct lca_config *, int));
void	lca_pci_init __P((pci_chipset_tag_t, void *));

bus_space_tag_t lca_bus_io_init __P((void *));
bus_space_tag_t lca_bus_mem_init __P((void *));
