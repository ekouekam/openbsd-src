/* Machine independent support for Solaris 2 core files for GDB.
   Copyright 1994 Free Software Foundation, Inc.

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


/* Solaris comes with two flavours of core files, cores generated by
   an ELF executable and cores generated by programs that were
   run under BCP (the part of Solaris which allows it to run SunOS4
   a.out files).
   This file combines the core register fetching from core-regset.c
   and sparc-nat.c to be able to read both flavours.  */

#include "defs.h"
#undef gregset_t
#undef fpregset_t

#include <time.h>
#include <sys/regset.h>
#include <sys/procfs.h>
#include <fcntl.h>
#include <errno.h>
#include "gdb_string.h"

#include "inferior.h"
#include "target.h"
#include "command.h"
#include "gdbcore.h"

static void fetch_core_registers PARAMS ((char *, unsigned, int, CORE_ADDR));

static void
fetch_core_registers (core_reg_sect, core_reg_size, which, reg_addr)
     char *core_reg_sect;
     unsigned core_reg_size;
     int which;
     CORE_ADDR reg_addr;	/* Unused in this version */
{
  prgregset_t prgregset;
  prfpregset_t prfpregset;

  if (which == 0)
    {
      if (core_reg_size == sizeof (prgregset))
	{
	  memcpy ((char *) &prgregset, core_reg_sect, sizeof (prgregset));
	  supply_gregset (&prgregset);
	}
      else if (core_reg_size == sizeof (struct regs))
	{
#define gregs ((struct regs *)core_reg_sect)
	  /* G0 *always* holds 0.  */
	  *(int *)&registers[REGISTER_BYTE (0)] = 0;

	  /* The globals and output registers.  */
	  memcpy (&registers[REGISTER_BYTE (G1_REGNUM)], &gregs->r_g1, 
		  15 * REGISTER_RAW_SIZE (G1_REGNUM));
	  *(int *)&registers[REGISTER_BYTE (PS_REGNUM)] = gregs->r_ps;
	  *(int *)&registers[REGISTER_BYTE (PC_REGNUM)] = gregs->r_pc;
	  *(int *)&registers[REGISTER_BYTE (NPC_REGNUM)] = gregs->r_npc;
	  *(int *)&registers[REGISTER_BYTE (Y_REGNUM)] = gregs->r_y;

	  /* My best guess at where to get the locals and input
	     registers is exactly where they usually are, right above
	     the stack pointer.  If the core dump was caused by a bus error
	     from blowing away the stack pointer (as is possible) then this
	     won't work, but it's worth the try. */
	  {
	    int sp;

	    sp = *(int *)&registers[REGISTER_BYTE (SP_REGNUM)];
	    if (0 != target_read_memory (sp,
					 &registers[REGISTER_BYTE (L0_REGNUM)], 
					 16 * REGISTER_RAW_SIZE (L0_REGNUM)))
	      {
		warning ("couldn't read input and local registers from core file\n");
	      }
	  }
	}
      else
	{
	  warning ("wrong size gregset struct in core file");
	}
    }
  else if (which == 2)
    {
      if (core_reg_size == sizeof (prfpregset))
	{
	  memcpy ((char *) &prfpregset, core_reg_sect, sizeof (prfpregset));
	  supply_fpregset (&prfpregset);
	}
      else if (core_reg_size >= sizeof (struct fpu))
	{
#define fpuregs  ((struct fpu *) core_reg_sect)
	  memcpy (&registers[REGISTER_BYTE (FP0_REGNUM)], &fpuregs->fpu_fr,
		  sizeof (fpuregs->fpu_fr));
	  memcpy (&registers[REGISTER_BYTE (FPS_REGNUM)], &fpuregs->fpu_fsr,
		  sizeof (FPU_FSR_TYPE));
	}
      else
	{
	  warning ("wrong size fpregset struct in core file");
	}
    }
}


/* Register that we are able to handle solaris core file formats. */

static struct core_fns solaris_core_fns =
{
  bfd_target_elf_flavour,
  fetch_core_registers,
  NULL
};

void
_initialize_core_solaris ()
{
  add_core_fns (&solaris_core_fns);
}
