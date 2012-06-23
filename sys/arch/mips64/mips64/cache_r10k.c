/*	$OpenBSD: cache_r10k.c,v 1.1 2012/06/23 21:56:06 miod Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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
 */

/*
 * Cache handling code for R10000 processor family (R10000, R12000,
 * R14000, R16000).
 *
 * R10000 caches are :
 * - L1 I$ is 2-way, VIPT, 64 bytes/line, 32KB total
 * - L1 D$ is 2-way, VIPT, write-back, 32 bytes/line, 32KB total
 * - L2 is 2-way, PIPT, write-back, 64 or 128 bytes/line, 512KB up to 16MB
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <mips64/cache.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

#define	IndexInvalidate_I	0x00
#define	IndexWBInvalidate_D	0x01
#define	IndexWBInvalidate_S	0x03

#define	HitInvalidate_D		0x11
#define	HitInvalidate_S		0x13

#define	CacheBarrier		0x14

#define	HitWBInvalidate_D	0x15
#define	HitWBInvalidate_S	0x17

#define	cache(op,set,addr) \
    __asm__ __volatile__ \
      ("cache %0, %1(%2)" :: "i"(op), "i"(set), "r"(addr) : "memory")

static __inline__ void	mips10k_hitinv_primary(vaddr_t, vsize_t);
static __inline__ void	mips10k_hitinv_secondary(vaddr_t, vsize_t);
static __inline__ void	mips10k_hitwbinv_primary(vaddr_t, vsize_t);
static __inline__ void	mips10k_hitwbinv_secondary(vaddr_t, vsize_t);

#define	R10K_L1I_LINE	64UL
#define	R10K_L1D_LINE	32UL

void
Mips10k_ConfigCache(struct cpu_info *ci)
{
	uint32_t cfg, valias_mask;

	cfg = cp0_get_config();

	ci->ci_l1instcacheline = R10K_L1I_LINE;
	ci->ci_l1instcachesize = (1 << 12) << ((cfg >> 29) & 0x07);	/* IC */

	ci->ci_l1datacacheline = R10K_L1D_LINE;
	ci->ci_l1datacachesize = (1 << 12) << ((cfg >> 26) & 0x07);	/* DC */

	ci->ci_l2line = 64;	/* XXX could be 128 */
	ci->ci_l2size = (1 << 19) << ((cfg >> 16) & 0x07);

	ci->ci_l3size = 0;

	ci->ci_cacheways = 2;
	ci->ci_l1instcacheset = ci->ci_l1instcachesize / 2;
	ci->ci_l1datacacheset = ci->ci_l1datacachesize / 2;

	valias_mask = (max(ci->ci_l1instcacheset, ci->ci_l1datacacheset) - 1) &
	    ~PAGE_MASK;

	if (valias_mask != 0) {
		valias_mask |= PAGE_MASK;
#ifdef MULTIPROCESSOR
		if (valias_mask > cache_valias_mask) {
#endif
			cache_valias_mask = valias_mask;
			pmap_prefer_mask = valias_mask;
#ifdef MULTIPROCESSOR
		}
#endif
	}
}

static __inline__ void
mips10k_hitwbinv_primary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_D, 0, va);
		va += R10K_L1D_LINE;
	}
}

static __inline__ void
mips10k_hitwbinv_secondary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitWBInvalidate_S, 0, va);
		va += 64UL;
	}
}

static __inline__ void
mips10k_hitinv_primary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_D, 0, va);
		va += R10K_L1D_LINE;
	}
}

static __inline__ void
mips10k_hitinv_secondary(vaddr_t va, vsize_t sz)
{
	vaddr_t eva;

	eva = va + sz;
	while (va != eva) {
		cache(HitInvalidate_S, 0, va);
		va += 64UL;
	}
}

/*
 * Writeback and invalidate all caches.
 */
void
Mips10k_SyncCache(struct cpu_info *ci)
{
	vaddr_t sva, eva;

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1instcacheset;
	while (sva != eva) {
		cache(IndexInvalidate_I, 0, sva);
		cache(IndexInvalidate_I, 1, sva);
		sva += R10K_L1I_LINE;
	}

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l1datacacheset;
	while (sva != eva) {
		cache(IndexWBInvalidate_D, 0, sva);
		cache(IndexWBInvalidate_D, 1, sva);
		sva += R10K_L1D_LINE;
	}

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	eva = sva + ci->ci_l2size / 2;
	while (sva != eva) {
		cache(IndexWBInvalidate_S, 0, sva);
		cache(IndexWBInvalidate_S, 1, sva);
		sva += 64UL;
	}
}

/*
 * Invalidate I$ for the given range.
 */
void
Mips10k_InvalidateICache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va, sva, eva;
	vsize_t sz;

	/* extend the range to integral cache lines */
	va = _va & ~(R10K_L1I_LINE - 1);
	sz = ((_va + _sz + R10K_L1I_LINE - 1) & ~(R10K_L1I_LINE - 1)) - va;

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	/* keep only the index bits */
	sva |= va & ((1UL << 14) - 1);
	eva = sva + sz;
	while (sva != eva) {
		cache(IndexInvalidate_I, 0, sva);
		cache(IndexInvalidate_I, 1, sva);
		sva += R10K_L1I_LINE;
	}
}

/*
 * Writeback D$ for the given page.
 */
void
Mips10k_SyncDCachePage(struct cpu_info *ci, vaddr_t va, paddr_t pa)
{
	vaddr_t sva, eva;

	sva = PHYS_TO_XKPHYS(0, CCA_CACHED);
	/* keep only the index bits */
	sva += va & ((1UL << 14) - 1);
	eva = sva + PAGE_SIZE;
	while (sva != eva) {
		cache(IndexWBInvalidate_D, 0 + 0 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 0 + 1 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 0 + 2 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 0 + 3 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 1 + 0 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 1 + 1 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 1 + 2 * R10K_L1D_LINE, sva);
		cache(IndexWBInvalidate_D, 1 + 3 * R10K_L1D_LINE, sva);
		sva += 4 * R10K_L1D_LINE;
	}
}

/*
 * Writeback D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

void
Mips10k_HitSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	/* extend the range to integral cache lines */
	va = _va & ~(R10K_L1D_LINE - 1);
	sz = ((_va + _sz + R10K_L1D_LINE - 1) & ~(R10K_L1D_LINE - 1)) - va;
	mips10k_hitwbinv_primary(va, sz);
}

/*
 * Invalidate D$ for the given range. Range is expected to be currently
 * mapped, allowing the use of `Hit' operations. This is less aggressive
 * than using `Index' operations.
 */

void
Mips10k_HitInvalidateDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz)
{
	vaddr_t va;
	vsize_t sz;

	/* extend the range to integral cache lines */
	va = _va & ~(R10K_L1D_LINE - 1);
	sz = ((_va + _sz + R10K_L1D_LINE - 1) & ~(R10K_L1D_LINE - 1)) - va;
	mips10k_hitinv_primary(va, sz);
}

/*
 * Backend for bus_dmamap_sync(). Enforce coherency of the given range
 * by performing the necessary cache writeback and/or invalidate
 * operations.
 */
void
Mips10k_IOSyncDCache(struct cpu_info *ci, vaddr_t _va, size_t _sz, int how)
{
	vaddr_t va;
	vsize_t sz;
	int partial_start, partial_end;

	/* extend the range to integral L2 cache lines */
	va = _va & ~(64UL - 1);
	sz = ((_va + _sz + 64UL - 1) & ~(64UL - 1)) - va;

	switch (how) {
	case CACHE_SYNC_R:
		/* writeback partial cachelines */
		if (((_va | _sz) & (64UL - 1)) != 0) {
			partial_start = va != _va;
			partial_end = va + sz != _va + _sz;
		} else {
			partial_start = partial_end = 0;
		}
		if (partial_start) {
			cache(HitWBInvalidate_S, 0, va);
			va += 64UL;
			sz -= 64UL;
		}
		if (sz != 0 && partial_end) {
			cache(HitWBInvalidate_S, 0, va + sz - 64UL);
			sz -= 64UL;
		}
		mips10k_hitinv_secondary(va, sz);
		break;
	case CACHE_SYNC_X:
	case CACHE_SYNC_W:
		mips10k_hitwbinv_secondary(va, sz);
	}
}
