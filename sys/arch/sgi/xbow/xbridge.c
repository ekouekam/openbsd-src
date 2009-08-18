/*	$OpenBSD: xbridge.c,v 1.45 2009/08/18 19:31:59 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009  Miodrag Vallat.
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
 * XBow Bridge (as well as XBridge and PIC) Widget driver.
 */

/*
 * IMPORTANT AUTHOR'S NOTE: I did not write any of this code under the
 * influence of drugs.  Looking back at that particular piece of hardware,
 * I wonder if this hasn't been a terrible mistake.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/mnode.h>

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/cardbus/rbus.h>

#include <mips64/archtype.h>
#include <sgi/xbow/hub.h>
#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xbowdevs.h>

#include <sgi/xbow/xbridgereg.h>

#include <sgi/sgi/ip30.h>

#include "cardbus.h"

int	xbridge_match(struct device *, void *, void *);
void	xbridge_attach(struct device *, struct device *, void *);
int	xbridge_print(void *, const char *);

struct xbridge_ate;
struct xbridge_intr;
struct xbridge_softc;

struct xbridge_bus {
	struct xbridge_softc *xb_sc;

	/*
	 * Bridge register accessors.
	 * Due to hardware bugs, PIC registers can only be accessed
	 * with 64 bit operations, although the hardware was supposed
	 * to be directly compatible with XBridge on that aspect.
	 */
	uint32_t	(*xb_read_reg)(bus_space_tag_t, bus_space_handle_t,
			    bus_addr_t);
	void		(*xb_write_reg)(bus_space_tag_t, bus_space_handle_t,
			    bus_addr_t, uint32_t);

	uint		xb_busno;
	uint		xb_nslots;

	int		xb_flags;	/* copy of xbridge_softc value */
	int16_t		xb_nasid;	/* copy of xbridge_softc value */
	int		xb_widget;	/* copy of xbridge_softc value */
	uint		xb_devio_skew;	/* copy of xbridge_softc value */

	struct mips_pci_chipset xb_pc;

	bus_space_tag_t xb_regt;
	bus_space_handle_t xb_regh;

	struct mips_bus_space *xb_mem_bus_space;
	struct mips_bus_space *xb_io_bus_space;
	struct machine_bus_dma_tag *xb_dmat;

	struct xbridge_intr	*xb_intr[BRIDGE_NINTRS];

	/*
	 * Device information.
	 */
	struct {
		pcireg_t	id;
		uint32_t	devio;
	} xb_devices[MAX_SLOTS];

	/*
	 * ATE management.
	 */
	struct mutex	xb_atemtx;
	uint		xb_atecnt;
	struct xbridge_ate	*xb_ate;
	LIST_HEAD(, xbridge_ate) xb_free_ate;
	LIST_HEAD(, xbridge_ate) xb_used_ate;

	/*
	 * Large resource view sizes
	 */
	bus_addr_t	xb_iostart, xb_ioend;
	bus_addr_t	xb_memstart, xb_memend;

	/*
	 * Resource extents for the large resource views, used during
	 * resource setup, then cleaned up for the MI code.
	 */
	char		xb_ioexname[32];
	struct extent	*xb_ioex;
	char		xb_memexname[32];
	struct extent	*xb_memex;
};

struct xbridge_softc {
	struct device	sc_dev;
	int		sc_flags;
#define	XF_XBRIDGE		0x01	/* is either PIC or XBridge */
#define	XF_PIC			0x02	/* is PIC */
#define	XF_NO_DIRECT_IO		0x04	/* no direct I/O mapping */
	int16_t		sc_nasid;
	int		sc_widget;
	uint		sc_devio_skew;
	uint		sc_nbuses;

	struct mips_bus_space	sc_regt;
	struct xbridge_bus	sc_bus[MAX_BUSES];
};

#define	DEVNAME(xb)	((xb)->xb_sc->sc_dev.dv_xname)

const struct cfattach xbridge_ca = {
	sizeof(struct xbridge_softc), xbridge_match, xbridge_attach,
};

struct cfdriver xbridge_cd = {
	NULL, "xbridge", DV_DULL,
};

void	xbridge_attach_bus(struct xbridge_softc *, uint, bus_space_tag_t);

void	xbridge_attach_hook(struct device *, struct device *,
				struct pcibus_attach_args *);
int	xbridge_bus_maxdevs(void *, int);
pcitag_t xbridge_make_tag(void *, int, int, int);
void	xbridge_decompose_tag(void *, pcitag_t, int *, int *, int *);
pcireg_t xbridge_conf_read(void *, pcitag_t, int);
void	xbridge_conf_write(void *, pcitag_t, int, pcireg_t);
int	xbridge_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *xbridge_intr_string(void *, pci_intr_handle_t);
void	*xbridge_intr_establish(void *, pci_intr_handle_t, int,
	    int (*func)(void *), void *, char *);
void	xbridge_intr_disestablish(void *, void *);
int	xbridge_intr_line(void *, pci_intr_handle_t);
int	xbridge_ppb_setup(void *, pcitag_t, bus_addr_t *, bus_addr_t *,
	    bus_addr_t *, bus_addr_t *);
void	*xbridge_rbus_parent_io(struct pci_attach_args *);
void	*xbridge_rbus_parent_mem(struct pci_attach_args *);

int	xbridge_intr_handler(void *);

uint8_t xbridge_read_1(bus_space_tag_t, bus_space_handle_t, bus_size_t);
uint16_t xbridge_read_2(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void	xbridge_write_1(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint8_t);
void	xbridge_write_2(bus_space_tag_t, bus_space_handle_t, bus_size_t,
	    uint16_t);
void	xbridge_read_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_2(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	xbridge_read_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_4(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);
void	xbridge_read_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint8_t *, bus_size_t);
void	xbridge_write_raw_8(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    const uint8_t *, bus_size_t);

int	xbridge_space_map_devio(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	xbridge_space_map_io(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	xbridge_space_map_mem(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
int	xbridge_space_region_devio(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, bus_space_handle_t *);
int	xbridge_space_region_io(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, bus_space_handle_t *);
int	xbridge_space_region_mem(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_size_t, bus_space_handle_t *);

int	xbridge_dmamap_load_buffer(bus_dma_tag_t, bus_dmamap_t, void *,
	    bus_size_t, struct proc *, int, paddr_t *, int *, int);
void	xbridge_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
int	xbridge_dmamem_alloc(bus_dma_tag_t, bus_size_t, bus_size_t, bus_size_t,
	    bus_dma_segment_t *, int, int *, int);
bus_addr_t xbridge_pa_to_device(paddr_t);
paddr_t	xbridge_device_to_pa(bus_addr_t);

int	xbridge_rbus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t,
	    int, bus_space_handle_t *);
void	xbridge_rbus_space_unmap(bus_space_tag_t, bus_space_handle_t,
	    bus_size_t, bus_addr_t *);

int	xbridge_address_map(struct xbridge_bus *, paddr_t, bus_addr_t *,
	    bus_addr_t *);
void	xbridge_address_unmap(struct xbridge_bus *, bus_addr_t, bus_size_t);
uint	xbridge_ate_add(struct xbridge_bus *, paddr_t);
void	xbridge_ate_dump(struct xbridge_bus *);
uint	xbridge_ate_find(struct xbridge_bus *, paddr_t);
uint64_t xbridge_ate_read(struct xbridge_bus *, uint);
void	xbridge_ate_unref(struct xbridge_bus *, uint, uint);
void	xbridge_ate_write(struct xbridge_bus *, uint, uint64_t);

int	xbridge_allocate_devio(struct xbridge_bus *, int, int);
void	xbridge_set_devio(struct xbridge_bus *, int, uint32_t);

int	xbridge_resource_explore(struct xbridge_bus *, pcitag_t,
	    struct extent *, struct extent *);
void	xbridge_resource_manage(struct xbridge_bus *, pcitag_t,
	    struct extent *, struct extent *);

void	xbridge_ate_setup(struct xbridge_bus *);
void	xbridge_device_setup(struct xbridge_bus *, int, int, uint32_t);
void	xbridge_extent_setup(struct xbridge_bus *);
struct extent *
	xbridge_mapping_setup(struct xbridge_bus *, int);
void	xbridge_resource_setup(struct xbridge_bus *);
void	xbridge_rrb_setup(struct xbridge_bus *, int);
void	xbridge_setup(struct xbridge_bus *);

uint32_t bridge_read_reg(bus_space_tag_t, bus_space_handle_t, bus_addr_t);
void	bridge_write_reg(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint32_t);
uint32_t pic_read_reg(bus_space_tag_t, bus_space_handle_t, bus_addr_t);
void	pic_write_reg(bus_space_tag_t, bus_space_handle_t, bus_addr_t,
	    uint32_t);

static __inline__ uint32_t
xbridge_read_reg(struct xbridge_bus *xb, bus_addr_t a)
{
	return (*xb->xb_read_reg)(xb->xb_regt, xb->xb_regh, a);
}
static __inline__ void
xbridge_write_reg(struct xbridge_bus *xb, bus_addr_t a, uint32_t v)
{
	(*xb->xb_write_reg)(xb->xb_regt, xb->xb_regh, a, v);
}

const struct machine_bus_dma_tag xbridge_dma_tag = {
	NULL,			/* _cookie */
	_dmamap_create,
	_dmamap_destroy,
	_dmamap_load,
	_dmamap_load_mbuf,
	_dmamap_load_uio,
	_dmamap_load_raw,
	xbridge_dmamap_load_buffer,
	xbridge_dmamap_unload,
	_dmamap_sync,
	xbridge_dmamem_alloc,
	_dmamem_free,
	_dmamem_map,
	_dmamem_unmap,
	_dmamem_mmap,
	xbridge_pa_to_device,
	xbridge_device_to_pa,
	BRIDGE_DMA_DIRECT_LENGTH - 1
};

/*
 ********************* Autoconf glue.
 */

static const struct {
	uint32_t vendor;
	uint32_t product;
	int	flags;
} xbridge_devices[] = {
	/* original Bridge */
	{ XBOW_VENDOR_SGI4, XBOW_PRODUCT_SGI4_BRIDGE,	0 },
	/* XBridge */
	{ XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_XBRIDGE,	XF_XBRIDGE },
	/* PIC first half */
	{ XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_PIC0,	XF_PIC },
	/* PIC second half */
	{ XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_PIC1,	XF_PIC }
};

int
xbridge_match(struct device *parent, void *match, void *aux)
{
	struct xbow_attach_args *xaa = aux;
	uint i;

	for (i = 0; i < nitems(xbridge_devices); i++)
		if (xaa->xaa_vendor == xbridge_devices[i].vendor &&
		    xaa->xaa_product == xbridge_devices[i].product)
			return 1;

	return 0;
}

void
xbridge_attach(struct device *parent, struct device *self, void *aux)
{
	struct xbridge_softc *sc = (struct xbridge_softc *)self;
	struct xbow_attach_args *xaa = aux;
	uint i;

	sc->sc_nasid = xaa->xaa_nasid;
	sc->sc_widget = xaa->xaa_widget;

	printf(" revision %d\n", xaa->xaa_revision);

	for (i = 0; i < nitems(xbridge_devices); i++)
		if (xaa->xaa_vendor == xbridge_devices[i].vendor &&
		    xaa->xaa_product == xbridge_devices[i].product) {
			sc->sc_flags = xbridge_devices[i].flags;
			break;
		}

	/* PICs are XBridges without an I/O window */
	if (ISSET(sc->sc_flags, XF_PIC))
		SET(sc->sc_flags, XF_XBRIDGE | XF_NO_DIRECT_IO);
	/* Bridge < D lacks an I/O window */
	if (!ISSET(sc->sc_flags, XF_XBRIDGE) && xaa->xaa_revision < 4)
		SET(sc->sc_flags, XF_NO_DIRECT_IO);

	/*
	 * Figure out where the devio mappings will go.
	 * On Octane, they are relative to the start of the widget.
	 * On Origin, they are computed from the widget number.
	 */
	if (sys_config.system_type == SGI_OCTANE)
		sc->sc_devio_skew = 0;
	else
		sc->sc_devio_skew = sc->sc_widget;

	sc->sc_nbuses =
	    ISSET(sc->sc_flags, XF_PIC) ? PIC_NBUSES : BRIDGE_NBUSES;

	/* make a permanent copy of the on-stack bus_space_tag */
	bcopy(xaa->xaa_iot, &sc->sc_regt, sizeof(struct mips_bus_space));

	/* configure and attach PCI buses */
	for (i = 0; i < sc->sc_nbuses; i++)
		xbridge_attach_bus(sc, i, &sc->sc_regt);
}

void
xbridge_attach_bus(struct xbridge_softc *sc, uint busno, bus_space_tag_t regt)
{
	struct xbridge_bus *xb = &sc->sc_bus[busno];
	struct pcibus_attach_args pba;

	xb->xb_sc = sc;
	xb->xb_busno = busno;
	xb->xb_flags = sc->sc_flags;
	xb->xb_nasid = sc->sc_nasid;
	xb->xb_widget = sc->sc_widget;
	xb->xb_devio_skew = sc->sc_devio_skew;

	if (ISSET(sc->sc_flags, XF_PIC)) {
		xb->xb_nslots = PIC_NSLOTS;

		xb->xb_read_reg = pic_read_reg;
		xb->xb_write_reg = pic_write_reg;
	} else {
		xb->xb_nslots = BRIDGE_NSLOTS;

		xb->xb_read_reg = bridge_read_reg;
		xb->xb_write_reg = bridge_write_reg;
	}

	/*
	 * Map Bridge registers.
	 */

	xb->xb_regt = regt;
	if (bus_space_map(regt, busno != 0 ? BRIDGE_BUS_OFFSET : 0,
	    BRIDGE_REGISTERS_SIZE, 0, &xb->xb_regh)) {
		printf("%s: unable to map control registers for bus %u\n",
		    DEVNAME(xb), busno);
		return;
	}

	/*
	 * Create bus_space accessors... we inherit them from xbow, but
	 * it is necessary to perform endianness conversion for the
	 * low-order address bits.
	 */

	xb->xb_mem_bus_space = malloc(sizeof (*xb->xb_mem_bus_space),
	    M_DEVBUF, M_NOWAIT);
	if (xb->xb_mem_bus_space == NULL)
		goto fail1;
	xb->xb_io_bus_space = malloc(sizeof (*xb->xb_io_bus_space),
	    M_DEVBUF, M_NOWAIT);
	if (xb->xb_io_bus_space == NULL)
		goto fail2;

	bcopy(regt, xb->xb_mem_bus_space, sizeof(*xb->xb_mem_bus_space));
	xb->xb_mem_bus_space->bus_private = xb;
	xb->xb_mem_bus_space->_space_map = xbridge_space_map_devio;
	xb->xb_mem_bus_space->_space_subregion = xbridge_space_region_devio;
	xb->xb_mem_bus_space->_space_read_1 = xbridge_read_1;
	xb->xb_mem_bus_space->_space_write_1 = xbridge_write_1;
	xb->xb_mem_bus_space->_space_read_2 = xbridge_read_2;
	xb->xb_mem_bus_space->_space_write_2 = xbridge_write_2;
	xb->xb_mem_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	xb->xb_mem_bus_space->_space_write_raw_2 = xbridge_write_raw_2;
	xb->xb_mem_bus_space->_space_read_raw_4 = xbridge_read_raw_4;
	xb->xb_mem_bus_space->_space_write_raw_4 = xbridge_write_raw_4;
	xb->xb_mem_bus_space->_space_read_raw_8 = xbridge_read_raw_8;
	xb->xb_mem_bus_space->_space_write_raw_8 = xbridge_write_raw_8;

	bcopy(regt, xb->xb_io_bus_space, sizeof(*xb->xb_io_bus_space));
	xb->xb_io_bus_space->bus_private = xb;
	xb->xb_io_bus_space->_space_map = xbridge_space_map_devio;
	xb->xb_io_bus_space->_space_subregion = xbridge_space_region_devio;
	xb->xb_io_bus_space->_space_read_1 = xbridge_read_1;
	xb->xb_io_bus_space->_space_write_1 = xbridge_write_1;
	xb->xb_io_bus_space->_space_read_2 = xbridge_read_2;
	xb->xb_io_bus_space->_space_write_2 = xbridge_write_2;
	xb->xb_io_bus_space->_space_read_raw_2 = xbridge_read_raw_2;
	xb->xb_io_bus_space->_space_write_raw_2 = xbridge_write_raw_2;
	xb->xb_io_bus_space->_space_read_raw_4 = xbridge_read_raw_4;
	xb->xb_io_bus_space->_space_write_raw_4 = xbridge_write_raw_4;
	xb->xb_io_bus_space->_space_read_raw_8 = xbridge_read_raw_8;
	xb->xb_io_bus_space->_space_write_raw_8 = xbridge_write_raw_8;

	xb->xb_dmat = malloc(sizeof (*xb->xb_dmat), M_DEVBUF, M_NOWAIT);
	if (xb->xb_dmat == NULL)
		goto fail3;
	memcpy(xb->xb_dmat, &xbridge_dma_tag, sizeof(*xb->xb_dmat));
	xb->xb_dmat->_cookie = xb;

	/*
	 * Initialize PCI methods.
	 */

	xb->xb_pc.pc_conf_v = xb;
	xb->xb_pc.pc_attach_hook = xbridge_attach_hook;
	xb->xb_pc.pc_make_tag = xbridge_make_tag;
	xb->xb_pc.pc_decompose_tag = xbridge_decompose_tag;
	xb->xb_pc.pc_bus_maxdevs = xbridge_bus_maxdevs;
	xb->xb_pc.pc_conf_read = xbridge_conf_read;
	xb->xb_pc.pc_conf_write = xbridge_conf_write;
	xb->xb_pc.pc_intr_v = xb;
	xb->xb_pc.pc_intr_map = xbridge_intr_map;
	xb->xb_pc.pc_intr_string = xbridge_intr_string;
	xb->xb_pc.pc_intr_establish = xbridge_intr_establish;
	xb->xb_pc.pc_intr_disestablish = xbridge_intr_disestablish;
	xb->xb_pc.pc_intr_line = xbridge_intr_line;
	xb->xb_pc.pc_ppb_setup = xbridge_ppb_setup;
#if NCARDBUS > 0
	xb->xb_pc.pc_rbus_parent_io = xbridge_rbus_parent_io;
	xb->xb_pc.pc_rbus_parent_mem = xbridge_rbus_parent_mem;
#endif

	/*
	 * Configure Bridge for proper operation (DMA, I/O mappings,
	 * RRB allocation, etc).
	 */

	xbridge_setup(xb);

	/*
	 * Attach children.
	 */

	xbridge_extent_setup(xb);

	bzero(&pba, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = xb->xb_io_bus_space;
	pba.pba_memt = xb->xb_mem_bus_space;
	pba.pba_dmat = xb->xb_dmat;
	pba.pba_ioex = xb->xb_ioex;
	pba.pba_memex = xb->xb_memex;
	pba.pba_pc = &xb->xb_pc;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = 0;

	config_found(&sc->sc_dev, &pba, xbridge_print);
	return;

fail3:
	free(xb->xb_io_bus_space, M_DEVBUF);
fail2:
	free(xb->xb_mem_bus_space, M_DEVBUF);
fail1:
	printf("%s: not enough memory to build bus %u access structures\n",
	    DEVNAME(xb), busno);
	return;
}

int
xbridge_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);

	return UNCONF;
}

/*
 ********************* PCI glue.
 */

void
xbridge_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

pcitag_t
xbridge_make_tag(void *cookie, int bus, int dev, int func)
{
	return (bus << 16) | (dev << 11) | (func << 8);
}

void
xbridge_decompose_tag(void *cookie, pcitag_t tag, int *busp, int *devp,
    int *funcp)
{
	if (busp != NULL)
		*busp = (tag >> 16) & 0xff;
	if (devp != NULL)
		*devp = (tag >> 11) & 0x1f;
	if (funcp != NULL)
		*funcp = (tag >> 8) & 0x7;
}

int
xbridge_bus_maxdevs(void *cookie, int busno)
{
	struct xbridge_bus *xb = cookie;

	return busno == 0 ? xb->xb_nslots : 32;
}

pcireg_t
xbridge_conf_read(void *cookie, pcitag_t tag, int offset)
{
	struct xbridge_bus *xb = cookie;
	pcireg_t data;
	int bus, dev, fn;
	paddr_t pa;
	int skip;
	int s;

	/* XXX should actually disable interrupts? */
	s = splhigh();

	xbridge_decompose_tag(cookie, tag, &bus, &dev, &fn);
	if (bus != 0) {
		xbridge_write_reg(xb, BRIDGE_PCI_CFG,
		    (bus << 16) | (dev << 11));
		pa = xb->xb_regh + BRIDGE_PCI_CFG1_SPACE;
	} else {
		if (ISSET(xb->xb_flags, XF_PIC)) {
			/*
			 * On PIC, device 0 in configuration space is the
			 * PIC itself, device slots are offset by one.
			 */
			pa = xb->xb_regh + BRIDGE_PCI_CFG_SPACE +
			    ((dev + 1) << 12);
		} else
			pa = xb->xb_regh + BRIDGE_PCI_CFG_SPACE + (dev << 12);
	}

	/*
	 * IOC3 devices only implement a subset of the PCI configuration
	 * registers (supposedly only the first 0x20 bytes, however
	 * writing to the second BAR also writes to the first).
	 *
	 * Depending on which particular model we encounter, things may
	 * seem to work, or write access to nonexisting registers would
	 * completely freeze the machine.
	 *
	 * We thus check for the device type here, and handle the non
	 * existing registers ourselves.
	 */

	skip = 0;
	if (bus == 0 && xb->xb_devices[dev].id ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		switch (offset) {
		case PCI_ID_REG:
		case PCI_COMMAND_STATUS_REG:
		case PCI_CLASS_REG:
		case PCI_BHLC_REG:
		case PCI_MAPREG_START:
			/* These registers are implemented. Go ahead. */
			break;
		case PCI_INTERRUPT_REG:
			/* This register is not implemented. Fake it. */
			data = (PCI_INTERRUPT_PIN_A <<
			    PCI_INTERRUPT_PIN_SHIFT) |
			    (dev << PCI_INTERRUPT_LINE_SHIFT);
			skip = 1;
			break;
		default:
			/* These registers are not implemented. */
			data = 0;
			skip = 1;
			break;
		}
	}

	if (skip == 0) {
		pa += (fn << 8) + offset;
		if (guarded_read_4(pa, &data) != 0)
			data = 0xffffffff;
	}

	splx(s);
	return(data);
}

void
xbridge_conf_write(void *cookie, pcitag_t tag, int offset, pcireg_t data)
{
	struct xbridge_bus *xb = cookie;
	int bus, dev, fn;
	paddr_t pa;
	int skip;
	int s;

	/* XXX should actually disable interrupts? */
	s = splhigh();

	xbridge_decompose_tag(cookie, tag, &bus, &dev, &fn);
	if (bus != 0) {
		xbridge_write_reg(xb, BRIDGE_PCI_CFG,
		    (bus << 16) | (dev << 11));
		pa = xb->xb_regh + BRIDGE_PCI_CFG1_SPACE;
	} else {
		if (ISSET(xb->xb_flags, XF_PIC)) {
			/*
			 * On PIC, device 0 in configuration space is the
			 * PIC itself, device slots are offset by one.
			 */
			pa = xb->xb_regh + BRIDGE_PCI_CFG_SPACE +
			    ((dev + 1) << 12);
		} else
			pa = xb->xb_regh + BRIDGE_PCI_CFG_SPACE + (dev << 12);
	}

	/*
	 * IOC3 devices only implement a subset of the PCI configuration
	 * registers.
	 * Depending on which particular model we encounter, things may
	 * seem to work, or write access to nonexisting registers would
	 * completely freeze the machine.
	 *
	 * We thus check for the device type here, and handle the non
	 * existing registers ourselves.
	 */

	skip = 0;
	if (bus == 0 && xb->xb_devices[dev].id ==
	    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
		switch (offset) {
		case PCI_COMMAND_STATUS_REG:
			/*
			 * Some IOC models do not support having this bit
			 * cleared (which is what pci_mapreg_probe() will
			 * do), so we set it unconditionnaly.
			 */
			data |= PCI_COMMAND_MEM_ENABLE;
			/* FALLTHROUGH */
		case PCI_ID_REG:
		case PCI_CLASS_REG:
		case PCI_BHLC_REG:
		case PCI_MAPREG_START:
			/* These registers are implemented. Go ahead. */
			break;
		default:
			/* These registers are not implemented. */
			skip = 1;
			break;
		}
	}

	if (skip == 0) {
		pa += (fn << 8) + offset;
		guarded_write_4(pa, data);
	}

	splx(s);
}

/*
 ********************* Interrupt handling.
 */

/*
 * We map each slot to its own interrupt bit, which will in turn be routed to
 * the Heart or Hub widget in charge of interrupt processing.
 */

struct xbridge_intrhandler {
	LIST_ENTRY(xbridge_intrhandler)	xih_nxt;
	struct xbridge_intr *xih_main;
	int	(*xih_func)(void *);
	void	*xih_arg;
	struct evcount	xih_count;
	int	 xih_level;
	int	 xih_device;	/* device slot number */
};

struct xbridge_intr {
	struct	xbridge_bus	*xi_bus;
	int	xi_intrsrc;	/* interrupt source on interrupt widget */
	int	xi_intrbit;	/* interrupt source on BRIDGE */
	LIST_HEAD(, xbridge_intrhandler) xi_handlers;
};

/* how our pci_intr_handle_t are constructed... */
#define	XBRIDGE_INTR_VALID		0x100
#define	XBRIDGE_INTR_HANDLE(d,b)	(XBRIDGE_INTR_VALID | ((d) << 3) | (b))
#define	XBRIDGE_INTR_DEVICE(h)		(((h) >> 3) & 07)
#define	XBRIDGE_INTR_BIT(h)		((h) & 07)

int
xbridge_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct xbridge_bus *xb = pa->pa_pc->pc_conf_v;
	int bus, device, intr;
	int pin;

	*ihp = 0;

	if (pa->pa_intrpin == 0) {
		/* No IRQ used. */
		return 1;
	}

#ifdef DIAGNOSTIC
	if (pa->pa_intrpin > 4) {
		printf("%s: bad interrupt pin %d\n", __func__, pa->pa_intrpin);
		return 1;
	}
#endif

	xbridge_decompose_tag(pa->pa_pc, pa->pa_tag, &bus, &device, NULL);

	if (pa->pa_bridgetag) {
		pin = PPB_INTERRUPT_SWIZZLE(pa->pa_rawintrpin, device);
		if (!ISSET(pa->pa_bridgeih[pin - 1], XBRIDGE_INTR_VALID))
			return 0;
		intr = XBRIDGE_INTR_BIT(pa->pa_bridgeih[pin - 1]);
	} else {
		/*
		 * For IOC devices, the real information is in pa_intrline.
		 */
		if (xb->xb_devices[device].id ==
		    PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3)) {
			intr = pa->pa_intrline;
		} else {
			if (pa->pa_intrpin & 1)
				intr = device;
			else
				intr = device ^ 4;
		}
	}

	*ihp = XBRIDGE_INTR_HANDLE(device, intr);

	return 0;
}

const char *
xbridge_intr_string(void *cookie, pci_intr_handle_t ih)
{
	static char str[16];

	snprintf(str, sizeof(str), "irq %d", XBRIDGE_INTR_BIT(ih));
	return(str);
}

void *
xbridge_intr_establish(void *cookie, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)cookie;
	struct xbridge_intr *xi;
	struct xbridge_intrhandler *xih;
	uint32_t int_addr;
	int intrbit = XBRIDGE_INTR_BIT(ih);
	int device = XBRIDGE_INTR_DEVICE(ih);
	int intrsrc;
	int new;

	/*
	 * Allocate bookkeeping structure if this is the
	 * first time we're using this interrupt source.
	 */
	if ((xi = xb->xb_intr[intrbit]) == NULL) {
		xi = (struct xbridge_intr *)
		    malloc(sizeof(*xi), M_DEVBUF, M_NOWAIT);
		if (xi == NULL)
			return NULL;

		xi->xi_bus = xb;
		xi->xi_intrbit = intrbit;
		LIST_INIT(&xi->xi_handlers);

		if (xbow_intr_register(xb->xb_widget, level, &intrsrc) != 0) {
			free(xi, M_DEVBUF);
			return NULL;
		}

		xi->xi_intrsrc = intrsrc;
		xb->xb_intr[intrbit] = xi;
	}
	
	/*
	 * Register the interrupt at the Heart or Hub level if this is the
	 * first time we're using this interrupt source.
	 */
	new = LIST_EMPTY(&xi->xi_handlers);
	if (new) {
		/*
		 * XXX The interrupt dispatcher is always registered
		 * XXX at IPL_BIO, in case the interrupt will be shared
		 * XXX between devices of different levels.
		 */
		if (xbow_intr_establish(xbridge_intr_handler, xi, intrsrc,
		    IPL_BIO, NULL)) {
			printf("%s: unable to register interrupt handler\n",
			    DEVNAME(xb));
			return NULL;
		}
	}

	xih = (struct xbridge_intrhandler *)
	    malloc(sizeof(*xih), M_DEVBUF, M_NOWAIT);
	if (xih == NULL)
		return NULL;

	xih->xih_main = xi;
	xih->xih_func = func;
	xih->xih_arg = arg;
	xih->xih_level = level;
	xih->xih_device = device;
	evcount_attach(&xih->xih_count, name, &xi->xi_intrbit, &evcount_intr);
	LIST_INSERT_HEAD(&xi->xi_handlers, xih, xih_nxt);

	if (new) {
		if (ISSET(xb->xb_flags, XF_PIC))
			int_addr = ((uint64_t)intrsrc << 48) |
			    (xbow_intr_widget_register & ((1UL << 48) - 1));
		else
			int_addr = ((xbow_intr_widget_register >> 30) &
			    0x0003ff00) | intrsrc;

		xbridge_write_reg(xb, BRIDGE_INT_ADDR(intrbit), int_addr);
		xbridge_write_reg(xb, BRIDGE_IER,
		    xbridge_read_reg(xb, BRIDGE_IER) | (1 << intrbit));
		/*
		 * INT_MODE register controls which interrupt pins cause
		 * ``interrupt clear'' packets to be sent for high->low
		 * transition.
		 * We enable such packets to be sent in order not to have to
		 * clear interrupts ourselves.
		 */
		xbridge_write_reg(xb, BRIDGE_INT_MODE,
		    xbridge_read_reg(xb, BRIDGE_INT_MODE) | (1 << intrbit));
		xbridge_write_reg(xb, BRIDGE_INT_DEV,
		    xbridge_read_reg(xb, BRIDGE_INT_DEV) |
		    (device << (intrbit * 3)));
		(void)xbridge_read_reg(xb, WIDGET_TFLUSH);
	}

	return (void *)xih;
}

void
xbridge_intr_disestablish(void *cookie, void *vih)
{
	struct xbridge_bus *xb = cookie;
	struct xbridge_intrhandler *xih = (struct xbridge_intrhandler *)vih;
	struct xbridge_intr *xi = xih->xih_main;
	int intrbit = xi->xi_intrbit;

	evcount_detach(&xih->xih_count);
	LIST_REMOVE(xih, xih_nxt);

	if (LIST_EMPTY(&xi->xi_handlers)) {
		xbridge_write_reg(xb, BRIDGE_INT_ADDR(intrbit), 0);
		xbridge_write_reg(xb, BRIDGE_IER,
		    xbridge_read_reg(xb, BRIDGE_IER) & ~(1 << intrbit));
		xbridge_write_reg(xb, BRIDGE_INT_MODE,
		    xbridge_read_reg(xb, BRIDGE_INT_MODE) & ~(1 << intrbit));
		xbridge_write_reg(xb, BRIDGE_INT_DEV,
		    xbridge_read_reg(xb, BRIDGE_INT_DEV) &
		    ~(7 << (intrbit * 3)));
		(void)xbridge_read_reg(xb, WIDGET_TFLUSH);

		xbow_intr_disestablish(xi->xi_intrsrc);
		/*
		 * Note we could free xb->xb_intr[intrbit] at this point,
		 * but it's not really worth doing.
		 */
	}

	free(xih, M_DEVBUF);
}

int
xbridge_intr_line(void *cookie, pci_intr_handle_t ih)
{
	return XBRIDGE_INTR_BIT(ih);
}

int
xbridge_intr_handler(void *v)
{
	struct xbridge_intr *xi = (struct xbridge_intr *)v;
	struct xbridge_bus *xb = xi->xi_bus;
	struct xbridge_intrhandler *xih;
	int rc = 0;
	int spurious;
	uint32_t isr;

	if (LIST_EMPTY(&xi->xi_handlers)) {
		printf("%s: spurious irq %d\n", DEVNAME(xb), xi->xi_intrbit);
		return 0;
	}

	/*
	 * Flush PCI write buffers before servicing the interrupt.
	 */
	LIST_FOREACH(xih, &xi->xi_handlers, xih_nxt)
		xbridge_read_reg(xb, BRIDGE_DEVICE_WBFLUSH(xih->xih_device));

	isr = xbridge_read_reg(xb, BRIDGE_ISR);
	if ((isr & (1 << xi->xi_intrbit)) == 0) {
		spurious = 1;
#ifdef DEBUG
		printf("%s: irq %d but not pending in ISR %08x\n",
		    DEVNAME(xb), xi->xi_intrbit, isr);
#endif
	} else
		spurious = 0;

	LIST_FOREACH(xih, &xi->xi_handlers, xih_nxt) {
		splraise(imask[xih->xih_level]);
		if ((*xih->xih_func)(xih->xih_arg) != 0) {
			xih->xih_count.ec_count++;
			rc = 1;
		}
	}
	if (rc == 0 && spurious == 0)
		printf("%s: spurious irq %d\n", DEVNAME(xb), xi->xi_intrbit);

	/*
	 * There is a known BRIDGE race in which, if two interrupts
	 * on two different pins occur within 60nS of each other,
	 * further interrupts on the first pin do not cause an
	 * interrupt to be sent.
	 *
	 * The workaround against this is to check if our interrupt
	 * source is still active (i.e. another interrupt is pending),
	 * in which case we force an interrupt anyway.
	 *
	 * The XBridge even has a nice facility to do this, where we
	 * do not even have to check if our interrupt is pending.
	 */

	if (ISSET(xb->xb_flags, XF_XBRIDGE))
		xbridge_write_reg(xb, BRIDGE_INT_FORCE_PIN(xi->xi_intrbit), 1);
	else {
		if (xbridge_read_reg(xb, BRIDGE_ISR) & (1 << xi->xi_intrbit)) {
			switch (sys_config.system_type) {
#if defined(TGT_OCTANE)
			case SGI_OCTANE:
				/* XXX what to do there? */
				break;
#endif
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000)
			case SGI_O200:
			case SGI_O300:
				IP27_RHUB_PI_S(xb->xb_nasid, 0, HUBPI_IR_CHANGE,
				    PI_IR_SET | xi->xi_intrsrc);
				break;
#endif
			}
		}
	}

	return 1;
}

/*
 ********************* chip register access.
 */

uint32_t
bridge_read_reg(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a)
{
	return widget_read_4(t, h, a);
}
void
bridge_write_reg(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a,
    uint32_t v)
{
	widget_write_4(t, h, a, v);
}

uint32_t
pic_read_reg(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a)
{
	return (uint32_t)widget_read_8(t, h, a);
}

void
pic_write_reg(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t a,
    uint32_t v)
{
	widget_write_8(t, h, a, (uint64_t)v);
}

/*
 ********************* bus_space glue.
 */

uint8_t
xbridge_read_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint8_t *)((h + o) ^ 3);
}

uint16_t
xbridge_read_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o)
{
	return *(volatile uint16_t *)((h + o) ^ 2);
}

void
xbridge_write_1(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint8_t v)
{
	*(volatile uint8_t *)((h + o) ^ 3) = v;
}

void
xbridge_write_2(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    uint16_t v)
{
	*(volatile uint16_t *)((h + o) ^ 2) = v;
}

void
xbridge_read_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)((h + o) ^ 2);
	len >>= 1;
	while (len-- != 0) {
		*(uint16_t *)buf = letoh16(*addr);
		buf += 2;
	}
}

void
xbridge_write_raw_2(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint16_t *addr = (volatile uint16_t *)((h + o) ^ 2);
	len >>= 1;
	while (len-- != 0) {
		*addr = htole16(*(uint16_t *)buf);
		buf += 2;
	}
}

void
xbridge_read_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*(uint32_t *)buf = letoh32(*addr);
		buf += 4;
	}
}

void
xbridge_write_raw_4(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint32_t *addr = (volatile uint32_t *)(h + o);
	len >>= 2;
	while (len-- != 0) {
		*addr = htole32(*(uint32_t *)buf);
		buf += 4;
	}
}

void
xbridge_read_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*(uint64_t *)buf = letoh64(*addr);
		buf += 8;
	}
}

void
xbridge_write_raw_8(bus_space_tag_t t, bus_space_handle_t h, bus_addr_t o,
    const uint8_t *buf, bus_size_t len)
{
	volatile uint64_t *addr = (volatile uint64_t *)(h + o);
	len >>= 3;
	while (len-- != 0) {
		*addr = htole64(*(uint64_t *)buf);
		buf += 8;
	}
}

int
xbridge_space_map_devio(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;
	bus_addr_t bpa;
#ifdef DIAGNOSTIC
	bus_addr_t start, end;
	uint d;
#endif

	if ((offs >> 24) != xb->xb_devio_skew)
		return EINVAL;	/* not a devio mapping */

	/*
	 * Figure out which devio `slot' we are using, and make sure
	 * we do not overrun it.
	 */
	bpa = offs & ((1UL << 24) - 1);
#ifdef DIAGNOSTIC
	for (d = 0; d < xb->xb_nslots; d++) {
		start = PIC_DEVIO_OFFS(xb->xb_busno, d);
		end = start + BRIDGE_DEVIO_SIZE(d);
		if (bpa >= start && bpa < end) {
			if (bpa + size > end)
				return EINVAL;
			else
				break;
		}
	}
	if (d == xb->xb_nslots)
		return EINVAL;
#endif

	/*
	 * Note we can not use our own bus_base because it might not point
	 * to our small window. Instead, use the one used by the xbridge
	 * driver itself, which _always_ points to the short window.
	 */
	*bshp = xb->xb_regt->bus_base + bpa;
	return 0;
}

int
xbridge_space_map_io(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.
	 */

	if ((offs >> 24) == xb->xb_devio_skew)
		return xbridge_space_map_devio(t, offs, size, flags, bshp);

#ifdef DIAGNOSTIC
	/* check that this does not overflow the mapping */
	if (offs < xb->xb_iostart || offs + size - 1 > xb->xb_ioend)
		return EINVAL;
#endif

	*bshp = (t->bus_base + offs);
	return 0;
}

int
xbridge_space_map_mem(bus_space_tag_t t, bus_addr_t offs, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.  Except on Octane where we never setup devio memory
	 * mappings, because the large mapping is always available.
	 */

	if (sys_config.system_type != SGI_OCTANE &&
	    (offs >> 24) == xb->xb_devio_skew)
		return xbridge_space_map_devio(t, offs, size, flags, bshp);

#ifdef DIAGNOSTIC
	/* check that this does not overflow the mapping */
	if (offs < xb->xb_memstart || offs + size - 1 > xb->xb_memend)
		return EINVAL;
#endif

	*bshp = (t->bus_base + offs);
	return 0;
}

int
xbridge_space_region_devio(bus_space_tag_t t , bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
#ifdef DIAGNOSTIC
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;
	bus_addr_t bpa;
	bus_addr_t start, end;
	uint d;
#endif

#ifdef DIAGNOSTIC
	/*
	 * Note we can not use our own bus_base because it might not point
	 * to our small window. Instead, use the one used by the xbridge
	 * driver itself, which _always_ points to the short window.
	 */
	bpa = (bus_addr_t)bsh - xb->xb_regt->bus_base;

	if ((bpa >> 24) != 0)
		return EINVAL;	/* not a devio mapping */

	/*
	 * Figure out which devio `slot' we are using, and make sure
	 * we do not overrun it.
	 */
	for (d = 0; d < xb->xb_nslots; d++) {
		start = PIC_DEVIO_OFFS(xb->xb_busno, d);
		end = start + BRIDGE_DEVIO_SIZE(d);
		if (bpa >= start && bpa < end) {
			if (bpa + offset + size > end)
				return EINVAL;
			else
				break;
		}
	}
	if (d == xb->xb_nslots)
		return EINVAL;
#endif

	*nbshp = bsh + offset;
	return 0;
}

int
xbridge_space_region_io(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;
	bus_addr_t bpa;

	/*
	 * Note we can not use our own bus_base because it might not point
	 * to our small window. Instead, use the one used by the xbridge
	 * driver itself, which _always_ points to the short window.
	 */
	bpa = (bus_addr_t)bsh - xb->xb_regt->bus_base;

	if ((bpa >> 24) == 0)
		return xbridge_space_region_devio(t, bsh, offset, size, nbshp);

#ifdef DIAGNOSTIC
	/* check that this does not overflow the mapping */
	bpa = (bus_addr_t)bsh - t->bus_base;
	if (bpa + offset + size - 1 > xb->xb_ioend)
		return EINVAL;
#endif

	*nbshp = bsh + offset;
	return 0;
}

int
xbridge_space_region_mem(bus_space_tag_t t, bus_space_handle_t bsh,
    bus_size_t offset, bus_size_t size, bus_space_handle_t *nbshp)
{
	struct xbridge_bus *xb = (struct xbridge_bus *)t->bus_private;
	bus_addr_t bpa;

	/*
	 * Base address is either within the devio area, or our direct
	 * window.  Except on Octane where we never setup devio memory
	 * mappings, because the large mapping is always available.
	 */
	if (sys_config.system_type != SGI_OCTANE) {
		/*
		 * Note we can not use our own bus_base because it might not
		 * point to our small window. Instead, use the one used by
		 * the xbridge driver itself, which _always_ points to the
		 * short window.
		 */
		bpa = (bus_addr_t)bsh - xb->xb_regt->bus_base;

		if ((bpa >> 24) == 0)
			return xbridge_space_region_devio(t, bsh, offset, size,
			    nbshp);
	}

#ifdef DIAGNOSTIC
	/* check that this does not overflow the mapping */
	bpa = (bus_addr_t)bsh - t->bus_base;
	if (bpa + offset + size - 1 > xb->xb_memend)
		return EINVAL;
#endif

	*nbshp = bsh + offset;
	return 0;
}

/*
 ********************* bus_dma helpers
 */

/*
 * ATE primer:
 *
 * ATE are iommu translation entries. PCI addresses in the translated
 * window transparently map to the address their ATE point to.
 *
 * Bridge chip have 128 so-called `internal' entries, and can use their
 * optional SSRAM to provide more (up to 65536 entries with 512KB SSRAM).
 * However, due to chip bugs, those `external' entries can not be updated
 * while there is DMA in progress using external entries, even if the
 * updated entries are disjoint from those used by the DMA transfer.
 *
 * XBridge chip extend the internal entries to 1024, and do not provide
 * support for external entries.
 *
 * We limit ourselves to internal entries only. Due to the way we force
 * bus_dmamem_alloc() to use the direct window, there won't hopefully be
 * many concurrent consumers of ATE at once.
 *
 * All ATE share the same page size, which is configurable as 4KB or 16KB.
 * In order to minimize the number of ATE used by the various drivers,
 * we use 16KB pages, at the expense of trickier code to account for
 * ATE shared by various dma maps.
 *
 * ATE management:
 *
 * An array of internal ATE management structures is allocated, and
 * provides reference counters (since various dma maps could overlap
 * the same 16KB ATE pages).
 *
 * When using ATE in the various bus_dmamap_load*() functions, we try
 * to coalesce individual contiguous pages sharing the same I/O page
 * (and thus the same ATE). However, no attempt is made to optimize
 * entries using contiguous ATEs.
 *
 * ATE are organized in lists of in-use and free entries.
 */

struct xbridge_ate {
	LIST_ENTRY(xbridge_ate)	 xa_nxt;
	uint			 xa_refcnt;
	paddr_t			 xa_pa;
};

#ifdef ATE_DEBUG
void
xbridge_ate_dump(struct xbridge_bus *xb)
{
	struct xbridge_ate *ate;
	uint a;

	printf("%s ATE list (in array order)\n", DEVNAME(xb));
	for (a = 0, ate = xb->xb_ate; a < xb->xb_atecnt; a++, ate++) {
		printf("%03x %p %02u", a, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
	}
	if ((a % 3) != 0)
		printf("\n");

	printf("%s USED ATE list (in link order)\n", DEVNAME(xb));
	a = 0;
	LIST_FOREACH(ate, &xb->xb_used_ate, xa_nxt) {
		printf("%03x %p %02u",
		    ate - xb->xb_ate, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
		a++;
	}
	if ((a % 3) != 0)
		printf("\n");

	printf("%s FREE ATE list (in link order)\n", DEVNAME(xb));
	a = 0;
	LIST_FOREACH(ate, &xb->xb_free_ate, xa_nxt) {
		printf("%03x %p %02u",
		    ate - xb->xb_ate, ate->xa_pa, ate->xa_refcnt);
		if ((a % 3) == 2)
			printf("\n");
		else
			printf("  ");
		a++;
	}
	if ((a % 3) != 0)
		printf("\n");
}
#endif

void
xbridge_ate_setup(struct xbridge_bus *xb)
{
	uint32_t ctrl;
	uint a;
	struct xbridge_ate *ate;

	mtx_init(&xb->xb_atemtx, IPL_HIGH);

	if (ISSET(xb->xb_flags, XF_XBRIDGE))
		xb->xb_atecnt = XBRIDGE_INTERNAL_ATE;
	else
		xb->xb_atecnt = BRIDGE_INTERNAL_ATE;

	xb->xb_ate = (struct xbridge_ate *)malloc(xb->xb_atecnt *
	    sizeof(struct xbridge_ate), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (xb->xb_ate == NULL) {
		/* we could run without, but this would be a PITA */
		panic("%s: no memory for ATE management", __func__);
	}

	/*
	 * Setup the ATE lists.
	 */
	LIST_INIT(&xb->xb_free_ate);
	LIST_INIT(&xb->xb_used_ate);
	for (ate = xb->xb_ate; ate != xb->xb_ate + xb->xb_atecnt; ate++)
		LIST_INSERT_HEAD(&xb->xb_free_ate, ate, xa_nxt);

	/*
	 * Switch to 16KB pages.
	 */
	ctrl = xbridge_read_reg(xb, WIDGET_CONTROL);
	xbridge_write_reg(xb, WIDGET_CONTROL,
	    ctrl | BRIDGE_WIDGET_CONTROL_LARGE_PAGES);
	(void)xbridge_read_reg(xb, WIDGET_TFLUSH);

	/*
	 * Initialize all ATE entries to invalid.
	 */
	for (a = 0; a < xb->xb_atecnt; a++)
		xbridge_ate_write(xb, a, ATE_NV);
}

#ifdef unused
uint64_t
xbridge_ate_read(struct xbridge_bus *xb, uint a)
{
	uint32_t lo, hi;
	uint64_t ate;

	/*
	 * ATE can not be read as a whole, and need two 32 bit accesses.
	 */
	hi = xbridge_read_reg(xb, BRIDGE_ATE(a) + 4);
	if (ISSET(xb->xb_flags, XF_XBRIDGE))
		lo = xbridge_read_reg(xb, BRIDGE_ATE(a + 1024) + 4);
	else
		lo = xbridge_read_reg(xb, BRIDGE_ATE(a + 512) + 4);

	ate = (uint64_t)hi;
	ate <<= 32;
	ate |= lo;

	return ate;
}
#endif

void
xbridge_ate_write(struct xbridge_bus *xb, uint a, uint64_t ate)
{
	widget_write_8(xb->xb_regt, xb->xb_regh, BRIDGE_ATE(a), ate);
}

uint
xbridge_ate_find(struct xbridge_bus *xb, paddr_t pa)
{
	uint a;
	struct xbridge_ate *ate;

	/* round to ATE page */
	pa &= ~BRIDGE_ATE_LMASK;

	/*
	 * XXX Might want to build a tree to make this faster than
	 * XXX that stupid linear search. On the other hand there
	 * XXX aren't many ATE entries.
	 */
	LIST_FOREACH(ate, &xb->xb_used_ate, xa_nxt)
		if (ate->xa_pa == pa) {
			a = ate - xb->xb_ate;
#ifdef ATE_DEBUG
			printf("%s: pa %p ate %u (r %u)\n",
			    __func__, pa, a, ate->xa_refcnt);
#endif
			return a;
		}

	return (uint)-1;
}

uint
xbridge_ate_add(struct xbridge_bus *xb, paddr_t pa)
{
	uint a;
	struct xbridge_ate *ate;

	/* round to ATE page */
	pa &= ~BRIDGE_ATE_LMASK;

	if (LIST_EMPTY(&xb->xb_free_ate)) {
#ifdef ATE_DEBUG
		printf("%s: out of ATEs\n", DEVNAME(xb));
#endif
		return (uint)-1;
	}

	ate = LIST_FIRST(&xb->xb_free_ate);
	LIST_REMOVE(ate, xa_nxt);
	LIST_INSERT_HEAD(&xb->xb_used_ate, ate, xa_nxt);
	ate->xa_refcnt = 1;
	ate->xa_pa = pa;

	a = ate - xb->xb_ate;
#ifdef ATE_DEBUG
	printf("%s: pa %p ate %u\n", __func__, pa, a);
#endif

	xbridge_ate_write(xb, a, ate->xa_pa |
	    (xbow_intr_widget << ATE_WIDGET_SHIFT) | ATE_COH | ATE_V);

	return a;
}

void
xbridge_ate_unref(struct xbridge_bus *xb, uint a, uint ref)
{
	struct xbridge_ate *ate;

	ate = xb->xb_ate + a;
#ifdef DIAGNOSTIC
	if (ref > ate->xa_refcnt)
		panic("%s: ate #%u %p has only %u refs but needs to drop %u",
		    DEVNAME(xb), a, ate, ate->xa_refcnt, ref);
#endif
	ate->xa_refcnt -= ref;
	if (ate->xa_refcnt == 0) {
#ifdef ATE_DEBUG
		printf("%s: free ate %u\n", __func__, a);
#endif
		xbridge_ate_write(xb, a, ATE_NV);
		LIST_REMOVE(ate, xa_nxt);
		LIST_INSERT_HEAD(&xb->xb_free_ate, ate, xa_nxt);
	} else {
#ifdef ATE_DEBUG
		printf("%s: unref ate %u (r %u)\n", __func__, a, ate->xa_refcnt);
#endif
	}
}

/*
 * Attempt to map the given address, either through the direct map, or
 * using an ATE.
 */
int
xbridge_address_map(struct xbridge_bus *xb, paddr_t pa, bus_addr_t *mapping,
    bus_addr_t *limit)
{
	struct xbridge_ate *ate;
	bus_addr_t ba;
	uint a;

	/*
	 * Try the direct DMA window first.
	 */

#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		ba = (bus_addr_t)pa - IP30_MEMORY_BASE;
	else
#endif
		ba = (bus_addr_t)pa;

	if (ba < BRIDGE_DMA_DIRECT_LENGTH) {
		*mapping = ba + BRIDGE_DMA_DIRECT_BASE;
		*limit = BRIDGE_DMA_DIRECT_LENGTH + BRIDGE_DMA_DIRECT_BASE;
		return 0;
	}

	/*
	 * Did not fit, so now we need to use an ATE.
	 * Check if an existing ATE would do the job; if not, try and
	 * allocate a new one.
	 */

	mtx_enter(&xb->xb_atemtx);

	a = xbridge_ate_find(xb, pa);
	if (a != (uint)-1) {
		ate = xb->xb_ate + a;
		ate->xa_refcnt++;
	} else
		a = xbridge_ate_add(xb, pa);

	if (a != (uint)-1) {
		ba = ATE_ADDRESS(a, BRIDGE_ATE_LSHIFT);
		/*
		 * Ask for byteswap during DMA. On Bridge (i.e non-XBridge),
		 * this setting is device-global and is enforced by
		 * BRIDGE_DEVICE_SWAP_PMU set in the devio register.
		 */
		if (ISSET(xb->xb_flags, XF_XBRIDGE))
			ba |= XBRIDGE_DMA_TRANSLATED_SWAP;
#ifdef ATE_DEBUG
		printf("%s: ate %u through %p\n", __func__, a, ba);
#endif
		*mapping = ba + (pa & BRIDGE_ATE_LMASK);
		*limit = ba + BRIDGE_ATE_LSIZE;
		mtx_leave(&xb->xb_atemtx);
		return 0;
	}

	printf("%s: out of ATE\n", DEVNAME(xb));
#ifdef ATE_DEBUG
	xbridge_ate_dump(xb);
#endif

	mtx_leave(&xb->xb_atemtx);

	/*
	 * We could try allocating a bounce buffer here.
	 * Maybe once there is a MI interface for this...
	 */

	return EINVAL;
}

void
xbridge_address_unmap(struct xbridge_bus *xb, bus_addr_t ba, bus_size_t len)
{
	uint a;
	uint refs;

	/*
	 * If this address matches an ATE, unref it, and make it
	 * available again if the reference count drops to zero.
	 */
	if (ba < BRIDGE_DMA_TRANSLATED_BASE || ba >= BRIDGE_DMA_DIRECT_BASE)
		return;

	if (ba & XBRIDGE_DMA_TRANSLATED_SWAP)
		ba &= ~XBRIDGE_DMA_TRANSLATED_SWAP;

	a = ATE_INDEX(ba, BRIDGE_ATE_LSHIFT);
#ifdef DIAGNOSTIC
	if (a >= xb->xb_atecnt)
		panic("%s: bus address %p references nonexisting ATE %u/%u",
		    __func__, ba, a, xb->xb_atecnt);
#endif

	/*
	 * Since we only coalesce contiguous pages or page fragments in
	 * the maps, and we made sure not to cross I/O page boundaries,
	 * we have one reference per cpu page the range [ba, ba+len-1]
	 * hits.
	 */
	refs = 1 + atop(ba + len - 1) - atop(ba);

	mtx_enter(&xb->xb_atemtx);
	xbridge_ate_unref(xb, a, refs);
	mtx_leave(&xb->xb_atemtx);
}

/*
 * bus_dmamap_loadXXX() bowels implementation.
 */
int
xbridge_dmamap_load_buffer(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags, paddr_t *lastaddrp,
    int *segp, int first)
{
	struct xbridge_bus *xb = t->_cookie;
	bus_size_t sgsize;
	bus_addr_t lastaddr, baddr, bmask;
	bus_addr_t busaddr, endaddr;
	paddr_t pa;
	vaddr_t vaddr = (vaddr_t)buf;
	int seg;
	pmap_t pmap;
	int rc;

	if (first) {
		for (seg = 0; seg < map->_dm_segcnt; seg++)
			map->dm_segs[seg].ds_addr = 0;
	}

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	lastaddr = *lastaddrp;
	bmask  = ~(map->_dm_boundary - 1);
	if (t->_dma_mask != 0)
		bmask &= t->_dma_mask;

	for (seg = *segp; buflen > 0; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap_extract(pmap, vaddr, &pa) == FALSE)
			panic("%s: pmap_extract(%x, %x) failed",
			    __func__, pmap, vaddr);

		/*
		 * Compute the DMA address and the physical range 
		 * this mapping can cover.
		 */
		if (xbridge_address_map(xb, pa, &busaddr, &endaddr) != 0) {
			rc = ENOMEM;
			goto fail_unmap;
		}

		/*
		 * Compute the segment size, and adjust counts.
		 * Note that we do not min() against (endaddr - busaddr)
		 * as the initial sgsize computation is <= (endaddr - busaddr).
		 */
		sgsize = PAGE_SIZE - ((u_long)vaddr & PGOFSET);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (map->_dm_boundary > 0) {
			baddr = (pa + map->_dm_boundary) & bmask;
			if (sgsize > (baddr - pa))
				sgsize = baddr - pa;
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			map->dm_segs[seg].ds_addr = busaddr;
			map->dm_segs[seg].ds_len = sgsize;
			map->dm_segs[seg]._ds_vaddr = (vaddr_t)vaddr;
			first = 0;
		} else {
			if (busaddr == lastaddr &&
			    (map->dm_segs[seg].ds_len + sgsize) <=
			     map->_dm_maxsegsz &&
			     (map->_dm_boundary == 0 ||
			     (map->dm_segs[seg].ds_addr & bmask) ==
			     (busaddr & bmask)))
				map->dm_segs[seg].ds_len += sgsize;
			else {
				if (++seg >= map->_dm_segcnt) {
					/* drop partial ATE reference */
					xbridge_address_unmap(xb, busaddr,
					    sgsize);
					break;
				}
				map->dm_segs[seg].ds_addr = busaddr;
				map->dm_segs[seg].ds_len = sgsize;
				map->dm_segs[seg]._ds_vaddr = (vaddr_t)vaddr;
			}
		}

		lastaddr = busaddr + sgsize;
		if (lastaddr == endaddr)
			lastaddr = ~0;	/* can't coalesce */
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		rc = EFBIG;
		goto fail_unmap;
	}

	return 0;

fail_unmap:
	/*
	 * If control goes there, we need to unref all our ATE, if any.
	 */
	for (seg = 0; seg < map->_dm_segcnt; seg++) {
		xbridge_address_unmap(xb, map->dm_segs[seg].ds_addr,
		    map->dm_segs[seg].ds_len);
		map->dm_segs[seg].ds_addr = 0;
	}

	return rc;
}

/*
 * bus_dmamap_unload() implementation.
 */
void
xbridge_dmamap_unload(bus_dma_tag_t t, bus_dmamap_t map)
{
	struct xbridge_bus *xb = t->_cookie;
	int seg;

	for (seg = 0; seg < map->_dm_segcnt; seg++) {
		xbridge_address_unmap(xb, map->dm_segs[seg].ds_addr,
		    map->dm_segs[seg].ds_len);
		map->dm_segs[seg].ds_addr = 0;
	}
	map->dm_nsegs = 0;
	map->dm_mapsize = 0;
}

/*
 * bus_dmamem_alloc() implementation.
 */
int
xbridge_dmamem_alloc(bus_dma_tag_t t, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	paddr_t low, high;

	/*
	 * Limit bus_dma'able memory to the first 2GB of physical memory.
	 * XXX This should be lifted if flags & BUS_DMA_64BIT for drivers
	 * XXX which do not need to restrict themselves to 32 bit DMA
	 * XXX addresses.
	 */
	switch (sys_config.system_type) {
	default:
#if defined(TGT_ORIGIN200) || defined(TGT_ORIGIN2000)
	case SGI_O200:
	case SGI_O300:
		low = 0;
		break;
#endif
#if defined(TGT_OCTANE)
	case SGI_OCTANE:
		low = IP30_MEMORY_BASE;
		break;
#endif
	}
	high = low + BRIDGE_DMA_DIRECT_LENGTH - 1;

	return _dmamem_alloc_range(t, size, alignment, boundary,
	    segs, nsegs, rsegs, flags, low, high);
}

/*
 * Since we override the various bus_dmamap_load*() functions, the only
 * caller of pa_to_device() and device_to_pa() is _dmamem_alloc_range(),
 * invoked by xbridge_dmamem_alloc() above. Since we make sure this
 * function can only return memory fitting in the direct DMA window, we do
 * not need to check for other cases.
 */

bus_addr_t
xbridge_pa_to_device(paddr_t pa)
{
#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		pa -= IP30_MEMORY_BASE;
#endif

	return pa + BRIDGE_DMA_DIRECT_BASE;
}

paddr_t
xbridge_device_to_pa(bus_addr_t addr)
{
	paddr_t pa = addr - BRIDGE_DMA_DIRECT_BASE;

#ifdef TGT_OCTANE
	if (sys_config.system_type == SGI_OCTANE)
		pa += IP30_MEMORY_BASE;
#endif

	return pa;
}

/*
 ********************* Bridge configuration code.
 */

void
xbridge_setup(struct xbridge_bus *xb)
{
	paddr_t pa;
	uint32_t ctrl;
	int dev;

	/*
	 * Gather device identification for all slots.
	 * We need this to be able to allocate RRBs correctly, and also
	 * to be able to check quickly whether a given device is an IOC3.
	 */

	for (dev = 0; dev < xb->xb_nslots; dev++) {
		pa = xb->xb_regh + BRIDGE_PCI_CFG_SPACE +
		    (dev << 12) + PCI_ID_REG;
		if (guarded_read_4(pa, &xb->xb_devices[dev].id) != 0)
			xb->xb_devices[dev].id =
			    PCI_ID_CODE(PCI_VENDOR_INVALID, 0xffff);
	}

	/*
	 * Configure the direct DMA window to access the low 2GB of memory.
	 * XXX assumes masternasid is 0
	 */

	if (sys_config.system_type == SGI_OCTANE)
		xbridge_write_reg(xb, BRIDGE_DIR_MAP, BRIDGE_DIRMAP_ADD_512MB |
		    (xbow_intr_widget << BRIDGE_DIRMAP_WIDGET_SHIFT));
	else
		xbridge_write_reg(xb, BRIDGE_DIR_MAP,
		    xbow_intr_widget << BRIDGE_DIRMAP_WIDGET_SHIFT);

	/*
	 * Figure out how many ATE we can use for non-direct DMA, and
	 * setup our ATE management code.
	 */

	xbridge_ate_setup(xb);

	/*
	 * Allocate RRB for the existing devices.
	 */

	xbridge_rrb_setup(xb, 0);
	xbridge_rrb_setup(xb, 1);

	/*
	 * Disable byteswapping on PIO accesses through the large window
	 * (we handle this at the bus_space level). It should not have
	 * been enabled by ARCS, since IOC serial console relies on this,
	 * but better enforce this anyway.
	 */

	ctrl = xbridge_read_reg(xb, WIDGET_CONTROL);
	ctrl &= ~BRIDGE_WIDGET_CONTROL_IO_SWAP;
	ctrl &= ~BRIDGE_WIDGET_CONTROL_MEM_SWAP;
	xbridge_write_reg(xb, WIDGET_CONTROL, ctrl);
	(void)xbridge_read_reg(xb, WIDGET_TFLUSH);

	/*
	 * The PROM will only configure the onboard devices. Set up
	 * any other device we might encounter.
	 */

	xbridge_resource_setup(xb);

	/*
	 * Setup interrupt handling.
	 */

	xbridge_write_reg(xb, BRIDGE_IER, 0);
	xbridge_write_reg(xb, BRIDGE_INT_MODE, 0);
	xbridge_write_reg(xb, BRIDGE_INT_DEV, 0);

	xbridge_write_reg(xb, WIDGET_INTDEST_ADDR_UPPER,
	    (xbow_intr_widget_register >> 32) | (xbow_intr_widget << 16));
	xbridge_write_reg(xb, WIDGET_INTDEST_ADDR_LOWER,
	    (uint32_t)xbow_intr_widget_register);

	(void)xbridge_read_reg(xb, WIDGET_TFLUSH);
}

/*
 * Build a not-so-pessimistic RRB allocation register value.
 */
void
xbridge_rrb_setup(struct xbridge_bus *xb, int odd)
{
	uint rrb[MAX_SLOTS / 2];	/* tentative rrb assignment */
	uint total;			/* rrb count */
	uint32_t proto;			/* proto rrb value */
	int dev, i, j;

	/*
	 * First, try to allocate as many RRBs per device as possible.
	 */

	total = 0;
	for (i = 0; i < nitems(rrb); i++) {
		dev = (i << 1) + !!odd;
		if (dev >= xb->xb_nslots ||
		    PCI_VENDOR(xb->xb_devices[dev].id) == PCI_VENDOR_INVALID)
			rrb[i] = 0;
		else
			rrb[i] = 4;	/* optimistic value */
		total += rrb[i];
	}

	/*
	 * Then, try to reduce greed until we do not claim more than
	 * the 8 RRBs we can afford.
	 */

	if (total > 8) {
		/*
		 * All devices should be able to live with 3 RRBs, so
		 * reduce their allocation from 4 to 3.
		 */
		for (i = 0; i < nitems(rrb); i++) {
			if (rrb[i] == 4) {
				rrb[i]--;
				if (--total == 8)
					break;
			}
		}
	}

	if (total > 8) {
		/*
		 * There are too many devices for 3 RRBs per device to
		 * be possible. Attempt to reduce from 3 to 2, except
		 * for isp(4) devices.
		 */
		for (i = 0; i < nitems(rrb); i++) {
			if (rrb[i] == 3) {
				dev = (i << 1) + !!odd;
				if (PCI_VENDOR(xb->xb_devices[dev].id) !=
				    PCI_VENDOR_QLOGIC) {
					rrb[i]--;
					if (--total == 8)
						break;
				}
			}
		}
	}

	if (total > 8) {
		/*
		 * Too bad, we need to shrink the RRB allocation for
		 * isp devices too. We'll try to favour the lowest
		 * slots, though, hence the reversed loop order.
		 */
		for (i = nitems(rrb) - 1; i >= 0; i--) {
			if (rrb[i] == 3) {
				rrb[i]--;
				if (--total == 8)
					break;
			}
		}
	}

	/*
	 * Now build the RRB register value proper.
	 */

	proto = 0;
	for (i = 0; i < nitems(rrb); i++) {
		for (j = 0; j < rrb[i]; j++)
			proto = (proto << RRB_SHIFT) | (RRB_VALID | i);
	}

	xbridge_write_reg(xb, odd ? BRIDGE_RRB_ODD : BRIDGE_RRB_EVEN, proto);
}

/*
 * Configure PCI resources for all devices.
 */
void
xbridge_resource_setup(struct xbridge_bus *xb)
{
	pci_chipset_tag_t pc = &xb->xb_pc;
	int dev, nfuncs;
	pcitag_t tag;
	pcireg_t id, bhlcr;
	uint32_t devio;
	int need_setup;
	uint secondary, nppb, npccbb, ppbstride;
	const struct pci_quirkdata *qd;

	/*
	 * On Octane, we will want to map everything through the large
	 * windows, whenever possible.
	 *
	 * Set up these mappings now.
	 */

	if (sys_config.system_type == SGI_OCTANE) {
		xb->xb_ioex = xbridge_mapping_setup(xb, 1);
		xb->xb_memex = xbridge_mapping_setup(xb, 0);
	}

	/*
	 * Configure all regular PCI devices.
	 */

	nppb = npccbb = 0;
	for (dev = 0; dev < xb->xb_nslots; dev++) {
		id = xb->xb_devices[dev].id;

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			continue;

		/*
		 * Count ppb and pccbb devices, we will need their number later.
		 */

		tag = pci_make_tag(pc, 0, dev, 0);
		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) == 1)
			nppb++;
		if (PCI_HDRTYPE_TYPE(bhlcr) == 2)
			npccbb++;

		/*
		 * We want to avoid changing mapping configuration for
		 * devices which have been setup by ARCS.
		 *
		 * On Octane, the whole on-board I/O widget has been
		 * set up, with direct mappings into widget space.
		 *
		 * On Origin, since direct mappings are expensive,
		 * everything set up by ARCS has a valid devio
		 * mapping; those can be identified as they sport the
		 * widget number in the high address bits.
		 *
		 * We will only fix the device-global devio flags on
		 * devices which have been set up by ARCS.  Otherwise,
		 * we'll need to perform proper PCI resource allocation.
		 */

		devio = xbridge_read_reg(xb, BRIDGE_DEVICE(dev));
#ifdef DEBUG
		printf("device %d: devio %08x\n", dev, devio);
#endif
		if (id != PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3))
			need_setup = 1;
		else
			need_setup = xb->xb_devio_skew !=
			    ((devio & BRIDGE_DEVICE_BASE_MASK) >>
			     (24 - BRIDGE_DEVICE_BASE_SHIFT));

		/*
		 * Enable byte swapping for DMA, except on IOC3 and
		 * RAD1 devices.
		 */
		if (ISSET(xb->xb_flags, XF_XBRIDGE))
			devio &= ~BRIDGE_DEVICE_SWAP_PMU;
		else
			devio |= BRIDGE_DEVICE_SWAP_PMU;
		devio |= BRIDGE_DEVICE_SWAP_DIR;
		if (id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_IOC3) ||
		    id == PCI_ID_CODE(PCI_VENDOR_SGI, PCI_PRODUCT_SGI_RAD1))
			devio &=
			    ~(BRIDGE_DEVICE_SWAP_DIR | BRIDGE_DEVICE_SWAP_PMU);

		/*
		 * Disable prefetching - on-board isp(4) controllers on
		 * Octane are set up with this, but this confuses the
		 * driver.
		 */
		devio &= ~BRIDGE_DEVICE_PREFETCH;

		/*
		 * Force cache coherency.
		 */
		devio |= BRIDGE_DEVICE_COHERENT;

		if (need_setup == 0) {
			xbridge_set_devio(xb, dev, devio);
			continue;
		}

		/*
		 * Clear any residual devio mapping.
		 */
		devio &= ~BRIDGE_DEVICE_BASE_MASK;
		devio &= ~BRIDGE_DEVICE_IO_MEM;
		xbridge_set_devio(xb, dev, devio);

		/*
		 * We now need to perform the resource allocation for this
		 * device, which has not been setup by ARCS.
		 */

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));
		if (PCI_HDRTYPE_MULTIFN(bhlcr) ||
		    (qd != NULL && (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0))
			nfuncs = 8;
		else
			nfuncs = 1;

		xbridge_device_setup(xb, dev, nfuncs, devio);
	}

	/*
	 * Configure PCI-PCI and PCI-CardBus bridges, if any.
	 *
	 * We do this after all the other PCI devices have been configured
	 * in order to favour them during resource allocation.
	 */

	if (npccbb != 0) {
		/*
		 * If there are PCI-CardBus bridges, we really want to be
		 * able to have large resource spaces...
		 */
		if (xb->xb_ioex == NULL)
			xb->xb_ioex = xbridge_mapping_setup(xb, 1);
		if (xb->xb_memex == NULL)
			xb->xb_memex = xbridge_mapping_setup(xb, 0);
	}

	secondary = 1;
	ppbstride = nppb == 0 ? 0 : (255 - npccbb) / nppb;
	for (dev = 0; dev < xb->xb_nslots; dev++) {
		id = xb->xb_devices[dev].id;

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID || PCI_VENDOR(id) == 0)
			continue;

		tag = pci_make_tag(pc, 0, dev, 0);
		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);

		switch (PCI_HDRTYPE_TYPE(bhlcr)) {
		case 1:	/* PCI-PCI bridge */
			ppb_initialize(pc, tag, 0, secondary,
			    secondary + ppbstride - 1);
			secondary += ppbstride;
			break;
		case 2:	/* PCI-CardBus bridge */
			/*
			 * We do not expect cardbus devices to sport
			 * PCI-PCI bridges themselves, so only one
			 * PCI bus will do.
			 */
			pccbb_initialize(pc, tag, 0, secondary, secondary);
			secondary++;
			break;
		}
	}

	if (xb->xb_ioex != NULL) {
		extent_destroy(xb->xb_ioex);
		xb->xb_ioex = NULL;
	}
	if (xb->xb_memex != NULL) {
		extent_destroy(xb->xb_memex);
		xb->xb_memex = NULL;
	}
}

/*
 * Build resource extents for the MI PCI code to play with.
 * These extents cover the configured devio areas, and the large resource
 * views, if applicable.
 */
void
xbridge_extent_setup(struct xbridge_bus *xb)
{
	int dev;
	int errors;
	bus_addr_t start, end;
	uint32_t devio;

	snprintf(xb->xb_ioexname, sizeof(xb->xb_ioexname), "%s_io",
	    DEVNAME(xb));
	xb->xb_ioex = extent_create(xb->xb_ioexname, 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_NOWAIT | EX_FILLED);

	if (xb->xb_ioex != NULL) {
		errors = 0;
		/* make all configured devio ranges available... */
		for (dev = 0; dev < xb->xb_nslots; dev++) {
			devio = xb->xb_devices[dev].devio;
			if (devio == 0)
				continue;
			if (ISSET(devio, BRIDGE_DEVICE_IO_MEM))
				continue;
			start = (devio & BRIDGE_DEVICE_BASE_MASK) <<
			    BRIDGE_DEVICE_BASE_SHIFT;
			if (start == 0)
				continue;
			if (extent_free(xb->xb_ioex, start,
			    BRIDGE_DEVIO_SIZE(dev), EX_NOWAIT) != 0) {
				errors++;
				break;
			}
		}
		/* ...as well as the large views, if any */
		if (xb->xb_ioend != 0) {
			start = xb->xb_iostart;
			if (start == 0)
				start = 1;
			end = xb->xb_devio_skew << 24;
			if (start < end)
				if (extent_free(xb->xb_ioex, start,
				    end, EX_NOWAIT) != 0)
					errors++;
			
			start = (xb->xb_devio_skew + 1) << 24;
			if (start < xb->xb_iostart)
				start = xb->xb_iostart;
			if (extent_free(xb->xb_ioex, start,
			    xb->xb_ioend + 1 - start, EX_NOWAIT) != 0)
				errors++;
		}

		if (errors != 0) {
			extent_destroy(xb->xb_ioex);
			xb->xb_ioex = NULL;
		}
	}

	snprintf(xb->xb_memexname, sizeof(xb->xb_memexname), "%s_mem",
	    DEVNAME(xb));
	xb->xb_memex = extent_create(xb->xb_memexname, 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_NOWAIT | EX_FILLED);

	if (xb->xb_memex != NULL) {
		errors = 0;
		/* make all configured devio ranges available... */
		for (dev = 0; dev < xb->xb_nslots; dev++) {
			devio = xb->xb_devices[dev].devio;
			if (devio == 0 || !ISSET(devio, BRIDGE_DEVICE_IO_MEM))
				continue;
			start = (devio & BRIDGE_DEVICE_BASE_MASK) <<
			    BRIDGE_DEVICE_BASE_SHIFT;
			if (start == 0)
				continue;
			if (extent_free(xb->xb_memex, start,
			    BRIDGE_DEVIO_SIZE(dev), EX_NOWAIT) != 0) {
				errors++;
				break;
			}
		}
		/* ...as well as the large views, if any */
		if (xb->xb_memend != 0) {
			start = xb->xb_memstart;
			if (start == 0)
				start = 1;
			end = xb->xb_devio_skew << 24;
			if (start < end)
				if (extent_free(xb->xb_memex, start,
				    end, EX_NOWAIT) != 0)
					errors++;

			start = (xb->xb_devio_skew + 1) << 24;
			if (start < xb->xb_memstart)
				start = xb->xb_memstart;
			if (extent_free(xb->xb_memex, start,
			    xb->xb_memend + 1 - start, EX_NOWAIT) != 0)
				errors++;
		}

		if (errors != 0) {
			extent_destroy(xb->xb_memex);
			xb->xb_memex = NULL;
		}
	}
}

struct extent *
xbridge_mapping_setup(struct xbridge_bus *xb, int io)
{
	bus_addr_t membase, offs;
	bus_size_t len;
	paddr_t base;
	u_long start, end;
	struct extent *ex = NULL;

	if (io) {
		/*
		 * I/O mappings are available in the widget at offset
		 * BRIDGE_PCI_IO_SPACE_BASE onwards, but weren't working
		 * correctly until Bridge revision 4 (apparently, what
		 * didn't work was the byteswap logic).
		 */

		if (!ISSET(xb->xb_flags, XF_NO_DIRECT_IO)) {
			offs = BRIDGE_PCI_IO_SPACE_BASE;
			len = BRIDGE_PCI_IO_SPACE_LENGTH;
			base = xbow_widget_map_space(xb->xb_sc->sc_dev.dv_parent,
			    xb->xb_widget, &offs, &len);
		} else
			base = 0;

		if (base != 0) {
			if (offs + len > BRIDGE_PCI_IO_SPACE_BASE +
			    BRIDGE_PCI_IO_SPACE_LENGTH)
				len = BRIDGE_PCI_IO_SPACE_BASE +
				    BRIDGE_PCI_IO_SPACE_LENGTH - offs;

#ifdef DEBUG
			printf("direct io %p-%p base %p\n",
			    offs, offs + len - 1, base);
#endif
			offs -= BRIDGE_PCI_IO_SPACE_BASE;

			ex = extent_create("xbridge_direct_io",
			    offs == 0 ? 1 : offs, offs + len - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);

			if (ex != NULL) {
				xb->xb_io_bus_space->bus_base = base - offs;
				xb->xb_io_bus_space->_space_map =
				    xbridge_space_map_io;
				xb->xb_io_bus_space->_space_subregion =
				    xbridge_space_region_io;

				xb->xb_iostart = offs;
				xb->xb_ioend = offs + len - 1;
			}
		}
	} else {
		/*
		 * Memory mappings are available in the widget at offset
		 * BRIDGE_PCI#_MEM_SPACE_BASE onwards.
		 */

		membase = xb->xb_busno == 0 ? BRIDGE_PCI0_MEM_SPACE_BASE :
		    BRIDGE_PCI1_MEM_SPACE_BASE;
		offs = membase;
		len = BRIDGE_PCI_MEM_SPACE_LENGTH;
		base = xbow_widget_map_space(xb->xb_sc->sc_dev.dv_parent,
		    xb->xb_widget, &offs, &len);

		if (base != 0) {
			/*
			 * Only the low 30 bits of memory BAR are honoured
			 * by the hardware, thus restricting memory mappings
			 * to 1GB.
			 */
			if (offs + len > membase + BRIDGE_PCI_MEM_SPACE_LENGTH)
				len = membase + BRIDGE_PCI_MEM_SPACE_LENGTH -
				    offs;

#ifdef DEBUG
			printf("direct mem %p-%p base %p\n",
			    offs, offs + len - 1, base);
#endif
			offs -= membase;

			ex = extent_create("xbridge_direct_mem",
			    offs == 0 ? 1 : offs, offs + len - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);

			if (ex != NULL) {
				xb->xb_mem_bus_space->bus_base = base - offs;
				xb->xb_mem_bus_space->_space_map =
				    xbridge_space_map_mem;
				xb->xb_mem_bus_space->_space_subregion =
				    xbridge_space_region_mem;

				xb->xb_memstart = offs;
				xb->xb_memend = offs + len - 1;
			}
		}
	}

	if (ex != NULL) {
		/*
		 * Remove the devio mapping range from the extent
		 * to avoid ambiguous mappings.
		 *
		 * Note that xbow_widget_map_space() may have returned
		 * a range in which the devio area does not appear.
		 */
		start = xb->xb_devio_skew << 24;
		end = (xb->xb_devio_skew + 1) << 24;

		if (end >= ex->ex_start && start <= ex->ex_end) {
			if (start < ex->ex_start)
				start = ex->ex_start;
			if (end > ex->ex_end + 1)
				end = ex->ex_end + 1;
			if (extent_alloc_region(ex, start, end - start,
			    EX_NOWAIT | EX_MALLOCOK) != 0) {
				printf("%s: failed to expurge devio range"
				    " from %s large extent\n",
				    DEVNAME(xb), io ? "i/o" : "mem");
				extent_destroy(ex);
				ex = NULL;
			}
		}
	}

	return ex;
}

/*
 * Flags returned by xbridge_resource_explore()
 */
#define	XR_IO		0x01	/* needs I/O mappings */
#define	XR_MEM		0x02	/* needs memory mappings */
#define	XR_IO_OFLOW_S	0x04	/* can't fit I/O in a short devio */
#define	XR_MEM_OFLOW_S	0x08	/* can't fit memory in a short devio */
#define	XR_IO_OFLOW	0x10	/* can't fit I/O in a large devio */
#define	XR_MEM_OFLOW	0x20	/* can't fit memory in a large devio */

int
xbridge_resource_explore(struct xbridge_bus *xb, pcitag_t tag,
    struct extent *ioex, struct extent *memex)
{
	pci_chipset_tag_t pc = &xb->xb_pc;
	pcireg_t bhlc, type, addr, mask;
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end, reg_rom;
	int rc = 0;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlc)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		reg_rom = PCI_ROM_REG;
		break;
	case 1:	/* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		reg_rom = 0;	/* 0x38 */
		break;
	case 2:	/* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		reg_rom = 0;
		break;
	default:
		return rc;
	}

	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, NULL, &size, NULL))
			continue;

		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			reg += 4;
			/* FALLTHROUGH */
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			if (memex != NULL) {
				rc |= XR_MEM;
				if (size > memex->ex_end - memex->ex_start)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (extent_alloc(memex, size, size,
				    0, 0, 0, &base) != 0)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (base >= BRIDGE_DEVIO_SHORT)
					rc |= XR_MEM_OFLOW_S;
			} else
				rc |= XR_MEM | XR_MEM_OFLOW | XR_MEM_OFLOW_S;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (ioex != NULL) {
				rc |= XR_IO;
				if (size > ioex->ex_end - ioex->ex_start)
					rc |= XR_IO_OFLOW | XR_IO_OFLOW_S;
				else if (extent_alloc(ioex, size, size,
				    0, 0, 0, &base) != 0)
					rc |= XR_IO_OFLOW | XR_IO_OFLOW_S;
				else if (base >= BRIDGE_DEVIO_SHORT)
					rc |= XR_IO_OFLOW_S;
			} else
				rc |= XR_IO | XR_IO_OFLOW | XR_IO_OFLOW_S;
			break;
		}
	}

	if (reg_rom != 0) {
		addr = pci_conf_read(pc, tag, reg_rom);
		pci_conf_write(pc, tag, reg_rom, ~PCI_ROM_ENABLE);
		mask = pci_conf_read(pc, tag, reg_rom);
		pci_conf_write(pc, tag, reg_rom, addr);
		size = PCI_ROM_SIZE(mask);

		if (size != 0) {
			if (memex != NULL) {
				rc |= XR_MEM;
				if (size > memex->ex_end - memex->ex_start)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (extent_alloc(memex, size, size,
				    0, 0, 0, &base) != 0)
					rc |= XR_MEM_OFLOW | XR_MEM_OFLOW_S;
				else if (base >= BRIDGE_DEVIO_SHORT)
					rc |= XR_MEM_OFLOW_S;
			} else
				rc |= XR_MEM | XR_MEM_OFLOW | XR_MEM_OFLOW_S;
		}
	}

	return rc;
}

void
xbridge_resource_manage(struct xbridge_bus *xb, pcitag_t tag,
    struct extent *ioex, struct extent *memex)
{
	pci_chipset_tag_t pc = &xb->xb_pc;
	pcireg_t bhlc, type, mask;
	bus_addr_t base;
	bus_size_t size;
	int reg, reg_start, reg_end, reg_rom;

	bhlc = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(bhlc)) {
	case 0:
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_END;
		reg_rom = PCI_ROM_REG;
		break;
	case 1:	/* PCI-PCI bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PPB_END;
		reg_rom = 0;	/* 0x38 */
		break;
	case 2:	/* PCI-CardBus bridge */
		reg_start = PCI_MAPREG_START;
		reg_end = PCI_MAPREG_PCB_END;
		reg_rom = 0;
		break;
	default:
		return;
	}

	for (reg = reg_start; reg < reg_end; reg += 4) {
		if (pci_mapreg_probe(pc, tag, reg, &type) == 0)
			continue;

		if (pci_mapreg_info(pc, tag, reg, type, &base, &size, NULL))
			continue;

		/*
		 * Note that we do not care about the existing BAR values,
		 * since these devices either have not been setup by ARCS
		 * or do not matter for early system setup (such as
		 * optional IOC3 PCI boards, which will get setup by
		 * ARCS but can be reinitialized as we see fit).
		 */
#ifdef DEBUG
		printf("bar %02x type %d base %p size %p",
		    reg, type, base, size);
#endif
		switch (type) {
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
			/*
			 * Since our mapping ranges are restricted to
			 * at most 30 bits, the upper part of the 64 bit
			 * BAR registers is always zero.
			 */
			pci_conf_write(pc, tag, reg + 4, 0);
			/* FALLTHROUGH */
		case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
			if (memex != NULL) {
				if (extent_alloc(memex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
			break;
		case PCI_MAPREG_TYPE_IO:
			if (ioex != NULL) {
				if (extent_alloc(ioex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
			break;
		}

#ifdef DEBUG
		printf(" setup at %p\n", base);
#endif
		pci_conf_write(pc, tag, reg, base);

		if (type == (PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT))
			reg += 4;
	}

	if (reg_rom != 0) {
		base = (bus_addr_t)pci_conf_read(pc, tag, reg_rom);
		pci_conf_write(pc, tag, reg_rom, ~PCI_ROM_ENABLE);
		mask = pci_conf_read(pc, tag, reg_rom);
		size = PCI_ROM_SIZE(mask);

		if (size != 0) {
#ifdef DEBUG
			printf("bar %02x type rom base %p size %p",
			    reg_rom, base, size);
#endif
			if (memex != NULL) {
				if (extent_alloc(memex, size, size, 0, 0, 0,
				    &base) != 0)
					base = 0;
			} else
				base = 0;
#ifdef DEBUG
			printf(" setup at %p\n", base);
#endif
		} else
			base = 0;

		/* ROM intentionally left disabled */
		pci_conf_write(pc, tag, reg_rom, base);
	}
}

void
xbridge_device_setup(struct xbridge_bus *xb, int dev, int nfuncs,
    uint32_t devio)
{
	pci_chipset_tag_t pc = &xb->xb_pc;
	int function;
	pcitag_t tag;
	pcireg_t id, csr;
	uint32_t baseio;
	int resources;
	int io_devio, mem_devio;
	struct extent *ioex, *memex;

	/*
	 * In a first step, we enumerate all the requested resources,
	 * and check if they could fit within devio mappings.
	 *
	 * If devio can't afford us the mappings we need, we'll
	 * try and allocate a large window.
	 */

	/*
	 * Allocate extents to use for devio mappings if necessary.
	 * This can fail; in that case we'll try to use a large mapping
	 * whenever possible, or silently fail to configure the device.
	 */
	if (xb->xb_ioex != NULL)
		ioex = NULL;
	else
		ioex = extent_create("xbridge_io",
		    0, BRIDGE_DEVIO_LARGE - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (xb->xb_memex != NULL)
		memex = NULL;
	else
		memex = extent_create("xbridge_mem",
		    0, BRIDGE_DEVIO_LARGE - 1,
		    M_DEVBUF, NULL, 0, EX_NOWAIT);

	resources = 0;
	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, 0, dev, function);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr &
		    ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE));

		resources |= xbridge_resource_explore(xb, tag, ioex, memex);
	}

	if (memex != NULL) {
		extent_destroy(memex);
		memex = NULL;
	}
	if (ioex != NULL) {
		extent_destroy(ioex);
		ioex = NULL;
	}

	/*
	 * In a second step, if resources can be mapped using devio slots,
	 * allocate them. Otherwise, or if we can't get a devio slot
	 * big enough for the resources we need to map, we'll need
	 * to get a large window mapping.
	 *
	 * Note that, on Octane, we try to avoid using devio whenever
	 * possible.
	 */

	io_devio = -1;
	if (ISSET(resources, XR_IO)) {
		if (!ISSET(resources, XR_IO_OFLOW) &&
		    (sys_config.system_type != SGI_OCTANE ||
		     xb->xb_ioex == NULL))
			io_devio = xbridge_allocate_devio(xb, dev,
			    ISSET(resources, XR_IO_OFLOW_S));
		if (io_devio >= 0) {
			baseio = (xb->xb_devio_skew << 24) |
			    PIC_DEVIO_OFFS(xb->xb_busno, io_devio);
			xbridge_set_devio(xb, io_devio, devio |
			    (baseio >> BRIDGE_DEVICE_BASE_SHIFT));

			ioex = extent_create("xbridge_io", baseio,
			    baseio + BRIDGE_DEVIO_SIZE(io_devio) - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);
		} else {
			/*
			 * Try to get a large window mapping if we don't
			 * have one already.
			 */
			if (xb->xb_ioex == NULL)
				xb->xb_ioex = xbridge_mapping_setup(xb, 1);
		}
	}

	mem_devio = -1;
	if (ISSET(resources, XR_MEM)) {
		if (!ISSET(resources, XR_MEM_OFLOW) &&
		    sys_config.system_type != SGI_OCTANE)
			mem_devio = xbridge_allocate_devio(xb, dev,
			    ISSET(resources, XR_MEM_OFLOW_S));
		if (mem_devio >= 0) {
			baseio = (xb->xb_devio_skew << 24) |
			    PIC_DEVIO_OFFS(xb->xb_busno, mem_devio);
			xbridge_set_devio(xb, mem_devio, devio |
			    BRIDGE_DEVICE_IO_MEM |
			    (baseio >> BRIDGE_DEVICE_BASE_SHIFT));

			memex = extent_create("xbridge_mem", baseio,
			    baseio + BRIDGE_DEVIO_SIZE(mem_devio) - 1,
			    M_DEVBUF, NULL, 0, EX_NOWAIT);
		} else {
			/*
			 * Try to get a large window mapping if we don't
			 * have one already.
			 */
			if (xb->xb_memex == NULL)
				xb->xb_memex = xbridge_mapping_setup(xb, 0);
		}
	}

	/*
	 * Finally allocate the resources proper and update the
	 * device BARs accordingly.
	 */

	for (function = 0; function < nfuncs; function++) {
		tag = pci_make_tag(pc, 0, dev, function);
		id = pci_conf_read(pc, tag, PCI_ID_REG);

		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID ||
		    PCI_VENDOR(id) == 0)
			continue;

		xbridge_resource_manage(xb, tag,
		    ioex != NULL ? ioex : xb->xb_ioex,
		    memex != NULL ?  memex : xb->xb_memex);
	}

	if (memex != NULL)
		extent_destroy(memex);
	if (ioex != NULL)
		extent_destroy(ioex);
}

int
xbridge_ppb_setup(void *cookie, pcitag_t tag, bus_addr_t *iostart,
    bus_addr_t *ioend, bus_addr_t *memstart, bus_addr_t *memend)
{
	struct xbridge_bus *xb = cookie;
	pci_chipset_tag_t pc = &xb->xb_pc;
	uint32_t base, devio;
	bus_size_t exsize;
	u_long exstart;
	int dev, devio_idx, tries;

	pci_decompose_tag(pc, tag, NULL, &dev, NULL);
	devio = xbridge_read_reg(xb, BRIDGE_DEVICE(dev));

	/*
	 * Since our caller computes resource needs starting at zero, we
	 * can ignore the start values when computing the amount of
	 * resources we'll need.
	 */

	exsize = *memend;
	*memstart = 0xffffffff;
	*memend = 0;
	if (exsize++ != 0) {
		/* try to allocate through a devio slot whenever possible... */
		if (exsize < BRIDGE_DEVIO_SHORT)
			devio_idx = xbridge_allocate_devio(xb, dev, 0);
		else if (exsize < BRIDGE_DEVIO_LARGE)
			devio_idx = xbridge_allocate_devio(xb, dev, 1);
		else
			devio_idx = -1;

		/* ...if it fails, try the large view.... */
		if (devio_idx < 0 && xb->xb_memex == NULL)
			xb->xb_memex = xbridge_mapping_setup(xb, 0);

		/* ...if it is not available, try to get a devio slot anyway. */
		if (devio_idx < 0 && xb->xb_memex == NULL) {
			if (exsize > BRIDGE_DEVIO_SHORT)
				devio_idx = xbridge_allocate_devio(xb, dev, 1);
			if (devio_idx < 0)
				devio_idx = xbridge_allocate_devio(xb, dev, 0);
		}

		if (devio_idx >= 0) {
			base = (xb->xb_devio_skew << 24) |
			    PIC_DEVIO_OFFS(xb->xb_busno, devio_idx);
			xbridge_set_devio(xb, devio_idx, devio |
			    BRIDGE_DEVICE_IO_MEM |
			    (base >> BRIDGE_DEVICE_BASE_SHIFT));
			*memstart = base;
			*memend = base + BRIDGE_DEVIO_SIZE(devio_idx) - 1;
		} else if (xb->xb_memex != NULL) {
			/*
			 * We know that the direct memory resource range fits
			 * within the 32 bit address space, and is limited to
			 * 30 bits, so our allocation, if successfull, will
			 * work as a 32 bit memory range.
			 */
			if (exsize < 1UL << 20)
				exsize = 1UL << 20;
			for (tries = 0; tries < 5; tries++) {
				if (extent_alloc(xb->xb_memex, exsize,
				    1UL << 20, 0, 0, EX_NOWAIT | EX_MALLOCOK,
				    &exstart) == 0) {
					*memstart = exstart;
					*memend = exstart + exsize - 1;
					break;
				}
				exsize >>= 1;
				if (exsize < 1UL << 20)
					break;
			}
		}
	}

	exsize = *ioend;
	*iostart = 0xffffffff;
	*ioend = 0;
	if (exsize++ != 0) {
		/* try to allocate through a devio slot whenever possible... */
		if (exsize < BRIDGE_DEVIO_SHORT)
			devio_idx = xbridge_allocate_devio(xb, dev, 0);
		else if (exsize < BRIDGE_DEVIO_LARGE)
			devio_idx = xbridge_allocate_devio(xb, dev, 1);
		else
			devio_idx = -1;

		/* ...if it fails, try the large view.... */
		if (devio_idx < 0 && xb->xb_ioex == NULL)
			xb->xb_ioex = xbridge_mapping_setup(xb, 1);

		/* ...if it is not available, try to get a devio slot anyway. */
		if (devio_idx < 0 && xb->xb_ioex == NULL) {
			if (exsize > BRIDGE_DEVIO_SHORT)
				devio_idx = xbridge_allocate_devio(xb, dev, 1);
			if (devio_idx < 0)
				devio_idx = xbridge_allocate_devio(xb, dev, 0);
		}

		if (devio_idx >= 0) {
			base = (xb->xb_devio_skew << 24) |
			    PIC_DEVIO_OFFS(xb->xb_busno, devio_idx);
			xbridge_set_devio(xb, devio_idx, devio |
			    (base >> BRIDGE_DEVICE_BASE_SHIFT));
			*iostart = base;
			*ioend = base + BRIDGE_DEVIO_SIZE(devio_idx) - 1;
		} else if (xb->xb_ioex != NULL) {
			/*
			 * We know that the direct I/O resource range fits
			 * within the 32 bit address space, so our allocation,
			 * if successfull, will work as a 32 bit i/o range.
			 */
			if (exsize < 1UL << 12)
				exsize = 1UL << 12;
			for (tries = 0; tries < 5; tries++) {
				if (extent_alloc(xb->xb_ioex, exsize,
				    1UL << 12, 0, 0, EX_NOWAIT | EX_MALLOCOK,
				    &exstart) == 0) {
					*iostart = exstart;
					*ioend = exstart + exsize - 1;
					break;
				}
				exsize >>= 1;
				if (exsize < 1UL << 12)
					break;
			}
		}
	}

	return 0;
}

#if NCARDBUS > 0

static struct rb_md_fnptr xbridge_rb_md_fn = {
	xbridge_rbus_space_map,
	xbridge_rbus_space_unmap
};

int
xbridge_rbus_space_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	return bus_space_map(t, addr, size, flags, bshp);
}

void
xbridge_rbus_space_unmap(bus_space_tag_t t, bus_space_handle_t h,
    bus_size_t size, bus_addr_t *addrp)
{
	bus_space_unmap(t, h, size);
	*addrp = h - t->bus_base;
}

void *
xbridge_rbus_parent_io(struct pci_attach_args *pa)
{
	struct extent *ex = pa->pa_ioex;
	bus_addr_t start, end;
	rbus_tag_t rb = NULL;

	/*
	 * We want to force I/O mappings to lie in the low 16 bits
	 * area.  This is mandatory for 16-bit pcmcia devices; and
	 * although 32-bit cardbus devices could use a larger range,
	 * the pccbb driver doesn't enable the large I/O windows.
	 */
	if (ex != NULL) {
		start = 0;
		end = 0x10000;
		if (start < ex->ex_start)
			start = ex->ex_start;
		if (end > ex->ex_end)
			end = ex->ex_end;

		if (start < end) {
			rb = rbus_new_root_share(pa->pa_iot, ex,
			    start, end - start, 0);
			if (rb != NULL)
				rb->rb_md = &xbridge_rb_md_fn;
		}
	}

	/*
	 * We are not allowed to return NULL. If we can't provide
	 * resources, return a valid body which will fail requests.
	 */
	if (rb == NULL)
		rb = rbus_new_body(pa->pa_iot, NULL, NULL, 0, 0, 0,
		    RBUS_SPACE_INVALID);

	return rb;
}

void *
xbridge_rbus_parent_mem(struct pci_attach_args *pa)
{
	struct xbridge_bus *xb = pa->pa_pc->pc_conf_v;
	struct extent *ex = pa->pa_memex;
	bus_addr_t start;
	rbus_tag_t rb = NULL;

	/*
	 * There is no restriction for the memory mappings,
	 * however we need to make sure these won't hit the
	 * devio range (for md_space_unmap to work correctly).
	 */
	if (ex != NULL) {
		start = (xb->xb_devio_skew + 1) << 24;
		if (start < ex->ex_start)
			start = ex->ex_start;

		if (start < ex->ex_end) {
			rb = rbus_new_root_share(pa->pa_memt, ex,
			    start, ex->ex_end - start, 0);
			if (rb != NULL)
				rb->rb_md = &xbridge_rb_md_fn;
		}
	}

	/*
	 * We are not allowed to return NULL. If we can't provide
	 * resources, return a valid body which will fail requests.
	 */
	if (rb == NULL)
		rb = rbus_new_body(pa->pa_iot, NULL, NULL, 0, 0, 0,
		    RBUS_SPACE_INVALID);

	return rb;
}

#endif	/* NCARDBUS > 0 */

int
xbridge_allocate_devio(struct xbridge_bus *xb, int dev, int wantlarge)
{
	pcireg_t id;

	/*
	 * If the preferred slot is available and matches the size requested,
	 * use it.
	 */

	if (xb->xb_devices[dev].devio == 0) {
		if (BRIDGE_DEVIO_SIZE(dev) >=
		    wantlarge ? BRIDGE_DEVIO_LARGE : BRIDGE_DEVIO_SHORT)
			return dev;
	}

	/*
	 * Otherwise pick the smallest available devio matching our size
	 * request.
	 */

	for (dev = 0; dev < xb->xb_nslots; dev++) {
		if (xb->xb_devices[dev].devio != 0)
			continue;	/* devio in use */

		id = xb->xb_devices[dev].id;
		if (PCI_VENDOR(id) != PCI_VENDOR_INVALID && PCI_VENDOR(id) != 0)
			continue;	/* devio to be used soon */

		if (BRIDGE_DEVIO_SIZE(dev) >=
		    wantlarge ? BRIDGE_DEVIO_LARGE : BRIDGE_DEVIO_SHORT)
			return dev;
	}

	return -1;
}

void
xbridge_set_devio(struct xbridge_bus *xb, int dev, uint32_t devio)
{
	xbridge_write_reg(xb, BRIDGE_DEVICE(dev), devio);
	(void)xbridge_read_reg(xb, WIDGET_TFLUSH);
	xb->xb_devices[dev].devio = devio;
#ifdef DEBUG
	printf("device %d: new devio %08x\n", dev, devio);
#endif
}
