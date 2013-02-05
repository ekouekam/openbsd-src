/*	$OpenBSD$ */
/*
 * Copyright (c) 2008,2013 joshua stein <jcs@openbsd.org>
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
#include <sys/proc.h>
#include <sys/workq.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <machine/apmvar.h>

#include "audio.h"
#include "wskbd.h"

struct acpiwmi_softc {
	struct device		 sc_dev;

	struct acpiec_softc     *sc_ec;
	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;
};

int	wmi_match(struct device *, void *, void *);
void	wmi_attach(struct device *, struct device *, void *);

#if NAUDIO > 0 && NWSKBD > 0
extern int wskbd_set_mixervolume(long dir, int out);
#endif

struct cfattach acpiwmi_ca = {
	sizeof(struct acpiwmi_softc), wmi_match, wmi_attach
};

struct cfdriver acpiwmi_cd = {
	NULL, "acpiwmi", DV_DULL
};

const char *acpiwmi_hids[] = { ACPI_DEV_WMI, 0 };

int
wmi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata *cf = match;

	return (acpi_matchhids(aa, acpiwmi_hids, cf->cf_driver->cd_name));
}

void
wmi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpiwmi_softc *sc = (struct acpiwmi_softc *)self;
	struct acpi_attach_args	*aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	printf("\n");

	//acpiasus_init(self);

	//aml_register_notify(sc->sc_devnode, aa->aaa_dev,
	//    acpiasus_notify, sc, ACPIDEV_NOPOLL);
}
