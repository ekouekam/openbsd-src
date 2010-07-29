/*	$OpenBSD: acpi_machdep.c,v 1.37 2010/07/29 00:29:49 mlarkin Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/conf.h>
#include <machine/acpiapm.h>
#include <i386/isa/isa_machdep.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuvar.h>
#include <machine/npx.h>

#include <dev/isa/isareg.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>

#include "apm.h"
#include "isa.h"
#include "ioapic.h"
#include "lapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#endif

#if NLAPIC > 0
#include <machine/apicvar.h>
#include <machine/i82489reg.h>
#include <machine/i82489var.h>
#endif

#if NAPM > 0
int haveacpibutusingapm;
#endif

extern u_char acpi_real_mode_resume[], acpi_resume_end[];

extern int acpi_savecpu(void) __returns_twice;
extern void intr_calculatemasks(void);

#define ACPI_BIOS_RSDP_WINDOW_BASE        0xe0000
#define ACPI_BIOS_RSDP_WINDOW_SIZE        0x20000

u_int8_t	*acpi_scan(struct acpi_mem_map *, paddr_t, size_t);

int
acpi_map(paddr_t pa, size_t len, struct acpi_mem_map *handle)
{
	paddr_t pgpa = trunc_page(pa);
	paddr_t endpa = round_page(pa + len);
	vaddr_t va = uvm_km_valloc(kernel_map, endpa - pgpa);

	if (va == 0)
		return (ENOMEM);

	handle->baseva = va;
	handle->va = (u_int8_t *)(va + (u_long)(pa & PGOFSET));
	handle->vsize = endpa - pgpa;
	handle->pa = pa;

	do {
		pmap_kenter_pa(va, pgpa, VM_PROT_READ | VM_PROT_WRITE);
		va += NBPG;
		pgpa += NBPG;
	} while (pgpa < endpa);

	return 0;
}

void
acpi_unmap(struct acpi_mem_map *handle)
{
	pmap_kremove(handle->baseva, handle->vsize);
	uvm_km_free(kernel_map, handle->baseva, handle->vsize);
}

u_int8_t *
acpi_scan(struct acpi_mem_map *handle, paddr_t pa, size_t len)
{
	size_t i;
	u_int8_t *ptr;
	struct acpi_rsdp1 *rsdp;

	if (acpi_map(pa, len, handle))
		return (NULL);
	for (ptr = handle->va, i = 0;
	     i < len;
	     ptr += 16, i += 16)
		if (memcmp(ptr, RSDP_SIG, sizeof(RSDP_SIG) - 1) == 0) {
			rsdp = (struct acpi_rsdp1 *)ptr;
			/*
			 * Only checksum whichever portion of the
			 * RSDP that is actually present
			 */
			if (rsdp->revision == 0 &&
			    acpi_checksum(ptr, sizeof(struct acpi_rsdp1)) == 0)
				return (ptr);
			else if (rsdp->revision >= 2 && rsdp->revision <= 3 &&
			    acpi_checksum(ptr, sizeof(struct acpi_rsdp)) == 0)
				return (ptr);
		}
	acpi_unmap(handle);

	return (NULL);
}

int
acpi_probe(struct device *parent, struct cfdata *match,
    struct bios_attach_args *ba)
{
	struct acpi_mem_map handle;
	u_int8_t *ptr;
	paddr_t ebda;
#if NAPM > 0
	extern int apm_attached;
#endif

	/*
	 * First try to find ACPI table entries in the EBDA
	 */
	if (acpi_map(0, NBPG, &handle))
		printf("acpi: failed to map BIOS data area\n");
	else {
		ebda = *(const u_int16_t *)(&handle.va[0x40e]);
		ebda <<= 4;
		acpi_unmap(&handle);

		if (ebda && ebda < IOM_BEGIN) {
			if ((ptr = acpi_scan(&handle, ebda, 1024)))
				goto havebase;
		}
	}

	/*
	 * Next try to find the ACPI table entries in the
	 * BIOS memory
	 */
	if ((ptr = acpi_scan(&handle, ACPI_BIOS_RSDP_WINDOW_BASE,
	    ACPI_BIOS_RSDP_WINDOW_SIZE)))
		goto havebase;

	return (0);

havebase:
	ba->ba_acpipbase = ptr - handle.va + handle.pa;
	acpi_unmap(&handle);
#if NAPM > 0
	if (apm_attached) {
		haveacpibutusingapm = 1;
		return (0);
	}
#endif

	return (1);
}

#ifndef SMALL_KERNEL

void
acpi_attach_machdep(struct acpi_softc *sc)
{
	extern void (*cpuresetfn)(void);

	sc->sc_interrupt = isa_intr_establish(NULL, sc->sc_fadt->sci_int,
	    IST_LEVEL, IPL_TTY, acpi_interrupt, sc, sc->sc_dev.dv_xname);
	acpiapm_open = acpiopen;
	acpiapm_close = acpiclose;
	acpiapm_ioctl = acpiioctl;
	acpiapm_kqfilter = acpikqfilter;
	cpuresetfn = acpi_reset;

	/*
	 * Sanity check before setting up trampoline.
	 * Ensure the trampoline size is < PAGE_SIZE
	 */
	KASSERT(acpi_resume_end - acpi_real_mode_resume < PAGE_SIZE);

	bcopy(acpi_real_mode_resume, (caddr_t)ACPI_TRAMPOLINE,
	    acpi_resume_end - acpi_real_mode_resume);
}

void
acpi_cpu_flush(struct acpi_softc *sc, int state)
{
	/*
	 * Flush write back caches since we'll lose them.
	 */
	if (state > ACPI_STATE_S1)
		wbinvd();
}

int
acpi_sleep_machdep(struct acpi_softc *sc, int state)
{
	if (sc->sc_facs == NULL) {
		printf("%s: acpi_sleep_machdep: no FACS\n", DEVNAME(sc));
		return (ENXIO);
	}

	/* i386 does lazy pmap_activate */
	pmap_activate(curproc);

	/*
	 *
	 * ACPI defines two wakeup vectors. One is used for ACPI 1.0
	 * implementations - it's in the FACS table as wakeup_vector and
	 * indicates a 32-bit physical address containing real-mode wakeup
	 * code.
	 *
	 * The second wakeup vector is in the FACS table as
	 * x_wakeup_vector and indicates a 64-bit physical address
	 * containing protected-mode wakeup code.
	 *
	 */
	sc->sc_facs->wakeup_vector = (u_int32_t)ACPI_TRAMPOLINE;
	if (sc->sc_facs->version == 1)
		sc->sc_facs->x_wakeup_vector = 0;

	/* Copy the current cpu registers into a safe place for resume.
	 * acpi_savecpu actually returns twice - once in the suspend
	 * path and once in the resume path (see setjmp(3)).
	 */
	if (acpi_savecpu()) {
		/* Suspend path */
		npxsave_cpu(curcpu(), 1);
#ifdef MULTIPROCESSOR
		i386_broadcast_ipi(I386_IPI_SYNCH_FPU);
		i386_broadcast_ipi(I386_IPI_HALT);
#endif
		wbinvd();
		acpi_enter_sleep_state(sc, state);
		panic("%s: acpi_enter_sleep_state failed", DEVNAME(sc));
	}

	/* Resume path continues here */

	/* Reset the vector */
	sc->sc_facs->wakeup_vector = 0;

#if NISA > 0
	isa_defaultirq();
#endif
	intr_calculatemasks();

#if NLAPIC > 0
	lapic_enable();
	if (initclock_func == lapic_initclocks)
		lapic_startclock();
	lapic_set_lvt();
#endif

	npxinit(&cpu_info_primary);

	/* Re-initialise memory range handling */
	if (mem_range_softc.mr_op != NULL)
		mem_range_softc.mr_op->initAP(&mem_range_softc);

#if NIOAPIC > 0
	ioapic_enable();
#endif
	i8254_startclock();
	if (initclock_func == i8254_initclocks)
		rtcstart();		/* in i8254 mode, rtc is profclock */
	inittodr(time_second);

	return (0);
}

void
acpi_resume_machdep(void)
{
#ifdef MULTIPROCESSOR
	struct cpu_info *ci;
	struct proc *p;
	struct pcb *pcb;
	struct trapframe *tf;
	struct switchframe *sf;
	int i;

	/* XXX refactor with matching code in cpu.c */

	for (i = 0; i < MAXCPUS; i++) {
		ci = cpu_info[i];
		if (ci == NULL)
			continue;
		if (ci->ci_idle_pcb == NULL)
			continue;
		if (ci->ci_flags & (CPUF_BSP|CPUF_SP|CPUF_PRIMARY))
			continue;
		KASSERT((ci->ci_flags & CPUF_RUNNING) == 0);

		p = ci->ci_schedstate.spc_idleproc;
		pcb = &p->p_addr->u_pcb;

		tf = (struct trapframe *)pcb->pcb_tss.tss_esp0 - 1;
		sf = (struct switchframe *)tf - 1;
		sf->sf_esi = (int)sched_idle;
		sf->sf_ebx = (int)ci;
		sf->sf_eip = (int)proc_trampoline;
		pcb->pcb_esp = (int)sf;

		ci->ci_idepth = 0;
	}

	cpu_boot_secondary_processors();
#endif /* MULTIPROCESSOR */
}
#endif /* ! SMALL_KERNEL */
