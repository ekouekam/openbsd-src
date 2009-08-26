/*	$OpenBSD: com_obio.c,v 1.2 2009/08/26 17:47:51 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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
#include <sys/device.h>
#include <sys/tty.h>

#include <machine/autoconf.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

int	com_obio_match(struct device *, void *, void *);
void	com_obio_attach(struct device *, struct device *, void *);

struct cfattach com_obio_ca = {
	sizeof(struct com_softc), com_obio_match, com_obio_attach
};

struct cfdriver com_obio_cd = {
	NULL, "com", DV_DULL
};

int
com_obio_match(struct device *parent, void *cfdata, void *aux)
{
	return (1);
}

void
com_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct com_softc *sc = (void *)self;
	struct obio_attach_args *oa = aux;

	sc->sc_iot = oa->oa_iot;
	sc->sc_iobase = oa->oa_offset;
	sc->sc_frequency = comconsfreq; /* XXX */

	if (sc->sc_iobase != comconsaddr) {
		if (bus_space_map(sc->sc_iot, sc->sc_iobase,
		    COM_NPORTS, 0, &sc->sc_ioh)) {
			printf(": can't map registers\n");
			return;
		}
	} else
		sc->sc_ioh = comconsioh;

	com_attach_subr(sc);

	intr_establish(oa->oa_ivec, IST_LEVEL, IPL_TTY, comintr,
	    sc, sc->sc_dev.dv_xname);
}
