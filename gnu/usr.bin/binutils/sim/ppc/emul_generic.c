/*  This file is part of the program psim.

    Copyright (C) 1994-1996, Andrew Cagney <cagney@highland.com.au>

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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#ifndef _EMUL_GENERIC_C_
#define _EMUL_GENERIC_C_

#include "emul_generic.h"

#ifndef STATIC_INLINE_EMUL_GENERIC
#define STATIC_INLINE_EMUL_GENERIC STATIC_INLINE
#endif


STATIC_INLINE_EMUL_GENERIC void
emul_syscall_enter(emul_syscall *emul,
		   int call,
		   int arg0,
		   cpu *processor,
		   unsigned_word cia)
{
  printf_filtered("%d:0x%lx:%s(",
		  cpu_nr(processor) + 1,
		  (long)cia, 
		  emul->syscall_descriptor[call].name);
}


STATIC_INLINE_EMUL_GENERIC void
emul_syscall_exit(emul_syscall *emul,
		  int call,
		  int arg0,
		  cpu *processor,
		  unsigned_word cia)
{
  int status = cpu_registers(processor)->gpr[3];
  int error = cpu_registers(processor)->gpr[0];
  printf_filtered(")=%d", status);
  if (error > 0 && error < emul->nr_error_names)
    printf_filtered("[%s]", emul->error_names[error]);
  printf_filtered("\n");
}


INLINE_EMUL_GENERIC unsigned64
emul_read_gpr64(cpu *processor,
		int g)
{
  unsigned32 hi;
  unsigned32 lo;
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN) {
    hi = cpu_registers(processor)->gpr[g];
    lo = cpu_registers(processor)->gpr[g+1];
  }
  else {
    lo = cpu_registers(processor)->gpr[g];
    hi = cpu_registers(processor)->gpr[g+1];
  }
  return (INSERTED64(hi, 0, 31) | INSERTED64(lo, 32, 63));
}


INLINE_EMUL_GENERIC void
emul_write_gpr64(cpu *processor,
		 int g,
		 unsigned64 val)
{
  unsigned32 hi = EXTRACTED64(val, 0, 31);
  unsigned32 lo = EXTRACTED64(val, 32, 63);
  if (CURRENT_TARGET_BYTE_ORDER == BIG_ENDIAN) {
    cpu_registers(processor)->gpr[g] = hi;
    cpu_registers(processor)->gpr[g+1] = lo;
  }
  else {
    cpu_registers(processor)->gpr[g] = lo;
    cpu_registers(processor)->gpr[g+1] = hi;
  }
}


INLINE_EMUL_GENERIC char *
emul_read_string(char *dest,
		 unsigned_word addr,
		 unsigned nr_bytes,
		 cpu *processor,
		 unsigned_word cia)
{
  unsigned nr_moved = 0;
  if (addr == 0)
    return NULL;
  while (1) {
    dest[nr_moved] = vm_data_map_read_1(cpu_data_map(processor),
					addr + nr_moved,
					processor, cia);
    if (dest[nr_moved] == '\0' || nr_moved >= nr_bytes)
      break;
    nr_moved++;
  }
  dest[nr_moved] = '\0';
  return dest;
}


INLINE_EMUL_GENERIC void
emul_write_status(cpu *processor,
		  int status,
		  int errno)
{
  if (status == -1 && errno != 0) {
    cpu_registers(processor)->gpr[3] = errno;
    CR_SET(0, cr_i_summary_overflow);
  }
  else {
    cpu_registers(processor)->gpr[3] = status;
    CR_SET(0, 0);
  }
}


INLINE_EMUL_GENERIC void
emul_write2_status(cpu *processor,
		   int status1,
		   int status2,
		   int errno)
{
  if (status1 == -1 && errno != 0) {
    cpu_registers(processor)->gpr[3] = errno;
    CR_SET(0, cr_i_summary_overflow);
  }
  else {
    cpu_registers(processor)->gpr[3] = status1;
    cpu_registers(processor)->gpr[4] = status2;
    CR_SET(0, 0);
  }
}


INLINE_EMUL_GENERIC unsigned_word
emul_read_word(unsigned_word addr,
	       cpu *processor,
	       unsigned_word cia)
{
  return vm_data_map_read_word(cpu_data_map(processor),
			       addr,
			       processor, cia);
}


INLINE_EMUL_GENERIC void
emul_write_word(unsigned_word addr,
		unsigned_word buf,
		cpu *processor,
		unsigned_word cia)
{
  vm_data_map_write_word(cpu_data_map(processor),
			 addr,
			 buf,
			 processor, cia);
}


INLINE_EMUL_GENERIC void
emul_write_buffer(const void *source,
		  unsigned_word addr,
		  unsigned nr_bytes,
		  cpu *processor,
		  unsigned_word cia)
{
  int nr_moved;
  for (nr_moved = 0; nr_moved < nr_bytes; nr_moved++) {
    vm_data_map_write_1(cpu_data_map(processor),
			addr + nr_moved,
			((const char*)source)[nr_moved],
			processor, cia);
  }
}


INLINE_EMUL_GENERIC void
emul_read_buffer(void *dest,
		 unsigned_word addr,
		 unsigned nr_bytes,
		 cpu *processor,
		 unsigned_word cia)
{
  int nr_moved;
  for (nr_moved = 0; nr_moved < nr_bytes; nr_moved++) {
    ((char*)dest)[nr_moved] = vm_data_map_read_1(cpu_data_map(processor),
						 addr + nr_moved,
						 processor, cia);
  }
}


INLINE_EMUL_GENERIC void
emul_do_system_call(os_emul_data *emul_data,
		    emul_syscall *emul,
		    unsigned call,
		    const int arg0,
		    cpu *processor,
		    unsigned_word cia)
{
  emul_syscall_handler *handler = NULL;
  if (call >= emul->nr_system_calls)
    error("do_call() os_emul call %d out-of-range\n", call);

  handler = emul->syscall_descriptor[call].handler;
  if (handler == NULL) {
    if (emul->syscall_descriptor[call].name) {
      error("do_call() unimplemented call %s\n", emul->syscall_descriptor[call].name);
    } else {
      error("do_call() unimplemented call %d\n", call);
    }
  }

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    emul_syscall_enter(emul, call, arg0, processor, cia);

  cpu_registers(processor)->gpr[0] = 0; /* default success */
  handler(emul_data, call, arg0, processor, cia);

  if (WITH_TRACE && ppc_trace[trace_os_emul])
    emul_syscall_exit(emul, call, arg0, processor, cia);
}


/* default size for the first bank of memory */

#ifndef OEA_MEMORY_SIZE
#define OEA_MEMORY_SIZE 0x100000
#endif


/* Add options to the device tree */

INLINE_EMUL_GENERIC void
emul_add_tree_options(device *tree,
		      bfd *image,
		      const char *emul,
		      const char *env,
		      int oea_interrupt_prefix)
{
  int little_endian = 0;

  /* sort out little endian */
  if (device_find_property(tree, "/options/little-endian?"))
    little_endian = device_find_boolean_property(tree, "/options/little-endian?");
  else {
#ifdef bfd_little_endian	/* new bfd */
    little_endian = (image != NULL && bfd_little_endian(image));
#else
    little_endian = (image != NULL &&
		     !image->xvec->byteorder_big_p);
#endif
    device_tree_add_parsed(tree, "/options/little-endian? %s",
			   little_endian ? "true" : "false");
  }

  /* misc other stuff */
  device_tree_add_parsed(tree, "/openprom/options/oea-memory-size 0x%x",
			 OEA_MEMORY_SIZE);
  device_tree_add_parsed(tree, "/openprom/options/oea-interrupt-prefix %d",
			 oea_interrupt_prefix);
  device_tree_add_parsed(tree, "/openprom/options/smp 1");
  device_tree_add_parsed(tree, "/openprom/options/env %s", env);
  device_tree_add_parsed(tree, "/openprom/options/os-emul %s", emul);
  device_tree_add_parsed(tree, "/openprom/options/strict-alignment? %s",
			 ((WITH_ALIGNMENT == 0 && little_endian)
			  || (WITH_ALIGNMENT == STRICT_ALIGNMENT))
			 ? "true" : "false");
  device_tree_add_parsed(tree, "/openprom/options/floating-point? %s",
			 WITH_FLOATING_POINT ? "true" : "false");
  device_tree_add_parsed(tree, "/openprom/options/use-stdio? %s",
			 WITH_STDIO == DO_USE_STDIO ? "true" : "false");
  device_tree_add_parsed(tree, "/openprom/options/model \"%s",
			 model_name[WITH_DEFAULT_MODEL]);
  device_tree_add_parsed(tree, "/openprom/options/model-issue %d",
			 MODEL_ISSUE_IGNORE);

  /* useful options */
  /* FIXME - need to check the OpenBoot powerpc bindings to see if
     this is still used and if so what it should be */
  device_tree_add_parsed(tree, "/options/load-base 0x80000");
}

INLINE_EMUL_GENERIC void
emul_add_tree_hardware(device *root)
{
  int i;
  int nr_cpus = device_find_integer_property(root, "/openprom/options/smp");

  /* sanity check the number of processors */
  if (nr_cpus > MAX_NR_PROCESSORS)
    error("Specified number of processors (%d) exceeds the number configured (%d).\n",
	  nr_cpus, MAX_NR_PROCESSORS);

  /* add some memory */
  if (device_tree_find_device(root, "/memory") == NULL) {
    unsigned_word memory_size =
      device_find_integer_property(root, "/openprom/options/oea-memory-size");
    device_tree_add_parsed(root, "/memory@0/reg { 0x0 0x%lx",
			   (unsigned long)memory_size);
    /* what about allocated? */
  }

  /* our processors */
  for (i = 0; i < nr_cpus; i++) {
    device_tree_add_parsed(root, "/cpus/cpu@%d/cpu-nr %d", i, i);
  }

  /* a fake eeprom - need to be able to write to it */
  device_tree_add_parsed(root, "/openprom/memory@0xfff00000/reg { 0xfff00000 0x3000");


  /* A local bus containing basic devices */
  device_tree_add_parsed(root, "/iobus@0xf0000000/reg { 0xf0000000 0x0f000000");

  /* NVRAM with clock */
  device_tree_add_parsed(root, "/iobus/nvram@0x0/reg   { 0 0x1000");
  device_tree_add_parsed(root, "/iobus/nvram/timezone 0");

  /* the debugging pal. Wire interrupts up directly */
  device_tree_add_parsed(root, "/iobus/pal@0x0x1000/reg  { 0x1000 32");
  for (i = 0; i < nr_cpus; i++) {
    device_tree_add_parsed(root, "/iobus/pal > int%d int /cpus/cpu@%d", i, i);
  }

#ifdef HAVE_DEVZERO
  /* a disk containing zero's */
  device_tree_add_parsed(root, "/iobus/disk@0x2000/reg { 0x2000 0x1000");
  device_tree_add_parsed(root, "/iobus/disk/file \"/dev/zero");
#endif

  /* chosen etc */
  device_tree_add_parsed(root, "/chosen/stdin */iobus/pal");
  device_tree_add_parsed(root, "/chosen/stdout !/chosen/stdin");
  device_tree_add_parsed(root, "/chosen/memory */memory");
#ifdef HAVE_DEVZERO
  device_tree_add_parsed(root, "/chosen/disk */iobus/disk");
#endif
}

#endif /* _EMUL_GENERIC_C_ */
