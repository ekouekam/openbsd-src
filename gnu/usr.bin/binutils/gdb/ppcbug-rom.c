/* Remote debugging interface for PPCbug (PowerPC) Rom monitor
   for GDB, the GNU debugger.
   Copyright 1995 Free Software Foundation, Inc.

   Written by Stu Grossman of Cygnus Support

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "gdbcore.h"
#include "target.h"
#include "monitor.h"
#include "serial.h"

static void ppcbug_open PARAMS ((char *args, int from_tty));

static void
ppcbug_supply_register (regname, regnamelen, val, vallen)
     char *regname;
     int regnamelen;
     char *val;
     int vallen;
{
  int regno = 0, base = 0;

  if (regnamelen < 2 || regnamelen > 4)
    return;

  switch (regname[0])
    {
    case 'R':
      if (regname[1] < '0' || regname[1] > '9')
	return;
      if (regnamelen == 2)
	regno = regname[1] - '0';
      else if (regnamelen == 3 && regname[2] >= '0' && regname[2] <= '9')
	regno = (regname[1] - '0') * 10 + (regname[2] - '0');
      else
	return;
      break;
    case 'F':
      if (regname[1] != 'R' || regname[2] < '0' || regname[2] > '9')
	return;
      if (regnamelen == 3)
	regno = 32 + regname[2] - '0';
      else if (regnamelen == 4 && regname[3] >= '0' && regname[3] <= '9')
	regno = 32 + (regname[2] - '0') * 10 + (regname[3] - '0');
      else
	return;
      break;
    case 'I':
      if (regnamelen != 2 || regname[1] != 'P')
	return;
      regno = 64;
      break;
    case 'M':
      if (regnamelen != 3 || regname[1] != 'S' || regname[2] != 'R')
	return;
      regno = 65;
      break;
    case 'C':
      if (regnamelen != 2 || regname[1] != 'R')
	return;
      regno = 66;
      break;
    case 'S':
      if (regnamelen != 4 || regname[1] != 'P' || regname[2] != 'R')
	return;
      else if (regname[3] == '8')
	regno = 67;
      else if (regname[3] == '9')
	regno = 68;
      else if (regname[3] == '1')
	regno = 69;
      else if (regname[3] == '0')
	regno = 70;
      else
	return;
      break;
    default:
      return;
    }

  monitor_supply_register (regno, val);
}

/*
 * This array of registers needs to match the indexes used by GDB. The
 * whole reason this exists is because the various ROM monitors use
 * different names than GDB does, and don't support all the
 * registers either. So, typing "info reg sp" becomes an "A7".
 */

static char *ppcbug_regnames[NUM_REGS] =
{
  "r0",   "r1",   "r2",   "r3",   "r4",   "r5",   "r6",   "r7",
  "r8",   "r9",   "r10",  "r11",  "r12",  "r13",  "r14",  "r15",
  "r16",  "r17",  "r18",  "r19",  "r20",  "r21",  "r22",  "r23",
  "r24",  "r25",  "r26",  "r27",  "r28",  "r29",  "r30",  "r31",

  "fr0",  "fr1",  "fr2",  "fr3",  "fr4",  "fr5",  "fr6",  "fr7",
  "fr8",  "fr9",  "fr10", "fr11", "fr12", "fr13", "fr14", "fr15",
  "fr16", "fr17", "fr18", "fr19", "fr20", "fr21", "fr22", "fr23",
  "fr24", "fr25", "fr26", "fr27", "fr28", "fr29", "fr30", "fr31",

/* pc      ps      cnd     lr      cnt     xer     mq */
  "ip",   "msr",  "cr",   "spr8", "spr9", "spr1", "spr0"
};

/*
 * Define the monitor command strings. Since these are passed directly
 * through to a printf style function, we need can include formatting
 * strings. We also need a CR or LF on the end.
 */

static struct target_ops ppcbug_ops0;
static struct target_ops ppcbug_ops1;

static char *ppcbug_inits[] = {"\r", NULL};

#define PPC_CMDS(LOAD_CMD, OPS)						\
{									\
  MO_CLR_BREAK_USES_ADDR | MO_HANDLE_NL,				\
  ppcbug_inits,			/* Init strings */			\
  "g\r",			/* continue command */			\
  "t\r",			/* single step */			\
  NULL,				/* interrupt command */			\
  "br %x\r",			/* set a breakpoint */			\
  "nobr %x\r",			/* clear a breakpoint */		\
  "nobr\r",			/* clear all breakpoints */		\
  "bf %x:%x %x;b\r",		/* fill (start count val) */		\
  {									\
    "ms %x %02x\r",		/* setmem.cmdb (addr, value) */		\
    "ms %x %04x\r",		/* setmem.cmdw (addr, value) */		\
    "ms %x %08x\r",		/* setmem.cmdl (addr, value) */		\
    NULL,			/* setmem.cmdll (addr, value) */	\
    NULL,			/* setreg.resp_delim */			\
    NULL,			/* setreg.term */			\
    NULL,			/* setreg.term_cmd */			\
  },									\
  {									\
    "md %x:%x;b\r",		/* getmem.cmdb (addr, len) */		\
    "md %x:%x;b\r",		/* getmem.cmdw (addr, len) */		\
    "md %x:%x;b\r",		/* getmem.cmdl (addr, len) */		\
    NULL,			/* getmem.cmdll (addr, len) */		\
    " ",			/* getmem.resp_delim */			\
    NULL,			/* getmem.term */			\
    NULL,			/* getmem.term_cmd */			\
  },									\
  {									\
    "rs %s %x\r",		/* setreg.cmd (name, value) */		\
    NULL,			/* setreg.resp_delim */			\
    NULL,			/* setreg.term */			\
    NULL			/* setreg.term_cmd */			\
  },									\
  {									\
    "rs %s\r",			/* getreg.cmd (name) */			\
    "=",			/* getreg.resp_delim */			\
    NULL,			/* getreg.term */			\
    NULL			/* getreg.term_cmd */			\
  },									\
  "rd\r",			/* dump_registers */			\
  "\\(\\w+\\) +=\\([0-9a-fA-F]+\\b\\)", /* register_pattern */		\
  ppcbug_supply_register,	/* supply_register */			\
  NULL,				/* load_routine (defaults to SRECs) */	\
  LOAD_CMD,			/* download command */			\
  NULL,				/* load response */			\
  "PPC1-Bug>",			/* monitor command prompt */		\
  "\r",				/* end-of-line terminator */		\
  NULL,				/* optional command terminator */	\
  &OPS,				/* target operations */			\
  SERIAL_1_STOPBITS,		/* number of stop bits */		\
  ppcbug_regnames,		/* registers names */			\
  MONITOR_OPS_MAGIC		/* magic */				\
}


static struct monitor_ops ppcbug_cmds0 = PPC_CMDS("lo 0\r", ppcbug_ops0);
static struct monitor_ops ppcbug_cmds1 = PPC_CMDS("lo 1\r", ppcbug_ops1);

static void
ppcbug_open0(args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &ppcbug_cmds0, from_tty);
}

static void
ppcbug_open1(args, from_tty)
     char *args;
     int from_tty;
{
  monitor_open (args, &ppcbug_cmds1, from_tty);
}

void
_initialize_ppcbug_rom ()
{
  init_monitor_ops (&ppcbug_ops0);

  ppcbug_ops0.to_shortname = "ppcbug";
  ppcbug_ops0.to_longname = "PowerPC PPCBug monitor on port 0";
  ppcbug_ops0.to_doc = "Debug via the PowerPC PPCBug monitor using port 0.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  ppcbug_ops0.to_open = ppcbug_open0;

  add_target (&ppcbug_ops0);

  init_monitor_ops (&ppcbug_ops1);

  ppcbug_ops1.to_shortname = "ppcbug1";
  ppcbug_ops1.to_longname = "PowerPC PPCBug monitor on port 1";
  ppcbug_ops1.to_doc = "Debug via the PowerPC PPCBug monitor using port 1.\n\
Specify the serial device it is connected to (e.g. /dev/ttya).";
  ppcbug_ops1.to_open = ppcbug_open1;

  add_target (&ppcbug_ops1);
}
