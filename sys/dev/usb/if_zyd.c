/*	$OpenBSD: if_zyd.c,v 1.31 2006/10/22 12:52:03 damien Exp $	*/

/*-
 * Copyright (c) 2006 by Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 by Florian Stoehr <ich@florian-stoehr.de>
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
 * ZyDAS ZD1211/ZD1211B USB WLAN driver.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/endian.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_zydreg.h>

/*#ifdef USB_DEBUG*/
#define ZYD_DEBUG
/*#endif*/

#ifdef ZYD_DEBUG
#define DPRINTF(x)	do { if (zyddebug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (zyddebug > (n)) printf x; } while (0)
int zyddebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

/* various supported device vendors/products */
#define ZYD_ZD1211_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211 }
#define ZYD_ZD1211B_DEV(v, p)	\
	{ { USB_VENDOR_##v, USB_PRODUCT_##v##_##p }, ZYD_ZD1211B }
static const struct zyd_type {
	struct usb_devno	dev;
	uint8_t			rev;
#define ZYD_ZD1211	0
#define ZYD_ZD1211B	1
} zyd_devs[] = {
	ZYD_ZD1211_DEV(3COM2,		3CRUSB10075),
	ZYD_ZD1211_DEV(ABOCOM,		WL54),
	ZYD_ZD1211_DEV(ASUS,		WL159G),
	ZYD_ZD1211_DEV(BELKIN,		F5D7050C),
	ZYD_ZD1211_DEV(CYBERTAN,	TG54USB),
	ZYD_ZD1211_DEV(DRAYTEK,		VIGOR550),
	ZYD_ZD1211_DEV(PLANEX2,		GWUS54GZL),
	ZYD_ZD1211_DEV(PLANEX3,		GWUS54MINI),
	ZYD_ZD1211_DEV(SAGEM,		XG760A),
	ZYD_ZD1211_DEV(SITECOMEU,	WL113),
	ZYD_ZD1211_DEV(SWEEX,		ZD1211),
	ZYD_ZD1211_DEV(TEKRAM,		QUICKWLAN),
	ZYD_ZD1211_DEV(TEKRAM,		ZD1211),
	ZYD_ZD1211_DEV(TWINMOS,		G240),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB_A),
	ZYD_ZD1211_DEV(UMEDIA,		TEW429UB),
	ZYD_ZD1211_DEV(WISTRONNEWEB,	UR055G),
	ZYD_ZD1211_DEV(ZYDAS,		ZD1211),
	ZYD_ZD1211_DEV(ZYXEL,		ZYAIRG220),
};
#define zyd_lookup(v, p)	\
	((const struct zyd_type *)usb_lookup(zyd_devs, v, p))

USB_DECLARE_DRIVER_CLASS(zyd, DV_IFNET);

void		zyd_attachhook(void *);
int		zyd_complete_attach(struct zyd_softc *);
int		zyd_open_pipes(struct zyd_softc *);
void		zyd_close_pipes(struct zyd_softc *);
int		zyd_alloc_tx_list(struct zyd_softc *);
void		zyd_free_tx_list(struct zyd_softc *);
int		zyd_alloc_rx_list(struct zyd_softc *);
void		zyd_free_rx_list(struct zyd_softc *);
struct		ieee80211_node *zyd_node_alloc(struct ieee80211com *);
int		zyd_media_change(struct ifnet *);
void		zyd_next_scan(void *);
void		zyd_task(void *);
int		zyd_newstate(struct ieee80211com *, enum ieee80211_state, int);
int		zyd_cmd(struct zyd_softc *, uint16_t, const void *, int,
		    void *, int, u_int);
int		zyd_read16(struct zyd_softc *, uint16_t, uint16_t *);
int		zyd_read32(struct zyd_softc *, uint16_t, uint32_t *);
int		zyd_write16(struct zyd_softc *, uint16_t, uint16_t);
int		zyd_write32(struct zyd_softc *, uint16_t, uint32_t);
int		zyd_rfwrite(struct zyd_softc *, uint32_t);
void		zyd_lock_phy(struct zyd_softc *);
void		zyd_unlock_phy(struct zyd_softc *);
int		zyd_rfmd_init(struct zyd_rf *);
int		zyd_rfmd_switch_radio(struct zyd_rf *, int);
int		zyd_rfmd_set_channel(struct zyd_rf *, uint8_t);
int		zyd_al2230_init(struct zyd_rf *);
int		zyd_al2230_switch_radio(struct zyd_rf *, int);
int		zyd_al2230_set_channel(struct zyd_rf *, uint8_t);
int		zyd_rf_attach(struct zyd_softc *, uint8_t);
const char	*zyd_rf_name(uint8_t);
int		zyd_hw_init(struct zyd_softc *);
int		zyd_read_eeprom(struct zyd_softc *);
int		zyd_set_macaddr(struct zyd_softc *, const uint8_t *);
int		zyd_set_bssid(struct zyd_softc *, const uint8_t *);
int		zyd_switch_radio(struct zyd_softc *, int);
void		zyd_set_led(struct zyd_softc *, int, int);
int		zyd_set_rxfilter(struct zyd_softc *);
void		zyd_set_chan(struct zyd_softc *, struct ieee80211_channel *);
int		zyd_set_beacon_interval(struct zyd_softc *, int);
uint8_t		zyd_plcp_signal(int);
void		zyd_intr(usbd_xfer_handle, usbd_private_handle, usbd_status);
void		zyd_rx_data(struct zyd_softc *, const uint8_t *, uint16_t);
void		zyd_rxeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
void		zyd_txeof(usbd_xfer_handle, usbd_private_handle, usbd_status);
int		zyd_tx_data(struct zyd_softc *, struct mbuf *,
		    struct ieee80211_node *);
void		zyd_start(struct ifnet *);
void		zyd_watchdog(struct ifnet *);
int		zyd_ioctl(struct ifnet *, u_long, caddr_t);
int		zyd_init(struct ifnet *);
void		zyd_stop(struct ifnet *, int);
int		zyd_loadfirmware(struct zyd_softc *, u_char *, size_t);
void		zyd_iter_func(void *, struct ieee80211_node *);
void		zyd_amrr_timeout(void *);
void		zyd_newassoc(struct ieee80211com *, struct ieee80211_node *,
		    int);

/*
 * Supported rates for 802.11b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset zyd_rateset_11b = {
	4, { 2, 4, 11, 22 }
};

static const struct ieee80211_rateset zyd_rateset_11g =	{
	8, { 12, 18, 24, 36, 48, 72, 96, 108 }
};

USB_MATCH(zyd)
{
	USB_MATCH_START(zyd, uaa);

	if (!uaa->iface)
		return UMATCH_NONE;

	return (zyd_lookup(uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

void
zyd_attachhook(void *xsc)
{
	struct zyd_softc *sc = xsc;
	const char *fwname;
	u_char *fw;
	size_t size;
	int error;

	fwname = (sc->mac_rev == ZYD_ZD1211) ? "zd1211" : "zd1211b";
	if ((error = loadfirmware(fwname, &fw, &size)) != 0) {
		printf("%s: could not read firmware file %s (error=%d)\n",
		    USBDEVNAME(sc->sc_dev), fwname, error);
		return;
	}

	error = zyd_loadfirmware(sc, fw, size);
	free(fw, M_DEVBUF);
	if (error != 0) {
		printf("%s: could not load firmware (error=%d)\n",
		    USBDEVNAME(sc->sc_dev), error);
		return;
	}

	/* complete the attach process */
	if (zyd_complete_attach(sc) == 0)
		sc->attached = 1;
}

USB_ATTACH(zyd)
{
	USB_ATTACH_START(zyd, sc, uaa);
	char *devinfop;
	usb_device_descriptor_t* ddesc;

	sc->sc_udev = uaa->device;

	devinfop = usbd_devinfo_alloc(sc->sc_udev, 0);
	USB_ATTACH_SETUP;
	printf("%s: %s\n", USBDEVNAME(sc->sc_dev), devinfop);
	usbd_devinfo_free(devinfop);

	sc->mac_rev = zyd_lookup(uaa->vendor, uaa->product)->rev;

	ddesc = usbd_get_device_descriptor(sc->sc_udev);
	if (UGETW(ddesc->bcdDevice) < 0x4330) {
		printf("%s: device version mismatch: 0x%x "
		    "(only >= 43.30 supported)\n", USBDEVNAME(sc->sc_dev),
		    UGETW(ddesc->bcdDevice));
		USB_ATTACH_ERROR_RETURN;
	}

	if (rootvp == NULL)
		mountroothook_establish(zyd_attachhook, sc);
	else
		zyd_attachhook(sc);

	USB_ATTACH_SUCCESS_RETURN;
}

int
zyd_complete_attach(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	usbd_status error;
	int i;

	usb_init_task(&sc->sc_task, zyd_task, sc);
	timeout_set(&sc->scan_to, zyd_next_scan, sc);

	sc->amrr.amrr_min_success_threshold =  1;
	sc->amrr.amrr_max_success_threshold = 10;
	timeout_set(&sc->amrr_to, zyd_amrr_timeout, sc);

	error = usbd_set_config_no(sc->sc_udev, ZYD_CONFIG_NO, 1);
	if (error != 0) {
		printf("%s: setting config no failed\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	error = usbd_device2interface_handle(sc->sc_udev, ZYD_IFACE_INDEX,
	    &sc->sc_iface);
	if (error != 0) {
		printf("%s: getting interface handle failed\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	if ((error = zyd_open_pipes(sc)) != 0) {
		printf("%s: could not open pipes\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	if ((error = zyd_read_eeprom(sc)) != 0) {
		printf("%s: could not read EEPROM\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	if ((error = zyd_rf_attach(sc, sc->rf_rev)) != 0) {
		printf("%s: could not attach RF\n", USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	if ((error = zyd_hw_init(sc)) != 0) {
		printf("%s: hardware initialization failed\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	printf("%s: HMAC ZD1211%s, FW %02x.%02x, RF %s, PA %x, address %s\n",
	    USBDEVNAME(sc->sc_dev), (sc->mac_rev == ZYD_ZD1211) ? "": "B",
	    sc->fw_rev >> 8, sc->fw_rev & 0xff, zyd_rf_name(sc->rf_rev),
	    sc->pa_rev, ether_sprintf(ic->ic_myaddr));

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =
	    IEEE80211_C_MONITOR |	/* monitor mode supported */
	    IEEE80211_C_TXPMGT |	/* tx power management */
	    IEEE80211_C_SHPREAMBLE |	/* short preamble supported */
	    IEEE80211_C_WEP;		/* s/w WEP */

	/* set supported .11b and .11g rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = zyd_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = zyd_rateset_11g;

	/* set supported .11b and .11g channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = zyd_init;
	ifp->if_ioctl = zyd_ioctl;
	ifp->if_start = zyd_start;
	ifp->if_watchdog = zyd_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	memcpy(ifp->if_xname, USBDEVNAME(sc->sc_dev), IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = zyd_node_alloc;
	ic->ic_newassoc = zyd_newassoc;

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = zyd_newstate;
	ieee80211_media_init(ifp, zyd_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(ZYD_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(ZYD_TX_RADIOTAP_PRESENT);
#endif

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

fail:	return error;
}

USB_DETACH(zyd)
{
	USB_DETACH_START(zyd, sc);
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	s = splusb();

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_to);
	timeout_del(&sc->amrr_to);

	zyd_close_pipes(sc);

	if (!sc->attached) {
		splx(s);
		return 0;
	}

	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	zyd_free_rx_list(sc);
	zyd_free_tx_list(sc);

	sc->attached = 0;

	splx(s);

	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_udev,
	    USBDEV(sc->sc_dev));

	return 0;
}

int
zyd_open_pipes(struct zyd_softc *sc)
{
	usb_endpoint_descriptor_t *edesc;
	int isize;
	usbd_status error;

	/* interrupt in */
	edesc = usbd_get_endpoint_descriptor(sc->sc_iface, 0x83);
	if (edesc == NULL)
		return EINVAL;

	isize = UGETW(edesc->wMaxPacketSize);
	if (isize == 0)	/* should not happen */
		return EINVAL;

	sc->ibuf = malloc(isize, M_USBDEV, M_NOWAIT);
	if (sc->ibuf == NULL)
		return ENOMEM;

	error = usbd_open_pipe_intr(sc->sc_iface, 0x83, USBD_SHORT_XFER_OK,
	    &sc->zyd_ep[ZYD_ENDPT_IIN], sc, sc->ibuf, isize, zyd_intr,
	    USBD_DEFAULT_INTERVAL);
	if (error != 0) {
		printf("%s: open rx intr pipe failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* interrupt out (not necessarily an interrupt pipe) */
	error = usbd_open_pipe(sc->sc_iface, 0x04, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_IOUT]);
	if (error != 0) {
		printf("%s: open tx intr pipe failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* bulk in */
	error = usbd_open_pipe(sc->sc_iface, 0x82, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BIN]);
	if (error != 0) {
		printf("%s: open rx pipe failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	/* bulk out */
	error = usbd_open_pipe(sc->sc_iface, 0x01, USBD_EXCLUSIVE_USE,
	    &sc->zyd_ep[ZYD_ENDPT_BOUT]);
	if (error != 0) {
		printf("%s: open tx pipe failed: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		goto fail;
	}

	return 0;

fail:	zyd_close_pipes(sc);
	return error;
}

void
zyd_close_pipes(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_ENDPT_CNT; i++) {
		if (sc->zyd_ep[i] != NULL) {
			usbd_abort_pipe(sc->zyd_ep[i]);
			usbd_close_pipe(sc->zyd_ep[i]);
			sc->zyd_ep[i] = NULL;
		}
	}
	if (sc->ibuf != NULL) {
		free(sc->ibuf, M_USBDEV);
		sc->ibuf = NULL;
	}
}

int
zyd_alloc_tx_list(struct zyd_softc *sc)
{
	int i, error;

	sc->tx_queued = 0;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate tx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYD_MAX_TXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate tx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}

		/* clear Tx descriptor */
		bzero(data->buf, sizeof (struct zyd_tx_desc));
	}
	return 0;

fail:	zyd_free_tx_list(sc);
	return error;
}

void
zyd_free_tx_list(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	for (i = 0; i < ZYD_TX_LIST_CNT; i++) {
		struct zyd_tx_data *data = &sc->tx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
		if (data->ni != NULL) {
			ieee80211_release_node(ic, data->ni);
			data->ni = NULL;
		}
	}
}

int
zyd_alloc_rx_list(struct zyd_softc *sc)
{
	int i, error;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		data->sc = sc;	/* backpointer for callbacks */

		data->xfer = usbd_alloc_xfer(sc->sc_udev);
		if (data->xfer == NULL) {
			printf("%s: could not allocate rx xfer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
		data->buf = usbd_alloc_buffer(data->xfer, ZYX_MAX_RXBUFSZ);
		if (data->buf == NULL) {
			printf("%s: could not allocate rx buffer\n",
			    USBDEVNAME(sc->sc_dev));
			error = ENOMEM;
			goto fail;
		}
	}
	return 0;

fail:	zyd_free_rx_list(sc);
	return error;
}

void
zyd_free_rx_list(struct zyd_softc *sc)
{
	int i;

	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		if (data->xfer != NULL) {
			usbd_free_xfer(data->xfer);
			data->xfer = NULL;
		}
	}
}

struct ieee80211_node *
zyd_node_alloc(struct ieee80211com *ic)
{
	struct zyd_node *zn;

	zn = malloc(sizeof (struct zyd_node), M_DEVBUF, M_NOWAIT);
	if (zn != NULL)
		bzero(zn, sizeof (struct zyd_node));
	return (struct ieee80211_node *)zn;
}

int
zyd_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		zyd_init(ifp);

	return 0;
}

/*
 * This function is called periodically (every 200ms) during scanning to
 * switch from one channel to another.
 */
void
zyd_next_scan(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
}

void
zyd_task(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	enum ieee80211_state ostate;

	ostate = ic->ic_state;

	switch (sc->sc_state) {
	case IEEE80211_S_INIT:
		if (ostate == IEEE80211_S_RUN) {
			/* turn link LED off */
			zyd_set_led(sc, ZYD_LED1, 1);

			/* stop data LED from blinking */
			zyd_write32(sc, sc->fwbase + ZYD_FW_LINK_STATUS, 0);
		}
		break;

	case IEEE80211_S_SCAN:
		zyd_set_chan(sc, ic->ic_bss->ni_chan);
		timeout_add(&sc->scan_to, hz / 5);
		break;

	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		zyd_set_chan(sc, ic->ic_bss->ni_chan);
		break;

	case IEEE80211_S_RUN:
	{
		struct ieee80211_node *ni = ic->ic_bss;

		zyd_set_chan(sc, ni->ni_chan);

		if (ic->ic_opmode != IEEE80211_M_MONITOR) {
			/* turn link LED on */
			zyd_set_led(sc, ZYD_LED1, 1);

			/* make data LED blink upon Tx */
			zyd_write32(sc, sc->fwbase + ZYD_FW_LINK_STATUS, 1);

			zyd_set_bssid(sc, ni->ni_bssid);
		}

		if (ic->ic_opmode == IEEE80211_M_STA) {
			/* fake a join to init the tx rate */
			zyd_newassoc(ic, ni, 1);
		}

		/* start automatic rate control timer */
		if (ic->ic_fixed_rate == -1)
			timeout_add(&sc->amrr_to, hz);

		break;
	}
	}

	sc->sc_newstate(ic, sc->sc_state, sc->sc_arg);
}

int
zyd_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct zyd_softc *sc = ic->ic_softc;

	usb_rem_task(sc->sc_udev, &sc->sc_task);
	timeout_del(&sc->scan_to);
	timeout_del(&sc->amrr_to);

	/* do it in a process context */
	sc->sc_state = nstate;
	sc->sc_arg = arg;
	usb_add_task(sc->sc_udev, &sc->sc_task);

	return 0;
}

int
zyd_cmd(struct zyd_softc *sc, uint16_t code, const void *idata, int ilen,
    void *odata, int olen, u_int flags)
{
	usbd_xfer_handle xfer;
	struct zyd_cmd cmd;
	uint16_t xferflags;
	usbd_status error;
	int s;

	if ((xfer = usbd_alloc_xfer(sc->sc_udev)) == NULL)
		return ENOMEM;

	cmd.code = htole16(code);
	bcopy(idata, cmd.data, ilen);

	xferflags = USBD_FORCE_SHORT_XFER;
	if (!(flags & ZYD_CMD_FLAG_READ))
		xferflags |= USBD_SYNCHRONOUS;
	else
		s = splusb();

	sc->odata = odata;
	sc->olen  = olen;

	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_IOUT], 0, &cmd,
	    sizeof (uint16_t) + ilen, xferflags, ZYD_INTR_TIMEOUT, NULL);
	error = usbd_transfer(xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		if (flags & ZYD_CMD_FLAG_READ)
			splx(s);
		printf("%s: could not send command (error=%s)\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(error));
		(void)usbd_free_xfer(xfer);
		return EIO;
	}
	if (!(flags & ZYD_CMD_FLAG_READ)) {
		(void)usbd_free_xfer(xfer);
		return 0;	/* write: don't wait for reply */
	}
	/* wait at most one second for command reply */
	error = tsleep(sc, PCATCH, "zydcmd", hz);
	sc->odata = NULL;	/* in case answer is received too late */
	splx(s);

	(void)usbd_free_xfer(xfer);
	return error;
}

int
zyd_read16(struct zyd_softc *sc, uint16_t reg, uint16_t *val)
{
	struct zyd_pair tmp;
	int error;

	reg = htole16(reg);
	error = zyd_cmd(sc, ZYD_CMD_IORD, &reg, sizeof reg, &tmp, sizeof tmp,
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = letoh16(tmp.val);
	return error;
}

int
zyd_read32(struct zyd_softc *sc, uint16_t reg, uint32_t *val)
{
	struct zyd_pair tmp[2];
	uint16_t regs[2];
	int error;

	regs[0] = htole16(ZYD_REG32_HI(reg));
	regs[1] = htole16(ZYD_REG32_LO(reg));
	error = zyd_cmd(sc, ZYD_CMD_IORD, regs, sizeof regs, tmp, sizeof tmp,
	    ZYD_CMD_FLAG_READ);
	if (error == 0)
		*val = letoh16(tmp[0].val) << 16 | letoh16(tmp[1].val);
	return error;
}

int
zyd_write16(struct zyd_softc *sc, uint16_t reg, uint16_t val)
{
	struct zyd_pair pair;

	pair.reg = htole16(reg);
	pair.val = htole16(val);

	return zyd_cmd(sc, ZYD_CMD_IOWR, &pair, sizeof pair, NULL, 0, 0);
}

int
zyd_write32(struct zyd_softc *sc, uint16_t reg, uint32_t val)
{
	struct zyd_pair pair[2];

	pair[0].reg = htole16(ZYD_REG32_HI(reg));
	pair[0].val = htole16(val >> 16);
	pair[1].reg = htole16(ZYD_REG32_LO(reg));
	pair[1].val = htole16(val & 0xffff);

	return zyd_cmd(sc, ZYD_CMD_IOWR, pair, sizeof pair, NULL, 0, 0);
}

int
zyd_rfwrite(struct zyd_softc *sc, uint32_t val)
{
	struct zyd_rf *rf = &sc->sc_rf;
	struct zyd_rfwrite req;
	uint16_t cr203;
	int i;

	(void)zyd_read16(sc, ZYD_CR203, &cr203);
	cr203 &= ~(ZYD_RF_IF_LE | ZYD_RF_CLK | ZYD_RF_DATA);

	req.code  = htole16(2);
	req.width = htole16(rf->width);
	for (i = 0; i < rf->width; i++) {
		req.bit[i] = htole16(cr203);
		if (val & (1 << (rf->width - 1 - i)))
			req.bit[i] |= htole16(ZYD_RF_DATA);
	}
	return zyd_cmd(sc, ZYD_CMD_RFCFG, &req, 4 + 2 * rf->width, NULL, 0, 0);
}

void
zyd_lock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp &= ~ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

void
zyd_unlock_phy(struct zyd_softc *sc)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_MISC, &tmp);
	tmp |= ZYD_UNLOCK_PHY_REGS;
	(void)zyd_write32(sc, ZYD_MAC_MISC, tmp);
}

/*
 * RFMD RF methods.
 */
int
zyd_rfmd_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_RFMD_PHY;
	static const uint32_t rfini[] = ZYD_RFMD_RF;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init RFMD radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

int
zyd_rfmd_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR10, on ? 0x89 : 0x15);
	(void)zyd_write16(sc, ZYD_CR11, on ? 0x00 : 0x81);

	return 0;
}

int
zyd_rfmd_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2;
	} rfprog[] = ZYD_RFMD_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);

	return 0;
}

/*
 * AL2230 RF methods.
 */
int
zyd_al2230_init(struct zyd_rf *rf)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	struct zyd_softc *sc = rf->rf_sc;
	static const struct zyd_phy_pair phyini[] = ZYD_AL2230_PHY;
	static const uint32_t rfini[] = ZYD_AL2230_RF;
	int i, error;

	/* init RF-dependent PHY registers */
	for (i = 0; i < N(phyini); i++) {
		error = zyd_write16(sc, phyini[i].reg, phyini[i].val);
		if (error != 0)
			return error;
	}

	/* init AL2230 radio */
	for (i = 0; i < N(rfini); i++) {
		if ((error = zyd_rfwrite(sc, rfini[i])) != 0)
			return error;
	}
	return 0;
#undef N
}

int
zyd_al2230_switch_radio(struct zyd_rf *rf, int on)
{
	struct zyd_softc *sc = rf->rf_sc;

	(void)zyd_write16(sc, ZYD_CR11,  on ? 0x00 : 0x04);
	(void)zyd_write16(sc, ZYD_CR251, on ? 0x3f : 0x2f);

	return 0;
}

int
zyd_al2230_set_channel(struct zyd_rf *rf, uint8_t chan)
{
	struct zyd_softc *sc = rf->rf_sc;
	static const struct {
		uint32_t	r1, r2, r3;
	} rfprog[] = ZYD_AL2230_CHANTABLE;

	(void)zyd_rfwrite(sc, rfprog[chan - 1].r1);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r2);
	(void)zyd_rfwrite(sc, rfprog[chan - 1].r3);

	(void)zyd_write16(sc, ZYD_CR138, 0x28);
	(void)zyd_write16(sc, ZYD_CR203, 0x06);

	return 0;
}

int
zyd_rf_attach(struct zyd_softc *sc, uint8_t type)
{
	struct zyd_rf *rf = &sc->sc_rf;

	rf->rf_sc = sc;

	switch (type) {
	case ZYD_RF_RFMD:
		rf->init         = zyd_rfmd_init;
		rf->switch_radio = zyd_rfmd_switch_radio;
		rf->set_channel  = zyd_rfmd_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	case ZYD_RF_AL2230:
		rf->init         = zyd_al2230_init;
		rf->switch_radio = zyd_al2230_switch_radio;
		rf->set_channel  = zyd_al2230_set_channel;
		rf->width        = 24;	/* 24-bit RF values */
		break;
	default:
		printf("%s: sorry, radio \"%s\" is not supported yet\n",
		    USBDEVNAME(sc->sc_dev), zyd_rf_name(type));
		return EINVAL;
	}
	return 0;
}

const char *
zyd_rf_name(uint8_t type)
{
	static const char * const zyd_rfs[] = {
		"unknown", "unknown", "UW2451",   "UCHIP",     "AL2230",
		"AL7230B", "THETA",   "AL2210",   "MAXIM_NEW", "GCT",
		"PV2000",  "RALINK",  "INTERSIL", "RFMD",      "MAXIM_NEW2",
		"PHILIPS"
	};
	return zyd_rfs[(type > 15) ? 0 : type];
}

int
zyd_hw_init(struct zyd_softc *sc)
{
#define N(a)	(sizeof (a) / sizeof ((a)[0]))
	static const struct zyd_phy_pair zyd_def_phy[] = ZYD_DEF_PHY;
	static const struct zyd_mac_pair zyd_def_mac[] = ZYD_DEF_MAC;
	struct zyd_rf *rf = &sc->sc_rf;
	int i, error;

	/* specify that the plug and play is finished */
	(void)zyd_write32(sc, ZYD_MAC_AFTER_PNP, 1);

	(void)zyd_read16(sc, ZYD_FIRMWARE_BASE_ADDR, &sc->fwbase);
	DPRINTF(("firmware base address=0x%04x\n", sc->fwbase));

	/* retrieve firmware revision number */
	(void)zyd_read16(sc, sc->fwbase + ZYD_FW_FIRMWARE_REV, &sc->fw_rev);

	(void)zyd_write32(sc, ZYD_CR_GPI_EN, 0);
	(void)zyd_write32(sc, ZYD_MAC_CONT_WIN_LIMIT, 0x7f043f);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	/* PHY init */
	zyd_lock_phy(sc);
	for (i = 0; i < N(zyd_def_phy); i++) {
		error = zyd_write16(sc, zyd_def_phy[i].reg,
		    zyd_def_phy[i].val);
		if (error != 0)
			goto fail;
	}
	zyd_unlock_phy(sc);

	/* HMAC init */
	for (i = 0; i < N(zyd_def_mac); i++) {
		error = zyd_write32(sc, zyd_def_mac[i].reg,
		    zyd_def_mac[i].val);
		if (error != 0)
			goto fail;
	}

	/* RF chip init */
	zyd_lock_phy(sc);
	error = (*rf->init)(rf);
	zyd_unlock_phy(sc);
	if (error != 0) {
		printf("%s: radio initialization failed\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/* init beacon interval to 100ms */
	if ((error = zyd_set_beacon_interval(sc, 100)) != 0)
		goto fail;

fail:	return error;
#undef N
}

int
zyd_read_eeprom(struct zyd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint32_t tmp;
	uint16_t val;
	int i;

	/* read MAC address */
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P1, &tmp);
	ic->ic_myaddr[0] = tmp & 0xff;
	ic->ic_myaddr[1] = tmp >>  8;
	ic->ic_myaddr[2] = tmp >> 16;
	ic->ic_myaddr[3] = tmp >> 24;
	(void)zyd_read32(sc, ZYD_EEPROM_MAC_ADDR_P2, &tmp);
	ic->ic_myaddr[4] = tmp & 0xff;
	ic->ic_myaddr[5] = tmp >>  8;

	(void)zyd_read32(sc, ZYD_EEPROM_POD, &tmp);
	sc->rf_rev = tmp & 0x0f;
	sc->pa_rev = (tmp >> 16) & 0x0f;

	/* read regulatory domain (currently unused) */
	(void)zyd_read32(sc, ZYD_EEPROM_SUBID, &tmp);
	sc->regdomain = tmp >> 16;
	DPRINTF(("regulatory domain %x\n", sc->regdomain));

	/* read Tx power calibration tables */
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_PWR_CAL + i, &val);
		sc->pwr_cal[i * 2] = val >> 8;
		sc->pwr_cal[i * 2 + 1] = val & 0xff;
	}
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_PWR_INT + i, &val);
		sc->pwr_int[i * 2] = val >> 8;
		sc->pwr_int[i * 2 + 1] = val & 0xff;
	}
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_36M_CAL + i, &val);
		sc->ofdm36_cal[i * 2] = val >> 8;
		sc->ofdm36_cal[i * 2 + 1] = val & 0xff;
	}
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_48M_CAL + i, &val);
		sc->ofdm48_cal[i * 2] = val >> 8;
		sc->ofdm48_cal[i * 2 + 1] = val & 0xff;
	}
	for (i = 0; i < 7; i++) {
		(void)zyd_read16(sc, ZYD_EEPROM_54M_CAL + i, &val);
		sc->ofdm54_cal[i * 2] = val >> 8;
		sc->ofdm54_cal[i * 2 + 1] = val & 0xff;
	}
	return 0;
}

int
zyd_set_macaddr(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_MACADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_MACADRH, tmp);

	return 0;
}

int
zyd_set_bssid(struct zyd_softc *sc, const uint8_t *addr)
{
	uint32_t tmp;

	tmp = addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRL, tmp);

	tmp = addr[5] << 8 | addr[4];
	(void)zyd_write32(sc, ZYD_MAC_BSSADRH, tmp);

	return 0;
}

int
zyd_switch_radio(struct zyd_softc *sc, int on)
{
	struct zyd_rf *rf = &sc->sc_rf;
	int error;

	zyd_lock_phy(sc);
	error = (*rf->switch_radio)(rf, on);
	zyd_unlock_phy(sc);

	return error;
}

void
zyd_set_led(struct zyd_softc *sc, int which, int on)
{
	uint32_t tmp;

	(void)zyd_read32(sc, ZYD_MAC_TX_PE_CONTROL, &tmp);
	tmp &= ~which;
	if (on)
		tmp |= which;
	(void)zyd_write32(sc, ZYD_MAC_TX_PE_CONTROL, tmp);
}

int
zyd_set_rxfilter(struct zyd_softc *sc)
{
	uint32_t rxfilter;

	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_STA:
		rxfilter = ZYD_FILTER_BSS;
		break;
	case IEEE80211_M_IBSS:
	case IEEE80211_M_HOSTAP:
		rxfilter = ZYD_FILTER_HOSTAP;
		break;
	case IEEE80211_M_MONITOR:
		rxfilter = ZYD_FILTER_MONITOR;
		break;
	default:
		/* should not get there */
		return EINVAL;
	}
	return zyd_write32(sc, ZYD_MAC_RXFILTER, rxfilter);
}

void
zyd_set_chan(struct zyd_softc *sc, struct ieee80211_channel *c)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct zyd_rf *rf = &sc->sc_rf;
	u_int chan;

	chan = ieee80211_chan2ieee(ic, c);
	if (chan == 0 || chan == IEEE80211_CHAN_ANY)
		return;

	zyd_lock_phy(sc);

	(*rf->set_channel)(rf, chan);

	/* update Tx power */
	(void)zyd_write32(sc, ZYD_CR31, sc->pwr_int[chan - 1]);
	(void)zyd_write32(sc, ZYD_CR68, sc->pwr_cal[chan - 1]);

	zyd_unlock_phy(sc);
}

int
zyd_set_beacon_interval(struct zyd_softc *sc, int bintval)
{
	/* XXX this is probably broken.. */
	(void)zyd_write32(sc, ZYD_CR_ATIM_WND_PERIOD, bintval - 2);
	(void)zyd_write32(sc, ZYD_CR_PRE_TBTT,        bintval - 1);
	(void)zyd_write32(sc, ZYD_CR_BCN_INTERVAL,    bintval);

	return 0;
}

uint8_t
zyd_plcp_signal(int rate)
{
	switch (rate) {
	/* CCK rates (returned values are device-dependent) */
	case 2:		return 0x0;
	case 4:		return 0x1;
	case 11:	return 0x2;
	case 22:	return 0x3;

	/* OFDM rates (cf IEEE Std 802.11a-1999, pp. 14 Table 80) */
	case 12:	return 0xb;
	case 18:	return 0xf;
	case 24:	return 0xa;
	case 36:	return 0xe;
	case 48:	return 0x9;
	case 72:	return 0xd;
	case 96:	return 0x8;
	case 108:	return 0xc;

	/* unsupported rates (should not get there) */
	default:	return 0xff;
	}
}

void
zyd_intr(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_softc *sc = (struct zyd_softc *)priv;
	const struct zyd_cmd *cmd;
	uint32_t len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_IIN]);
		}
		return;
	}

	cmd = (const struct zyd_cmd *)sc->ibuf;

	if (letoh16(cmd->code) == ZYD_NOTIF_RETRYSTATUS) {
		struct zyd_notif_retry *retry =
		    (struct zyd_notif_retry *)cmd->data;
		struct ieee80211com *ic = &sc->sc_ic;
		struct ifnet *ifp = &ic->ic_if;
		struct ieee80211_node *ni;

		DPRINTF(("retry intr: rate=0x%x addr=%s count=%d (0x%x)\n",
		    letoh16(retry->rate), ether_sprintf(retry->macaddr),
		    letoh16(retry->count) & 0xff, letoh16(retry->count)));

		/*
		 * Find the node to which the packet was sent and update its
		 * retry statistics.  In BSS mode, this node is the AP we're
		 * associated to so no lookup is actually needed.
		 */
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(ic, retry->macaddr);
			if (ni == NULL)
				return;	/* just ignore */
		} else
			ni = ic->ic_bss;

		((struct zyd_node *)ni)->amn.amn_retrycnt++;

		if (letoh16(retry->count) & 0x100)
			ifp->if_oerrors++;	/* too many retries */

	} else if (letoh16(cmd->code) == ZYD_NOTIF_IORD) {
		if (letoh16(*(uint16_t *)cmd->data) == ZYD_CR_INTERRUPT)
			return;	/* HMAC interrupt */

		if (sc->odata == NULL)
			return;	/* unexpected IORD notification */

		/* copy answer into caller-supplied buffer */
		usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);
		bcopy(cmd->data, sc->odata, sc->olen);

		wakeup(sc);	/* wakeup caller */

	} else {
		printf("%s: unknown notification %x\n", USBDEVNAME(sc->sc_dev),
		    letoh16(cmd->code));
	}
}

void
zyd_rx_data(struct zyd_softc *sc, const uint8_t *buf, uint16_t len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	const struct zyd_plcphdr *plcp;
	const struct zyd_rx_stat *stat;
	struct mbuf *m;
	int rlen, s;

	if (len < ZYD_MIN_FRAGSZ) {
		printf("%s: frame too short (length=%d)\n",
		    USBDEVNAME(sc->sc_dev), len);
		ifp->if_ierrors++;
		return;
	}

	plcp = (const struct zyd_plcphdr *)buf;
	stat = (const struct zyd_rx_stat *)
	    (buf + len - sizeof (struct zyd_rx_stat));

	if (stat->flags & ZYD_RX_ERROR) {
		DPRINTF(("%s: RX status indicated error (%x)\n",
		    USBDEVNAME(sc->sc_dev), stat->flags));
		ifp->if_ierrors++;
		return;
	}

	/* compute actual frame length */
	rlen = len - sizeof (struct zyd_plcphdr) -
	    sizeof (struct zyd_rx_stat) - IEEE80211_CRC_LEN;

	/* allocate a mbuf to store the frame */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    USBDEVNAME(sc->sc_dev));
		ifp->if_ierrors++;
		return;
	}
	if (rlen > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if (!(m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    USBDEVNAME(sc->sc_dev));
			m_freem(m);
			ifp->if_ierrors++;
			return;
		}
	}
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = rlen;
	bcopy((const uint8_t *)(plcp + 1), mtod(m, uint8_t *), rlen);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct zyd_rx_radiotap_header *tap = &sc->sc_rxtap;
		static const uint8_t rates[] = {
			/* reverse function of zyd_plcp_signal() */
			2, 4, 11, 22, 0, 0, 0, 0,
			96, 48, 24, 12, 108, 72, 36, 18
		};

		tap->wr_flags = 0;
		tap->wr_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
		tap->wr_rssi = stat->rssi;
		tap->wr_rate = rates[plcp->signal & 0xf];

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
	}
#endif

	s = splnet();
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);
	ieee80211_input(ifp, m, ni, stat->rssi, 0);

	/* node is no longer needed */
	ieee80211_release_node(ic, ni);

	splx(s);
}

void
zyd_rxeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_rx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	const struct zyd_rx_desc *desc;
	int len;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall(sc->zyd_ep[ZYD_ENDPT_BIN]);

		goto skip;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	if (len < ZYD_MIN_RXBUFSZ) {
		printf("%s: xfer too short (length=%d)\n",
		    USBDEVNAME(sc->sc_dev), len);
		ifp->if_ierrors++;
		goto skip;
	}

	desc = (const struct zyd_rx_desc *)
	    (data->buf + len - sizeof (struct zyd_rx_desc));

	if (UGETW(desc->tag) == ZYD_TAG_MULTIFRAME) {
		const uint8_t *p = data->buf, *end = p + len;
		int i;

		DPRINTFN(3, ("received multi-frame transfer\n"));

		for (i = 0; i < ZYD_MAX_RXFRAMECNT; i++) {
			const uint16_t len = UGETW(desc->len[i]);

			if (len == 0 || p + len > end)
				break;

			zyd_rx_data(sc, p, len);
			/* next frame is aligned on a 32-bit boundary */
			p += (len + 3) & ~3;
		}
	} else {
		DPRINTFN(3, ("received single-frame transfer\n"));

		zyd_rx_data(sc, data->buf, len);
	}

skip:	/* setup a new transfer */
	usbd_setup_xfer(xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data, NULL,
	    ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
	    USBD_NO_TIMEOUT, zyd_rxeof);
	(void)usbd_transfer(xfer);
}

void
zyd_txeof(usbd_xfer_handle xfer, usbd_private_handle priv, usbd_status status)
{
	struct zyd_tx_data *data = priv;
	struct zyd_softc *sc = data->sc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	if (status != USBD_NORMAL_COMPLETION) {
		if (status == USBD_NOT_STARTED || status == USBD_CANCELLED)
			return;

		printf("%s: could not transmit buffer: %s\n",
		    USBDEVNAME(sc->sc_dev), usbd_errstr(status));

		if (status == USBD_STALLED) {
			usbd_clear_endpoint_stall_async(
			    sc->zyd_ep[ZYD_ENDPT_BOUT]);
		}
		ifp->if_oerrors++;
		return;
	}

	s = splnet();

	/* update rate control statistics */
	((struct zyd_node *)data->ni)->amn.amn_txcnt++;

	ieee80211_release_node(ic, data->ni);
	data->ni = NULL;

	sc->tx_queued--;
	ifp->if_opackets++;

	sc->tx_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;
	zyd_start(ifp);

	splx(s);
}

int
zyd_tx_data(struct zyd_softc *sc, struct mbuf *m0, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct zyd_tx_desc *desc;
	struct zyd_tx_data *data;
	struct ieee80211_frame *wh;
	int xferlen, totlen, rate;
	usbd_status error;

	wh = mtod(m0, struct ieee80211_frame *);

	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		m0 = ieee80211_wep_crypt(ifp, m0, 1);
		if (m0 == NULL)
			return ENOBUFS;

		/* packet header may have moved, reset our local pointer */
		wh = mtod(m0, struct ieee80211_frame *);
	}

	/* pickup a rate */
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		/* mgmt frames are sent at the lowest available bit-rate */
		rate = ni->ni_rates.rs_rates[0];
	} else {
		if (ic->ic_fixed_rate != -1) {
			rate = ic->ic_sup_rates[ic->ic_curmode].
			    rs_rates[ic->ic_fixed_rate];
		} else
			rate = ni->ni_rates.rs_rates[ni->ni_txrate];
	}
	rate &= IEEE80211_RATE_VAL;

	data = &sc->tx_data[0];
	desc = (struct zyd_tx_desc *)data->buf;

	data->ni = ni;

	xferlen = sizeof (struct zyd_tx_desc) + m0->m_pkthdr.len;
	totlen = m0->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* fill Tx descriptor */
	desc->len = htole16(totlen);

	desc->flags = ZYD_TX_FLAG_BACKOFF;
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		if (totlen > ic->ic_rtsthreshold ||
		    (ZYD_RATE_IS_OFDM(rate) &&
		    (ic->ic_flags & IEEE80211_F_USEPROT)))
			desc->flags |= ZYD_TX_FLAG_RTS;
	} else
		desc->flags |= ZYD_TX_FLAG_MULTICAST;

	if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL | IEEE80211_FC0_SUBTYPE_PS_POLL))
		desc->flags |= ZYD_TX_FLAG_TYPE(ZYD_TX_TYPE_PS_POLL);

	desc->phy = zyd_plcp_signal(rate);
	if (ZYD_RATE_IS_OFDM(rate)) {
		desc->phy |= ZYD_TX_PHY_OFDM;
		if (ic->ic_curmode == IEEE80211_MODE_11A)
			desc->phy |= ZYD_TX_PHY_5GHZ;
	} else if (rate != 2 && (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
		desc->phy |= ZYD_TX_PHY_SHPREAMBLE;

	/* actual transmit length (XXX why +10?) */
	desc->pktlen = htole16(sizeof (struct zyd_tx_desc) + totlen + 10);

	desc->plcp_length = (16 * totlen + rate - 1) / rate;
	desc->plcp_service = 0;
	if (rate == 22) {
		const int remainder = (16 * totlen) % 22;
		if (remainder != 0 && remainder < 7)
			desc->plcp_service |= ZYD_PLCP_LENGEXT;
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct zyd_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_rate = rate;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		M_DUP_PKTHDR(&mb, m0);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m0;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	m_copydata(m0, 0, m0->m_pkthdr.len,
	    data->buf + sizeof (struct zyd_tx_desc));

	DPRINTFN(10, ("%s: sending data frame len=%u rate=%u xferlen=%u\n",
	    USBDEVNAME(sc->sc_dev), m0->m_pkthdr.len, rate, xferlen));

	m_freem(m0);	/* mbuf no longer needed */

	usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BOUT], data,
	    data->buf, xferlen, USBD_FORCE_SHORT_XFER | USBD_NO_COPY,
	    ZYD_TX_TIMEOUT, zyd_txeof);
	error = usbd_transfer(data->xfer);
	if (error != USBD_IN_PROGRESS && error != 0) {
		ifp->if_oerrors++;
		return EIO;
	}
	sc->tx_queued++;

	return 0;
}

void
zyd_start(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m0;

	/*
	 * net80211 may still try to send management frames even if the
	 * IFF_RUNNING flag is not set...
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IF_POLL(&ic->ic_mgtq, m0);
		if (m0 != NULL) {
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IF_DEQUEUE(&ic->ic_mgtq, m0);

			ni = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
			m0->m_pkthdr.rcvif = NULL;
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (zyd_tx_data(sc, m0, ni) != 0)
				break;
		} else {
			if (ic->ic_state != IEEE80211_S_RUN)
				break;
			IFQ_POLL(&ifp->if_snd, m0);
			if (m0 == NULL)
				break;
			if (sc->tx_queued >= ZYD_TX_LIST_CNT) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
			IFQ_DEQUEUE(&ifp->if_snd, m0);
#if NBPFILTER > 0
			if (ifp->if_bpf != NULL)
				bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif
			if ((m0 = ieee80211_encap(ifp, m0, &ni)) == NULL) {
				ifp->if_oerrors++;
				continue;
			}
#if NBPFILTER > 0
			if (ic->ic_rawbpf != NULL)
				bpf_mtap(ic->ic_rawbpf, m0, BPF_DIRECTION_OUT);
#endif
			if (zyd_tx_data(sc, m0, ni) != 0) {
				if (ni != NULL)
					ieee80211_release_node(ic, ni);
				ifp->if_oerrors++;
				break;
			}
		}

		sc->tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
zyd_watchdog(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->tx_timer > 0) {
		if (--sc->tx_timer == 0) {
			printf("%s: device timeout\n", USBDEVNAME(sc->sc_dev));
			/* zyd_init(ifp); XXX needs a process context ? */
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
zyd_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&ic->ic_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				zyd_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				zyd_stop(ifp, 1);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);
		if (error == ENETRESET)
			error = 0;
		break;

	default:
		error = ieee80211_ioctl(ifp, command, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING | IFF_UP)) ==
		    (IFF_RUNNING | IFF_UP))
			zyd_init(ifp);
		error = 0;
	}

	splx(s);

	return error;
}

int
zyd_init(struct ifnet *ifp)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i, error;

	zyd_stop(ifp, 0);

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	DPRINTF(("setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = zyd_set_macaddr(sc, ic->ic_myaddr);
	if (error != 0)
		return error;

	/* we'll do software WEP decryption for now */
	DPRINTF(("setting encryption type\n"));
	error = zyd_write32(sc, ZYD_MAC_ENCRYPTION_TYPE, ZYD_ENC_SNIFFER);
	if (error != 0)
		return error;

	/* promiscuous mode */
	(void)zyd_write32(sc, ZYD_MAC_SNIFFER,
	    (ic->ic_opmode == IEEE80211_M_MONITOR) ? 1 : 0);

	(void)zyd_set_rxfilter(sc);

	/* switch radio transmitter ON */
	(void)zyd_switch_radio(sc, 1);

	/* set basic rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x3);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_BAS_RATE, 0x150f);

	/* set mandatory rates */
	if (ic->ic_curmode == IEEE80211_MODE_11B)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x0f);
	else if (ic->ic_curmode == IEEE80211_MODE_11A)
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x1500);
	else	/* assumes 802.11b/g */
		(void)zyd_write32(sc, ZYD_MAC_MAN_RATE, 0x150f);

	/* set default BSS channel */
	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	zyd_set_chan(sc, ic->ic_bss->ni_chan);

	/* enable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, ZYD_HWINT_MASK);

	/*
	 * Allocate Tx and Rx xfer queues.
	 */
	if ((error = zyd_alloc_tx_list(sc)) != 0) {
		printf("%s: could not allocate Tx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}
	if ((error = zyd_alloc_rx_list(sc)) != 0) {
		printf("%s: could not allocate Rx list\n",
		    USBDEVNAME(sc->sc_dev));
		goto fail;
	}

	/*
	 * Start up the receive pipe.
	 */
	for (i = 0; i < ZYD_RX_LIST_CNT; i++) {
		struct zyd_rx_data *data = &sc->rx_data[i];

		usbd_setup_xfer(data->xfer, sc->zyd_ep[ZYD_ENDPT_BIN], data,
		    NULL, ZYX_MAX_RXBUFSZ, USBD_NO_COPY | USBD_SHORT_XFER_OK,
		    USBD_NO_TIMEOUT, zyd_rxeof);
		error = usbd_transfer(data->xfer);
		if (error != USBD_IN_PROGRESS && error != 0) {
			printf("%s: could not queue Rx transfer\n",
			    USBDEVNAME(sc->sc_dev));
			goto fail;
		}
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

	return 0;

fail:	zyd_stop(ifp, 1);
	return error;
}

void
zyd_stop(struct ifnet *ifp, int disable)
{
	struct zyd_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	sc->tx_timer = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);	/* free all nodes */

	/* switch radio transmitter OFF */
	(void)zyd_switch_radio(sc, 0);

	/* disable Rx */
	(void)zyd_write32(sc, ZYD_MAC_RXFILTER, 0);

	/* disable interrupts */
	(void)zyd_write32(sc, ZYD_CR_INTERRUPT, 0);

	zyd_free_rx_list(sc);
	zyd_free_tx_list(sc);
}

int
zyd_loadfirmware(struct zyd_softc *sc, u_char *fw, size_t size)
{
	usb_device_request_t req;
	uint16_t addr;
	uint8_t stat;

	DPRINTF(("firmware size=%d\n", size));

	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADREQ;
	USETW(req.wIndex, 0);

	addr = ZYD_FIRMWARE_START_ADDR;
	while (size > 0) {
		const int mlen = min(size, 4096);

		DPRINTF(("loading firmware block: len=%d, addr=0x%x\n", mlen,
		    addr));

		USETW(req.wValue, addr);
		USETW(req.wLength, mlen);
		if (usbd_do_request(sc->sc_udev, &req, fw) != 0)
			return EIO;

		addr += mlen / 2;
		fw   += mlen;
		size -= mlen;
	}

	/* check whether the upload succeeded */
	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = ZYD_DOWNLOADSTS;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, sizeof stat);
	if (usbd_do_request(sc->sc_udev, &req, &stat) != 0)
		return EIO;

	return (stat & 0x80) ? EIO : 0;
}

void
zyd_iter_func(void *arg, struct ieee80211_node *ni)
{
	struct zyd_softc *sc = arg;
	struct zyd_node *zn = (struct zyd_node *)ni;

	ieee80211_amrr_choose(&sc->amrr, ni, &zn->amn);
}

void
zyd_amrr_timeout(void *arg)
{
	struct zyd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	int s;

	s = splnet();
	if (ic->ic_opmode == IEEE80211_M_STA)
		zyd_iter_func(sc, ic->ic_bss);
	else
		ieee80211_iterate_nodes(ic, zyd_iter_func, sc);
	splx(s);

	timeout_add(&sc->amrr_to, hz);
}

void
zyd_newassoc(struct ieee80211com *ic, struct ieee80211_node *ni, int isnew)
{
	struct zyd_softc *sc = ic->ic_softc;
	int i;

	ieee80211_amrr_node_init(&sc->amrr, &((struct zyd_node *)ni)->amn);

	/* set rate to some reasonable initial value */
	for (i = ni->ni_rates.rs_nrates - 1;
	     i > 0 && (ni->ni_rates.rs_rates[i] & IEEE80211_RATE_VAL) > 72;
	     i--);
	ni->ni_txrate = i;
}

int
zyd_activate(device_ptr_t self, enum devact act)
{
	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		/*if_deactivate(&sc->sc_ic.sc_if);*/
		break;
	}
	return 0;
}
