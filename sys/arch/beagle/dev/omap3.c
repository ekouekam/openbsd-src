#include <sys/types.h>
#include <machine/bus.h>

#include <beagle/dev/omapvar.h>

#define PRCM_ADDR	0x48004000
#define PRCM_SIZE	0x2000

#define	INTC_ADDR	0x48200000
#define	INTC_SIZE	0x200

#define	GPTIMERx_SIZE	0x100
#define	GPTIMER1_ADDR	0x48318000
#define	GPTIMER1_IRQ	37
#define	GPTIMER2_ADDR	0x49032000
#define	GPTIMER2_IRQ	38

#define WD_ADDR		0x48314000
#define WD_SIZE		0x80

#define GPIOx_SIZE	0x1000
#define GPIO1_ADDR	0x48310000
#define GPIO2_ADDR	0x49050000
#define GPIO3_ADDR	0x49052000
#define GPIO4_ADDR	0x49054000
#define GPIO5_ADDR	0x49056000
#define GPIO6_ADDR	0x49058000

#define GPIO1_IRQ	29
#define GPIO2_IRQ	30
#define GPIO3_IRQ	31
#define GPIO4_IRQ	32
#define GPIO5_IRQ	33
#define GPIO6_IRQ	34

#define UARTx_SIZE	0x400
#define UART3_ADDR	0x49020000

#define HSMMCx_SIZE	0x200
#define HSMMC1_ADDR	0x4809c000
#define HSMMC1_IRQ	83

#define	USBTLL_ADDR	0x48062000
#define	USBTLL_SIZE	0x1000

struct omap_dev omap3_devs[] = {

	/*
	 * Power, Reset and Clock Manager
	 */

	{ .name = "prcm",
	  .unit = 0,
	  .mem = { { PRCM_ADDR, PRCM_SIZE } },
	},

	/*
	 * Interrupt Controller
	 */

	{ .name = "intc",
	  .unit = 0,
	  .mem = { { INTC_ADDR, INTC_SIZE } },
	},

	/*
	 * General Purpose Timers
	 */

	{ .name = "gptimer",
	  .unit = 1,			/* XXX see gptimer.c */
	  .mem = { { GPTIMER1_ADDR, GPTIMERx_SIZE } },
	  .irq = { GPTIMER1_IRQ }
	},

	{ .name = "gptimer",
	  .unit = 0,			/* XXX see gptimer.c */
	  .mem = { { GPTIMER2_ADDR, GPTIMERx_SIZE } },
	  .irq = { GPTIMER2_IRQ }
	},

	/*
	 * GPIO
	 */

	{ .name = "omgpio",
	  .unit = 0,
	  .mem = { { GPIO1_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO1_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 1,
	  .mem = { { GPIO2_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO2_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 2,
	  .mem = { { GPIO3_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO3_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 3,
	  .mem = { { GPIO4_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO4_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 4,
	  .mem = { { GPIO5_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO5_IRQ }
	},

	{ .name = "omgpio",
	  .unit = 5,
	  .mem = { { GPIO6_ADDR, GPIOx_SIZE } },
	  .irq = { GPIO6_IRQ }
	},

	/*
	 * Watchdog Timer
	 */

	{ .name = "omdog",
	  .unit = 0,
	  .mem = { { WD_ADDR, WD_SIZE } }
	},

	/*
	 * UART
	 */

	{ .name = "com",
	  .unit = 2,
	  .mem = { { UART3_ADDR, UARTx_SIZE } }
	},

	/*
	 * MMC
	 */

	{ .name = "ommmc",
	  .unit = 0,
	  .mem = { { HSMMC1_ADDR, HSMMCx_SIZE } },
	  .irq = { HSMMC1_IRQ }
	},

	/*
	 * USB
	 */

	{ .name = "omusbtll",
	  .unit = 0,
	  .mem = { { USBTLL_ADDR, USBTLL_SIZE } },
	}
};

void
omap3_init(void)
{
	omap_set_devs(omap3_devs);
}
