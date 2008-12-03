/*	$OpenBSD: ieee80211_ioctl.c,v 1.27 2008/12/03 17:24:45 damien Exp $	*/
/*	$NetBSD: ieee80211_ioctl.c,v 1.15 2004/05/06 02:58:16 dyoung Exp $	*/

/*-
 * Copyright (c) 2001 Atsushi Onoe
 * Copyright (c) 2002, 2003 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * IEEE 802.11 ioctl support
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/proc.h>
#include <sys/tree.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

void	 ieee80211_node2req(struct ieee80211com *,
	    const struct ieee80211_node *, struct ieee80211_nodereq *);
void	 ieee80211_req2node(struct ieee80211com *,
	    const struct ieee80211_nodereq *, struct ieee80211_node *);

void
ieee80211_node2req(struct ieee80211com *ic, const struct ieee80211_node *ni,
    struct ieee80211_nodereq *nr)
{
	/* Node address and name information */
	IEEE80211_ADDR_COPY(nr->nr_macaddr, ni->ni_macaddr);
	IEEE80211_ADDR_COPY(nr->nr_bssid, ni->ni_bssid);
	nr->nr_nwid_len = ni->ni_esslen;
	bcopy(ni->ni_essid, nr->nr_nwid, IEEE80211_NWID_LEN);

	/* Channel and rates */
	nr->nr_channel = ieee80211_chan2ieee(ic, ni->ni_chan);
	nr->nr_chan_flags = ni->ni_chan->ic_flags;
	nr->nr_nrates = ni->ni_rates.rs_nrates;
	bcopy(ni->ni_rates.rs_rates, nr->nr_rates, IEEE80211_RATE_MAXSIZE);

	/* Node status information */
	nr->nr_rssi = (*ic->ic_node_getrssi)(ic, ni);
	nr->nr_max_rssi = ic->ic_max_rssi;
	bcopy(ni->ni_tstamp, nr->nr_tstamp, sizeof(nr->nr_tstamp));
	nr->nr_intval = ni->ni_intval;
	nr->nr_capinfo = ni->ni_capinfo;
	nr->nr_erp = ni->ni_erp;
	nr->nr_pwrsave = ni->ni_pwrsave;
	nr->nr_associd = ni->ni_associd;
	nr->nr_txseq = ni->ni_txseq;
	nr->nr_rxseq = ni->ni_rxseq;
	nr->nr_fails = ni->ni_fails;
	nr->nr_inact = ni->ni_inact;
	nr->nr_txrate = ni->ni_txrate;
	nr->nr_state = ni->ni_state;
	/* XXX RSN */

	/* Node flags */
	nr->nr_flags = 0;
	if (bcmp(nr->nr_macaddr, nr->nr_bssid, IEEE80211_ADDR_LEN) == 0)
		nr->nr_flags |= IEEE80211_NODEREQ_AP;
	if (ni == ic->ic_bss)
		nr->nr_flags |= IEEE80211_NODEREQ_AP_BSS;
}

void
ieee80211_req2node(struct ieee80211com *ic, const struct ieee80211_nodereq *nr,
    struct ieee80211_node *ni)
{
	/* Node address and name information */
	IEEE80211_ADDR_COPY(ni->ni_macaddr, nr->nr_macaddr);
	IEEE80211_ADDR_COPY(ni->ni_bssid, nr->nr_bssid);
	ni->ni_esslen = nr->nr_nwid_len;
	bcopy(nr->nr_nwid, ni->ni_essid, IEEE80211_NWID_LEN);

	/* Rates */
	ni->ni_rates.rs_nrates = nr->nr_nrates;
	bcopy(nr->nr_rates, ni->ni_rates.rs_rates, IEEE80211_RATE_MAXSIZE);

	/* Node information */
	ni->ni_intval = nr->nr_intval;
	ni->ni_capinfo = nr->nr_capinfo;
	ni->ni_erp = nr->nr_erp;
	ni->ni_pwrsave = nr->nr_pwrsave;
	ni->ni_associd = nr->nr_associd;
	ni->ni_txseq = nr->nr_txseq;
	ni->ni_rxseq = nr->nr_rxseq;
	ni->ni_fails = nr->nr_fails;
	ni->ni_inact = nr->nr_inact;
	ni->ni_txrate = nr->nr_txrate;
	ni->ni_state = nr->nr_state;
}

static int
ieee80211_ioctl_setnwkeys(struct ieee80211com *ic,
    const struct ieee80211_nwkey *nwkey)
{
	struct ieee80211_key *k;
	int error, i;

	if (!(ic->ic_caps & IEEE80211_C_WEP))
		return ENODEV;

	if (nwkey->i_wepon == IEEE80211_NWKEY_OPEN) {
		if (!(ic->ic_flags & IEEE80211_F_WEPON))
			return 0;
		ic->ic_flags &= ~IEEE80211_F_WEPON;
		return ENETRESET;
	}
	if (nwkey->i_defkid < 1 || nwkey->i_defkid > IEEE80211_WEP_NKID)
		return EINVAL;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keylen == 0 ||
		    nwkey->i_key[i].i_keydat == NULL)
			continue;	/* entry not set */
		if (nwkey->i_key[i].i_keylen > IEEE80211_KEYBUF_SIZE)
			return EINVAL;

		/* map wep key to ieee80211_key */
		k = &ic->ic_nw_keys[i];
		if (k->k_cipher != IEEE80211_CIPHER_NONE)
			(*ic->ic_delete_key)(ic, NULL, k);
		memset(k, 0, sizeof(*k));
		if (nwkey->i_key[i].i_keylen <= 5)
			k->k_cipher = IEEE80211_CIPHER_WEP40;
		else
			k->k_cipher = IEEE80211_CIPHER_WEP104;
		k->k_len = nwkey->i_key[i].i_keylen;
		k->k_flags = IEEE80211_KEY_GROUP | IEEE80211_KEY_TX;
		error = copyin(nwkey->i_key[i].i_keydat, k->k_key,
		    nwkey->i_key[i].i_keylen);
		if (error != 0)
			return error;
		if ((error = (*ic->ic_set_key)(ic, NULL, k)) != 0)
			return error;
	}

	ic->ic_def_txkey = nwkey->i_defkid - 1;
	ic->ic_flags |= IEEE80211_F_WEPON;

	return ENETRESET;
}

static int
ieee80211_ioctl_getnwkeys(struct ieee80211com *ic,
    struct ieee80211_nwkey *nwkey)
{
	struct ieee80211_key *k;
	int error, i;

	if (ic->ic_flags & IEEE80211_F_WEPON)
		nwkey->i_wepon = IEEE80211_NWKEY_WEP;
	else
		nwkey->i_wepon = IEEE80211_NWKEY_OPEN;

	nwkey->i_defkid = ic->ic_wep_txkey + 1;

	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		if (nwkey->i_key[i].i_keydat == NULL)
			continue;
		/* do not show any keys to non-root user */
		if ((error = suser(curproc, 0)) != 0)
			return error;
		k = &ic->ic_nw_keys[i];
		if (k->k_cipher != IEEE80211_CIPHER_WEP40 &&
		    k->k_cipher != IEEE80211_CIPHER_WEP104)
			nwkey->i_key[i].i_keylen = 0;
		else
			nwkey->i_key[i].i_keylen = k->k_len;
		error = copyout(k->k_key, nwkey->i_key[i].i_keydat,
		    nwkey->i_key[i].i_keylen);
		if (error != 0)
			return error;
	}
	return 0;
}

static int
ieee80211_ioctl_setwpaparms(struct ieee80211com *ic,
    const struct ieee80211_wpaparams *wpa)
{
	if (!(ic->ic_caps & IEEE80211_C_RSN))
		return ENODEV;

	if (!wpa->i_enabled) {
		if (!(ic->ic_flags & IEEE80211_F_RSNON))
			return 0;
		ic->ic_flags &= ~IEEE80211_F_RSNON;
		return ENETRESET;
	}

	ic->ic_rsnprotos = 0;
	if (wpa->i_protos & IEEE80211_WPA_PROTO_WPA1)
		ic->ic_rsnprotos |= IEEE80211_PROTO_WPA;
	if (wpa->i_protos & IEEE80211_WPA_PROTO_WPA2)
		ic->ic_rsnprotos |= IEEE80211_PROTO_RSN;
	if (ic->ic_rsnprotos == 0)	/* set to default (WPA+RSN) */
		ic->ic_rsnprotos = IEEE80211_PROTO_WPA | IEEE80211_PROTO_RSN;

	ic->ic_rsnakms = 0;
	if (wpa->i_akms & IEEE80211_WPA_AKM_PSK)
		ic->ic_rsnakms |=
		    IEEE80211_AKM_PSK | IEEE80211_AKM_SHA256_PSK;
	if (wpa->i_akms & IEEE80211_WPA_AKM_IEEE8021X)
		ic->ic_rsnakms |=
		    IEEE80211_AKM_8021X | IEEE80211_AKM_SHA256_8021X;
	if (ic->ic_rsnakms == 0)	/* set to default (PSK+802.1X) */
		ic->ic_rsnakms =
		    IEEE80211_AKM_PSK | IEEE80211_AKM_8021X |
		    IEEE80211_AKM_SHA256_PSK | IEEE80211_AKM_SHA256_8021X;

	if (wpa->i_groupcipher == IEEE80211_WPA_CIPHER_WEP40)
		ic->ic_rsngroupcipher = IEEE80211_CIPHER_WEP40;
	else if (wpa->i_groupcipher == IEEE80211_WPA_CIPHER_TKIP)
		ic->ic_rsngroupcipher = IEEE80211_CIPHER_TKIP;
	else if (wpa->i_groupcipher == IEEE80211_WPA_CIPHER_CCMP)
		ic->ic_rsngroupcipher = IEEE80211_CIPHER_CCMP;
	else if (wpa->i_groupcipher == IEEE80211_WPA_CIPHER_WEP104)
		ic->ic_rsngroupcipher = IEEE80211_CIPHER_WEP104;
	else  {	/* set to default */
		if (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)
			ic->ic_rsngroupcipher = IEEE80211_CIPHER_TKIP;
		else
			ic->ic_rsngroupcipher = IEEE80211_CIPHER_CCMP;
	}

	ic->ic_rsnciphers = 0;
	if (wpa->i_ciphers & IEEE80211_WPA_CIPHER_TKIP)
		ic->ic_rsnciphers |= IEEE80211_CIPHER_TKIP;
	if (wpa->i_ciphers & IEEE80211_WPA_CIPHER_CCMP)
		ic->ic_rsnciphers |= IEEE80211_CIPHER_CCMP;
	if (wpa->i_ciphers & IEEE80211_WPA_CIPHER_USEGROUP)
		ic->ic_rsnciphers = IEEE80211_CIPHER_USEGROUP;
	if (ic->ic_rsnciphers == 0)	/* set to default (TKIP+CCMP) */
		ic->ic_rsnciphers = IEEE80211_CIPHER_TKIP |
		    IEEE80211_CIPHER_CCMP;

	ic->ic_flags |= IEEE80211_F_RSNON;

	return ENETRESET;
}

static int
ieee80211_ioctl_getwpaparms(struct ieee80211com *ic,
    struct ieee80211_wpaparams *wpa)
{
	wpa->i_enabled = (ic->ic_flags & IEEE80211_F_RSNON) ? 1 : 0;

	wpa->i_protos = 0;
	if (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)
		wpa->i_protos |= IEEE80211_WPA_PROTO_WPA1;
	if (ic->ic_rsnprotos & IEEE80211_PROTO_RSN)
		wpa->i_protos |= IEEE80211_WPA_PROTO_WPA2;

	wpa->i_akms = 0;
	if (ic->ic_rsnakms &
	    (IEEE80211_AKM_PSK | IEEE80211_AKM_SHA256_PSK))
		wpa->i_akms |= IEEE80211_WPA_AKM_PSK;
	if (ic->ic_rsnakms &
	    (IEEE80211_AKM_8021X | IEEE80211_AKM_SHA256_8021X))
		wpa->i_akms |= IEEE80211_WPA_AKM_IEEE8021X;

	if (ic->ic_rsngroupcipher == IEEE80211_CIPHER_WEP40)
		wpa->i_groupcipher = IEEE80211_WPA_CIPHER_WEP40;
	else if (ic->ic_rsngroupcipher == IEEE80211_CIPHER_TKIP)
		wpa->i_groupcipher = IEEE80211_WPA_CIPHER_TKIP;
	else if (ic->ic_rsngroupcipher == IEEE80211_CIPHER_CCMP)
		wpa->i_groupcipher = IEEE80211_WPA_CIPHER_CCMP;
	else if (ic->ic_rsngroupcipher == IEEE80211_CIPHER_WEP104)
		wpa->i_groupcipher = IEEE80211_WPA_CIPHER_WEP104;
	else
		wpa->i_groupcipher = IEEE80211_WPA_CIPHER_NONE;

	wpa->i_ciphers = 0;
	if (ic->ic_rsnciphers & IEEE80211_CIPHER_TKIP)
		wpa->i_ciphers |= IEEE80211_WPA_CIPHER_TKIP;
	if (ic->ic_rsnciphers & IEEE80211_CIPHER_CCMP)
		wpa->i_ciphers |= IEEE80211_WPA_CIPHER_CCMP;
	if (ic->ic_rsnciphers & IEEE80211_CIPHER_USEGROUP)
		wpa->i_ciphers = IEEE80211_WPA_CIPHER_USEGROUP;

	return 0;
}

int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ieee80211com *ic = (void *)ifp;
	struct ifreq *ifr = (struct ifreq *)data;
	int i, error = 0;
	struct ieee80211_nwid nwid;
	struct ieee80211_wpapsk *psk;
	struct ieee80211_wmmparams *wmm;
	struct ieee80211_keyavail *ka;
	struct ieee80211_keyrun *kr;
	struct ieee80211_power *power;
	struct ieee80211_bssid *bssid;
	struct ieee80211chanreq *chanreq;
	struct ieee80211_channel *chan;
	struct ieee80211_txpower *txpower;
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	struct ieee80211_nodereq *nr, nrbuf;
	struct ieee80211_nodereq_all *na;
	struct ieee80211_node *ni;
	u_int32_t flags;

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &ic->ic_media, cmd);
		break;
	case SIOCS80211NWID:
		if ((error = suser(curproc, 0)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &nwid, sizeof(nwid))) != 0)
			break;
		if (nwid.i_len > IEEE80211_NWID_LEN) {
			error = EINVAL;
			break;
		}
		memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
		ic->ic_des_esslen = nwid.i_len;
		memcpy(ic->ic_des_essid, nwid.i_nwid, nwid.i_len);
		error = ENETRESET;
		break;
	case SIOCG80211NWID:
		memset(&nwid, 0, sizeof(nwid));
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			nwid.i_len = ic->ic_des_esslen;
			memcpy(nwid.i_nwid, ic->ic_des_essid, nwid.i_len);
			break;
		default:
			nwid.i_len = ic->ic_bss->ni_esslen;
			memcpy(nwid.i_nwid, ic->ic_bss->ni_essid, nwid.i_len);
			break;
		}
		error = copyout(&nwid, ifr->ifr_data, sizeof(nwid));
		break;
	case SIOCS80211NWKEY:
		if ((error = suser(curproc, 0)) != 0)
			break;
		error = ieee80211_ioctl_setnwkeys(ic, (void *)data);
		break;
	case SIOCG80211NWKEY:
		error = ieee80211_ioctl_getnwkeys(ic, (void *)data);
		break;
	case SIOCS80211WMMPARMS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		if (!(ic->ic_flags & IEEE80211_C_QOS)) {
			error = ENODEV;
			break;
		}
		wmm = (struct ieee80211_wmmparams *)data;
		if (wmm->i_enabled)
			ic->ic_flags |= IEEE80211_F_QOS;
		else
			ic->ic_flags &= ~IEEE80211_F_QOS;
		error = ENETRESET;
		break;
	case SIOCG80211WMMPARMS:
		wmm = (struct ieee80211_wmmparams *)data;
		wmm->i_enabled = (ic->ic_flags & IEEE80211_F_QOS) ? 1 : 0;
		break;
	case SIOCS80211WPAPARMS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		error = ieee80211_ioctl_setwpaparms(ic, (void *)data);
		break;
	case SIOCG80211WPAPARMS:
		error = ieee80211_ioctl_getwpaparms(ic, (void *)data);
		break;
	case SIOCS80211WPAPSK:
		if ((error = suser(curproc, 0)) != 0)
			break;
		psk = (struct ieee80211_wpapsk *)data;
		if (psk->i_enabled) {
			ic->ic_flags |= IEEE80211_F_PSK;
			memcpy(ic->ic_psk, psk->i_psk, sizeof(ic->ic_psk));
		} else {
			ic->ic_flags &= ~IEEE80211_F_PSK;
			memset(ic->ic_psk, 0, sizeof(ic->ic_psk));
		}
		error = ENETRESET;
		break;
	case SIOCG80211WPAPSK:
		psk = (struct ieee80211_wpapsk *)data;
		if (ic->ic_flags & IEEE80211_F_PSK) {
			psk->i_enabled = 1;
			/* do not show any keys to non-root user */
			if (suser(curproc, 0) != 0) {
				psk->i_enabled = 2;
				memset(psk->i_psk, 0, sizeof(psk->i_psk));
				break;	/* return ok but w/o key */
			}
			memcpy(psk->i_psk, ic->ic_psk, sizeof(psk->i_psk));
		} else
			psk->i_enabled = 0;
		break;
	case SIOCS80211KEYAVAIL:
		if ((error = suser(curproc, 0)) != 0)
			break;
		ka = (struct ieee80211_keyavail *)data;
		(void)ieee80211_pmksa_add(ic, IEEE80211_AKM_8021X,
		    ka->i_macaddr, ka->i_key, ka->i_lifetime);
		(void)ieee80211_pmksa_add(ic, IEEE80211_AKM_SHA256_8021X,
		    ka->i_macaddr, ka->i_key, ka->i_lifetime);
		break;
	case SIOCS80211KEYRUN:
		if ((error = suser(curproc, 0)) != 0)
			break;
		kr = (struct ieee80211_keyrun *)data;
		error = ieee80211_keyrun(ic, kr->i_macaddr);
		break;
	case SIOCS80211POWER:
		if ((error = suser(curproc, 0)) != 0)
			break;
		power = (struct ieee80211_power *)data;
		ic->ic_lintval = power->i_maxsleep;
		if (power->i_enabled != 0) {
			if ((ic->ic_caps & IEEE80211_C_PMGT) == 0)
				error = EINVAL;
			else if ((ic->ic_flags & IEEE80211_F_PMGTON) == 0) {
				ic->ic_flags |= IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		} else {
			if (ic->ic_flags & IEEE80211_F_PMGTON) {
				ic->ic_flags &= ~IEEE80211_F_PMGTON;
				error = ENETRESET;
			}
		}
		break;
	case SIOCG80211POWER:
		power = (struct ieee80211_power *)data;
		power->i_enabled = (ic->ic_flags & IEEE80211_F_PMGTON) ? 1 : 0;
		power->i_maxsleep = ic->ic_lintval;
		break;
	case SIOCS80211BSSID:
		if ((error = suser(curproc, 0)) != 0)
			break;
		bssid = (struct ieee80211_bssid *)data;
		if (IEEE80211_ADDR_EQ(bssid->i_bssid, empty_macaddr))
			ic->ic_flags &= ~IEEE80211_F_DESBSSID;
		else {
			ic->ic_flags |= IEEE80211_F_DESBSSID;
			IEEE80211_ADDR_COPY(ic->ic_des_bssid, bssid->i_bssid);
		}
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
#endif
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
			    !IEEE80211_ADDR_EQ(ic->ic_des_bssid,
			    ic->ic_bss->ni_bssid))
				error = ENETRESET;
			break;
		}
		break;
	case SIOCG80211BSSID:
		bssid = (struct ieee80211_bssid *)data;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
#ifndef IEEE80211_STA_ONLY
			if (ic->ic_opmode == IEEE80211_M_HOSTAP)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_myaddr);
			else
#endif
			if (ic->ic_flags & IEEE80211_F_DESBSSID)
				IEEE80211_ADDR_COPY(bssid->i_bssid,
				    ic->ic_des_bssid);
			else
				memset(bssid->i_bssid, 0, IEEE80211_ADDR_LEN);
			break;
		default:
			IEEE80211_ADDR_COPY(bssid->i_bssid,
			    ic->ic_bss->ni_bssid);
			break;
		}
		break;
	case SIOCS80211CHANNEL:
		if ((error = suser(curproc, 0)) != 0)
			break;
		chanreq = (struct ieee80211chanreq *)data;
		if (chanreq->i_channel == IEEE80211_CHAN_ANY)
			ic->ic_des_chan = IEEE80211_CHAN_ANYC;
		else if (chanreq->i_channel > IEEE80211_CHAN_MAX ||
		    isclr(ic->ic_chan_active, chanreq->i_channel)) {
			error = EINVAL;
			break;
		} else
			ic->ic_ibss_chan = ic->ic_des_chan =
			    &ic->ic_channels[chanreq->i_channel];
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			error = ENETRESET;
			break;
		default:
			if (ic->ic_opmode == IEEE80211_M_STA) {
				if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
				    ic->ic_bss->ni_chan != ic->ic_des_chan)
					error = ENETRESET;
			} else {
				if (ic->ic_bss->ni_chan != ic->ic_ibss_chan)
					error = ENETRESET;
			}
			break;
		}
		break;
	case SIOCG80211CHANNEL:
		chanreq = (struct ieee80211chanreq *)data;
		switch (ic->ic_state) {
		case IEEE80211_S_INIT:
		case IEEE80211_S_SCAN:
			if (ic->ic_opmode == IEEE80211_M_STA)
				chan = ic->ic_des_chan;
			else
				chan = ic->ic_ibss_chan;
			break;
		default:
			chan = ic->ic_bss->ni_chan;
			break;
		}
		chanreq->i_channel = ieee80211_chan2ieee(ic, chan);
		break;
#if 0
	case SIOCG80211ZSTATS:
#endif
	case SIOCG80211STATS:
		ifr = (struct ifreq *)data;
		copyout(&ic->ic_stats, ifr->ifr_data, sizeof (ic->ic_stats));
#if 0
		if (cmd == SIOCG80211ZSTATS)
			memset(&ic->ic_stats, 0, sizeof(ic->ic_stats));
#endif
		break;
	case SIOCS80211TXPOWER:
		if ((error = suser(curproc, 0)) != 0)
			break;
		txpower = (struct ieee80211_txpower *)data;
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0) {
			error = EINVAL;
			break;
		}
		if (!(IEEE80211_TXPOWER_MIN < txpower->i_val &&
			txpower->i_val < IEEE80211_TXPOWER_MAX)) {
			error = EINVAL;
			break;
		}
		ic->ic_txpower = txpower->i_val;
		error = ENETRESET;
		break;
	case SIOCG80211TXPOWER:
		txpower = (struct ieee80211_txpower *)data;
		if ((ic->ic_caps & IEEE80211_C_TXPMGT) == 0)
			error = EINVAL;
		else
			txpower->i_val = ic->ic_txpower;
		break;
	case SIOCSIFMTU:
		ifr = (struct ifreq *)data;
		if (!(IEEE80211_MTU_MIN <= ifr->ifr_mtu &&
		    ifr->ifr_mtu <= IEEE80211_MTU_MAX))
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCS80211SCAN:
		if ((error = suser(curproc, 0)) != 0)
			break;
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP)
			break;
#endif
		if ((ifp->if_flags & IFF_UP) == 0) {
			error = ENETDOWN;
			break;
		}
		if ((ic->ic_scan_lock & IEEE80211_SCAN_REQUEST) == 0) {
			if (ic->ic_scan_lock & IEEE80211_SCAN_LOCKED)
				ic->ic_scan_lock |= IEEE80211_SCAN_RESUME;
			ic->ic_scan_lock |= IEEE80211_SCAN_REQUEST;
			if (ic->ic_state != IEEE80211_S_SCAN)
				ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		}
		/* Let the userspace process wait for completion */
		error = tsleep(&ic->ic_scan_lock, PCATCH, "80211scan",
		    hz * IEEE80211_SCAN_TIMEOUT);
		break;
	case SIOCG80211NODE:
		nr = (struct ieee80211_nodereq *)data;
		ni = ieee80211_find_node(ic, nr->nr_macaddr);
		if (ni == NULL) {
			error = ENOENT;
			break;
		}
		ieee80211_node2req(ic, ni, nr);
		break;
	case SIOCS80211NODE:
		if ((error = suser(curproc, 0)) != 0)
			break;
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
			error = EINVAL;
			break;
		}
#endif
		nr = (struct ieee80211_nodereq *)data;

		ni = ieee80211_find_node(ic, nr->nr_macaddr);
		if (ni == NULL)
			ni = ieee80211_alloc_node(ic, nr->nr_macaddr);
		if (ni == NULL) {
			error = ENOENT;
			break;
		}

		if (nr->nr_flags & IEEE80211_NODEREQ_COPY)
			ieee80211_req2node(ic, nr, ni);
		break;
	case SIOCS80211DELNODE:
		if ((error = suser(curproc, 0)) != 0)
			break;
		nr = (struct ieee80211_nodereq *)data;
		ni = ieee80211_find_node(ic, nr->nr_macaddr);
		if (ni == NULL)
			error = ENOENT;
		else if (ni == ic->ic_bss)
			error = EPERM;
		else {
			if (ni->ni_state == IEEE80211_STA_COLLECT)
				break;

			/* Disassociate station. */
			if (ni->ni_state == IEEE80211_STA_ASSOC)
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DISASSOC,
				    IEEE80211_REASON_ASSOC_LEAVE);

			/* Deauth station. */
			if (ni->ni_state >= IEEE80211_STA_AUTH)
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_LEAVE);

			ieee80211_release_node(ic, ni);
		}
		break;
	case SIOCG80211ALLNODES:
		na = (struct ieee80211_nodereq_all *)data;
		na->na_nodes = i = 0;
		ni = RB_MIN(ieee80211_tree, &ic->ic_tree);
		while (ni && na->na_size >=
		    i + sizeof(struct ieee80211_nodereq)) {
			ieee80211_node2req(ic, ni, &nrbuf);
			error = copyout(&nrbuf, (caddr_t)na->na_node + i,
			    sizeof(struct ieee80211_nodereq));
			if (error)
				break;
			i += sizeof(struct ieee80211_nodereq);
			na->na_nodes++;
			ni = RB_NEXT(ieee80211_tree, &ic->ic_tree, ni);
		}
		break;
	case SIOCG80211FLAGS:
		flags = ic->ic_flags;
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode != IEEE80211_M_HOSTAP)
#endif
			flags &= ~IEEE80211_F_HOSTAPMASK;
		ifr->ifr_flags = flags >> IEEE80211_F_USERSHIFT;
		break;
	case SIOCS80211FLAGS:
		if ((error = suser(curproc, 0)) != 0)
			break;
		flags = (u_int32_t)ifr->ifr_flags << IEEE80211_F_USERSHIFT;
		if (
#ifndef IEEE80211_STA_ONLY
		    ic->ic_opmode != IEEE80211_M_HOSTAP &&
#endif
		    (flags & IEEE80211_F_HOSTAPMASK)) {
			error = EINVAL;
			break;
		}
		ic->ic_flags = (ic->ic_flags & ~IEEE80211_F_USERMASK) | flags;
		error = ENETRESET;
		break;
	default:
		error = ether_ioctl(ifp, &ic->ic_ac, cmd, data);
	}

	return error;
}
