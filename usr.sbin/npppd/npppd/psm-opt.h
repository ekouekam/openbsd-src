/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef	PSM_H
#define	PSM_H	1

/**@file
 * <p>PPPステートマシンのオプションのネゴシエーション状態を保持、変更するための
 * マクロ。</p>
 * <p>
 * オプションは LCP、CCP など fsm から導出された型に保持する。
 * <pre>
 *	struct lcp {
 *		fsm fsm;
 *		struct {
 *			uint8_t pfc;
 *			uint8_t acfc;
 *		} opt;
 *	};</pre></p>
 * <p>
 * 使用例:
 * <pre>
 *	if (!psm_opt_is_accepted(_this->lcp, pcf)) {
 *		// LCP の Protocol Field Compression を accept している
 *	}</pre></p>
 * <p>
 * fsm ではなく、導出されたクラスに保持することにしたので、fsm という名前
 * は使わず psm のサフィックスを使った。</p>
 * $Id: psm-opt.h,v 1.1 2010/01/11 04:20:57 yasuoka Exp $
 */

#define	PSM_OPT_REQUEST_OURS		0x01
#define	PSM_OPT_ACCEPT_OURS		0x02
#define	PSM_OPT_REJECT_OURS		0x04
#define	PSM_OPT_ENABLED_OURS		0x08

#define	PSM_OPT_REQUEST_PEERS		0x10
#define	PSM_OPT_ACCEPT_PEERS		0x20
#define	PSM_OPT_REJECT_PEERS		0x40
#define	PSM_OPT_ENABLED_PEERS		0x80

#define	psm_peer_opt_is_requested(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_REQUEST_PEERS) != 0)
#define	psm_peer_opt_set_requested(psm, confopt, boolval)	\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_REQUEST_PEERS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_REQUEST_PEERS;	\
	}
#define	psm_opt_is_requested(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_REQUEST_OURS) != 0)
#define	psm_opt_set_requested(psm, confopt, boolval)		\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_REQUEST_OURS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_REQUEST_OURS;	\
	}
#define	psm_peer_opt_is_accepted(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_ACCEPT_PEERS) != 0)
#define	psm_peer_opt_set_accepted(psm, confopt, boolval)	\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_ACCEPT_PEERS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_ACCEPT_PEERS;	\
	}
#define	psm_opt_is_accepted(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_ACCEPT_OURS) != 0)
#define	psm_opt_set_accepted(psm, confopt, boolval)		\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_ACCEPT_OURS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_ACCEPT_OURS;	\
	}
#define	psm_peer_opt_is_rejected(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_REJECT_PEERS) != 0)
#define	psm_peer_opt_set_rejected(psm, confopt, boolval)	\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_REJECT_PEERS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_REJECT_PEERS;	\
	}
#define	psm_opt_is_rejected(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_REJECT_OURS) != 0)
#define	psm_opt_set_rejected(psm, confopt, boolval)		\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_REJECT_OURS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_REJECT_OURS;	\
	}
#define	psm_peer_opt_is_enabled(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_ENABLED_PEERS) != 0)
#define	psm_peer_opt_set_enabled(psm, confopt, boolval)	\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_ENABLED_PEERS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_ENABLED_PEERS;	\
	}
#define	psm_opt_is_enabled(psm, confopt)			\
	(((psm)->opt.confopt & PSM_OPT_ENABLED_OURS) != 0)
#define	psm_opt_set_enabled(psm, confopt, boolval)		\
	if ((boolval)) {					\
		(psm)->opt.confopt |= PSM_OPT_ENABLED_OURS;	\
	} else {						\
		(psm)->opt.confopt &= ~PSM_OPT_ENABLED_OURS;	\
	}
#endif
