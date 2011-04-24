/* Xtensa-specific support for 32-bit ELF.
   Copyright 2003, 2004, 2005 Free Software Foundation, Inc.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "bfd.h"
#include "sysdep.h"

#include <stdarg.h>
#include <strings.h>

#include "bfdlink.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "elf/xtensa.h"
#include "xtensa-isa.h"
#include "xtensa-config.h"

#define XTENSA_NO_NOP_REMOVAL 0

/* Local helper functions.  */

static bfd_boolean add_extra_plt_sections (bfd *, int);
static char *vsprint_msg (const char *, const char *, int, ...) ATTRIBUTE_PRINTF(2,4);
static bfd_reloc_status_type bfd_elf_xtensa_reloc
  (bfd *, arelent *, asymbol *, void *, asection *, bfd *, char **);
static bfd_boolean do_fix_for_relocatable_link
  (Elf_Internal_Rela *, bfd *, asection *, bfd_byte *);
static void do_fix_for_final_link
  (Elf_Internal_Rela *, bfd *, asection *, bfd_byte *, bfd_vma *);

/* Local functions to handle Xtensa configurability.  */

static bfd_boolean is_indirect_call_opcode (xtensa_opcode);
static bfd_boolean is_direct_call_opcode (xtensa_opcode);
static bfd_boolean is_windowed_call_opcode (xtensa_opcode);
static xtensa_opcode get_const16_opcode (void);
static xtensa_opcode get_l32r_opcode (void);
static bfd_vma l32r_offset (bfd_vma, bfd_vma);
static int get_relocation_opnd (xtensa_opcode, int);
static int get_relocation_slot (int);
static xtensa_opcode get_relocation_opcode
  (bfd *, asection *, bfd_byte *, Elf_Internal_Rela *);
static bfd_boolean is_l32r_relocation
  (bfd *, asection *, bfd_byte *, Elf_Internal_Rela *);
static bfd_boolean is_alt_relocation (int);
static bfd_boolean is_operand_relocation (int);
static bfd_size_type insn_decode_len
  (bfd_byte *, bfd_size_type, bfd_size_type);
static xtensa_opcode insn_decode_opcode
  (bfd_byte *, bfd_size_type, bfd_size_type, int);
static bfd_boolean check_branch_target_aligned
  (bfd_byte *, bfd_size_type, bfd_vma, bfd_vma);
static bfd_boolean check_loop_aligned
  (bfd_byte *, bfd_size_type, bfd_vma, bfd_vma);
static bfd_boolean check_branch_target_aligned_address (bfd_vma, int);
static bfd_size_type get_asm_simplify_size
  (bfd_byte *, bfd_size_type, bfd_size_type);

/* Functions for link-time code simplifications.  */

static bfd_reloc_status_type elf_xtensa_do_asm_simplify
  (bfd_byte *, bfd_vma, bfd_vma, char **);
static bfd_reloc_status_type contract_asm_expansion
  (bfd_byte *, bfd_vma, Elf_Internal_Rela *, char **);
static xtensa_opcode swap_callx_for_call_opcode (xtensa_opcode);
static xtensa_opcode get_expanded_call_opcode (bfd_byte *, int, bfd_boolean *);

/* Access to internal relocations, section contents and symbols.  */

static Elf_Internal_Rela *retrieve_internal_relocs
  (bfd *, asection *, bfd_boolean);
static void pin_internal_relocs (asection *, Elf_Internal_Rela *);
static void release_internal_relocs (asection *, Elf_Internal_Rela *);
static bfd_byte *retrieve_contents (bfd *, asection *, bfd_boolean);
static void pin_contents (asection *, bfd_byte *);
static void release_contents (asection *, bfd_byte *);
static Elf_Internal_Sym *retrieve_local_syms (bfd *);

/* Miscellaneous utility functions.  */

static asection *elf_xtensa_get_plt_section (bfd *, int);
static asection *elf_xtensa_get_gotplt_section (bfd *, int);
static asection *get_elf_r_symndx_section (bfd *, unsigned long);
static struct elf_link_hash_entry *get_elf_r_symndx_hash_entry
  (bfd *, unsigned long);
static bfd_vma get_elf_r_symndx_offset (bfd *, unsigned long);
static bfd_boolean is_reloc_sym_weak (bfd *, Elf_Internal_Rela *);
static bfd_boolean pcrel_reloc_fits (xtensa_opcode, int, bfd_vma, bfd_vma);
static bfd_boolean xtensa_is_property_section (asection *);
static bfd_boolean xtensa_is_littable_section (asection *);
static int internal_reloc_compare (const void *, const void *);
static int internal_reloc_matches (const void *, const void *);
extern char *xtensa_get_property_section_name (asection *, const char *);
static flagword xtensa_get_property_predef_flags (asection *);

/* Other functions called directly by the linker.  */

typedef void (*deps_callback_t)
  (asection *, bfd_vma, asection *, bfd_vma, void *);
extern bfd_boolean xtensa_callback_required_dependence
  (bfd *, asection *, struct bfd_link_info *, deps_callback_t, void *);


/* Globally visible flag for choosing size optimization of NOP removal
   instead of branch-target-aware minimization for NOP removal.
   When nonzero, narrow all instructions and remove all NOPs possible
   around longcall expansions.  */

int elf32xtensa_size_opt;


/* The "new_section_hook" is used to set up a per-section
   "xtensa_relax_info" data structure with additional information used
   during relaxation.  */

typedef struct xtensa_relax_info_struct xtensa_relax_info;


/* Total count of PLT relocations seen during check_relocs.
   The actual PLT code must be split into multiple sections and all
   the sections have to be created before size_dynamic_sections,
   where we figure out the exact number of PLT entries that will be
   needed.  It is OK if this count is an overestimate, e.g., some
   relocations may be removed by GC.  */

static int plt_reloc_count = 0;


/* The GNU tools do not easily allow extending interfaces to pass around
   the pointer to the Xtensa ISA information, so instead we add a global
   variable here (in BFD) that can be used by any of the tools that need
   this information. */

xtensa_isa xtensa_default_isa;


/* When this is true, relocations may have been modified to refer to
   symbols from other input files.  The per-section list of "fix"
   records needs to be checked when resolving relocations.  */

static bfd_boolean relaxing_section = FALSE;

/* When this is true, during final links, literals that cannot be
   coalesced and their relocations may be moved to other sections.  */

int elf32xtensa_no_literal_movement = 1;


static reloc_howto_type elf_howto_table[] =
{
  HOWTO (R_XTENSA_NONE, 0, 0, 0, FALSE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_NONE",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  HOWTO (R_XTENSA_32, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_32",
	 TRUE, 0xffffffff, 0xffffffff, FALSE),
  /* Replace a 32-bit value with a value from the runtime linker (only
     used by linker-generated stub functions).  The r_addend value is
     special: 1 means to substitute a pointer to the runtime linker's
     dynamic resolver function; 2 means to substitute the link map for
     the shared object.  */
  HOWTO (R_XTENSA_RTLD, 0, 2, 32, FALSE, 0, complain_overflow_dont,
	 NULL, "R_XTENSA_RTLD",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  HOWTO (R_XTENSA_GLOB_DAT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_GLOB_DAT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_JMP_SLOT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_JMP_SLOT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_RELATIVE, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_generic_reloc, "R_XTENSA_RELATIVE",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_PLT, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_PLT",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  EMPTY_HOWTO (7),
  HOWTO (R_XTENSA_OP0, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP0",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_OP1, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP1",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_OP2, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_OP2",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  /* Assembly auto-expansion.  */
  HOWTO (R_XTENSA_ASM_EXPAND, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_ASM_EXPAND",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  /* Relax assembly auto-expansion.  */
  HOWTO (R_XTENSA_ASM_SIMPLIFY, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_ASM_SIMPLIFY",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  EMPTY_HOWTO (13),
  EMPTY_HOWTO (14),
  /* GNU extension to record C++ vtable hierarchy.  */
  HOWTO (R_XTENSA_GNU_VTINHERIT, 0, 2, 0, FALSE, 0, complain_overflow_dont,
         NULL, "R_XTENSA_GNU_VTINHERIT",
	 FALSE, 0x00000000, 0x00000000, FALSE),
  /* GNU extension to record C++ vtable member usage.  */
  HOWTO (R_XTENSA_GNU_VTENTRY, 0, 2, 0, FALSE, 0, complain_overflow_dont,
         _bfd_elf_rel_vtable_reloc_fn, "R_XTENSA_GNU_VTENTRY",
	 FALSE, 0x00000000, 0x00000000, FALSE),

  /* Relocations for supporting difference of symbols.  */
  HOWTO (R_XTENSA_DIFF8, 0, 0, 8, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_DIFF8",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_DIFF16, 0, 1, 16, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_DIFF16",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),
  HOWTO (R_XTENSA_DIFF32, 0, 2, 32, FALSE, 0, complain_overflow_bitfield,
	 bfd_elf_xtensa_reloc, "R_XTENSA_DIFF32",
	 FALSE, 0xffffffff, 0xffffffff, FALSE),

  /* General immediate operand relocations.  */
  HOWTO (R_XTENSA_SLOT0_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT0_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT1_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT1_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT2_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT2_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT3_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT3_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT4_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT4_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT5_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT5_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT6_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT6_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT7_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT7_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT8_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT8_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT9_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT9_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT10_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT10_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT11_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT11_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT12_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT12_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT13_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT13_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT14_OP, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT14_OP",
	 FALSE, 0x00000000, 0x00000000, TRUE),

  /* "Alternate" relocations.  The meaning of these is opcode-specific.  */
  HOWTO (R_XTENSA_SLOT0_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT0_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT1_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT1_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT2_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT2_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT3_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT3_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT4_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT4_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT5_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT5_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT6_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT6_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT7_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT7_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT8_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT8_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT9_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT9_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT10_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT10_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT11_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT11_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT12_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT12_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT13_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT13_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE),
  HOWTO (R_XTENSA_SLOT14_ALT, 0, 0, 0, TRUE, 0, complain_overflow_dont,
	 bfd_elf_xtensa_reloc, "R_XTENSA_SLOT14_ALT",
	 FALSE, 0x00000000, 0x00000000, TRUE)
};

#if DEBUG_GEN_RELOC
#define TRACE(str) \
  fprintf (stderr, "Xtensa bfd reloc lookup %d (%s)\n", code, str)
#else
#define TRACE(str)
#endif

static reloc_howto_type *
elf_xtensa_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			      bfd_reloc_code_real_type code)
{
  switch (code)
    {
    case BFD_RELOC_NONE:
      TRACE ("BFD_RELOC_NONE");
      return &elf_howto_table[(unsigned) R_XTENSA_NONE ];

    case BFD_RELOC_32:
      TRACE ("BFD_RELOC_32");
      return &elf_howto_table[(unsigned) R_XTENSA_32 ];

    case BFD_RELOC_XTENSA_DIFF8:
      TRACE ("BFD_RELOC_XTENSA_DIFF8");
      return &elf_howto_table[(unsigned) R_XTENSA_DIFF8 ];

    case BFD_RELOC_XTENSA_DIFF16:
      TRACE ("BFD_RELOC_XTENSA_DIFF16");
      return &elf_howto_table[(unsigned) R_XTENSA_DIFF16 ];

    case BFD_RELOC_XTENSA_DIFF32:
      TRACE ("BFD_RELOC_XTENSA_DIFF32");
      return &elf_howto_table[(unsigned) R_XTENSA_DIFF32 ];

    case BFD_RELOC_XTENSA_RTLD:
      TRACE ("BFD_RELOC_XTENSA_RTLD");
      return &elf_howto_table[(unsigned) R_XTENSA_RTLD ];

    case BFD_RELOC_XTENSA_GLOB_DAT:
      TRACE ("BFD_RELOC_XTENSA_GLOB_DAT");
      return &elf_howto_table[(unsigned) R_XTENSA_GLOB_DAT ];

    case BFD_RELOC_XTENSA_JMP_SLOT:
      TRACE ("BFD_RELOC_XTENSA_JMP_SLOT");
      return &elf_howto_table[(unsigned) R_XTENSA_JMP_SLOT ];

    case BFD_RELOC_XTENSA_RELATIVE:
      TRACE ("BFD_RELOC_XTENSA_RELATIVE");
      return &elf_howto_table[(unsigned) R_XTENSA_RELATIVE ];

    case BFD_RELOC_XTENSA_PLT:
      TRACE ("BFD_RELOC_XTENSA_PLT");
      return &elf_howto_table[(unsigned) R_XTENSA_PLT ];

    case BFD_RELOC_XTENSA_OP0:
      TRACE ("BFD_RELOC_XTENSA_OP0");
      return &elf_howto_table[(unsigned) R_XTENSA_OP0 ];

    case BFD_RELOC_XTENSA_OP1:
      TRACE ("BFD_RELOC_XTENSA_OP1");
      return &elf_howto_table[(unsigned) R_XTENSA_OP1 ];

    case BFD_RELOC_XTENSA_OP2:
      TRACE ("BFD_RELOC_XTENSA_OP2");
      return &elf_howto_table[(unsigned) R_XTENSA_OP2 ];

    case BFD_RELOC_XTENSA_ASM_EXPAND:
      TRACE ("BFD_RELOC_XTENSA_ASM_EXPAND");
      return &elf_howto_table[(unsigned) R_XTENSA_ASM_EXPAND ];

    case BFD_RELOC_XTENSA_ASM_SIMPLIFY:
      TRACE ("BFD_RELOC_XTENSA_ASM_SIMPLIFY");
      return &elf_howto_table[(unsigned) R_XTENSA_ASM_SIMPLIFY ];

    case BFD_RELOC_VTABLE_INHERIT:
      TRACE ("BFD_RELOC_VTABLE_INHERIT");
      return &elf_howto_table[(unsigned) R_XTENSA_GNU_VTINHERIT ];

    case BFD_RELOC_VTABLE_ENTRY:
      TRACE ("BFD_RELOC_VTABLE_ENTRY");
      return &elf_howto_table[(unsigned) R_XTENSA_GNU_VTENTRY ];

    default:
      if (code >= BFD_RELOC_XTENSA_SLOT0_OP
	  && code <= BFD_RELOC_XTENSA_SLOT14_OP)
	{
	  unsigned n = (R_XTENSA_SLOT0_OP +
			(code - BFD_RELOC_XTENSA_SLOT0_OP));
	  return &elf_howto_table[n];
	}

      if (code >= BFD_RELOC_XTENSA_SLOT0_ALT
	  && code <= BFD_RELOC_XTENSA_SLOT14_ALT)
	{
	  unsigned n = (R_XTENSA_SLOT0_ALT +
			(code - BFD_RELOC_XTENSA_SLOT0_ALT));
	  return &elf_howto_table[n];
	}

      break;
    }

  TRACE ("Unknown");
  return NULL;
}


/* Given an ELF "rela" relocation, find the corresponding howto and record
   it in the BFD internal arelent representation of the relocation.  */

static void
elf_xtensa_info_to_howto_rela (bfd *abfd ATTRIBUTE_UNUSED,
			       arelent *cache_ptr,
			       Elf_Internal_Rela *dst)
{
  unsigned int r_type = ELF32_R_TYPE (dst->r_info);

  BFD_ASSERT (r_type < (unsigned int) R_XTENSA_max);
  cache_ptr->howto = &elf_howto_table[r_type];
}


/* Functions for the Xtensa ELF linker.  */

/* The name of the dynamic interpreter.  This is put in the .interp
   section.  */

#define ELF_DYNAMIC_INTERPRETER "/lib/ld.so"

/* The size in bytes of an entry in the procedure linkage table.
   (This does _not_ include the space for the literals associated with
   the PLT entry.) */

#define PLT_ENTRY_SIZE 16

/* For _really_ large PLTs, we may need to alternate between literals
   and code to keep the literals within the 256K range of the L32R
   instructions in the code.  It's unlikely that anyone would ever need
   such a big PLT, but an arbitrary limit on the PLT size would be bad.
   Thus, we split the PLT into chunks.  Since there's very little
   overhead (2 extra literals) for each chunk, the chunk size is kept
   small so that the code for handling multiple chunks get used and
   tested regularly.  With 254 entries, there are 1K of literals for
   each chunk, and that seems like a nice round number.  */

#define PLT_ENTRIES_PER_CHUNK 254

/* PLT entries are actually used as stub functions for lazy symbol
   resolution.  Once the symbol is resolved, the stub function is never
   invoked.  Note: the 32-byte frame size used here cannot be changed
   without a corresponding change in the runtime linker.  */

static const bfd_byte elf_xtensa_be_plt_entry[PLT_ENTRY_SIZE] =
{
  0x6c, 0x10, 0x04,	/* entry sp, 32 */
  0x18, 0x00, 0x00,	/* l32r  a8, [got entry for rtld's resolver] */
  0x1a, 0x00, 0x00,	/* l32r  a10, [got entry for rtld's link map] */
  0x1b, 0x00, 0x00,	/* l32r  a11, [literal for reloc index] */
  0x0a, 0x80, 0x00,	/* jx    a8 */
  0			/* unused */
};

static const bfd_byte elf_xtensa_le_plt_entry[PLT_ENTRY_SIZE] =
{
  0x36, 0x41, 0x00,	/* entry sp, 32 */
  0x81, 0x00, 0x00,	/* l32r  a8, [got entry for rtld's resolver] */
  0xa1, 0x00, 0x00,	/* l32r  a10, [got entry for rtld's link map] */
  0xb1, 0x00, 0x00,	/* l32r  a11, [literal for reloc index] */
  0xa0, 0x08, 0x00,	/* jx    a8 */
  0			/* unused */
};


static inline bfd_boolean
xtensa_elf_dynamic_symbol_p (struct elf_link_hash_entry *h,
			     struct bfd_link_info *info)
{
  /* Check if we should do dynamic things to this symbol.  The
     "ignore_protected" argument need not be set, because Xtensa code
     does not require special handling of STV_PROTECTED to make function
     pointer comparisons work properly.  The PLT addresses are never
     used for function pointers.  */

  return _bfd_elf_dynamic_symbol_p (h, info, 0);
}


static int
property_table_compare (const void *ap, const void *bp)
{
  const property_table_entry *a = (const property_table_entry *) ap;
  const property_table_entry *b = (const property_table_entry *) bp;

  if (a->address == b->address)
    {
      if (a->size != b->size)
	return (a->size - b->size);

      if ((a->flags & XTENSA_PROP_ALIGN) != (b->flags & XTENSA_PROP_ALIGN))
	return ((b->flags & XTENSA_PROP_ALIGN)
		- (a->flags & XTENSA_PROP_ALIGN));

      if ((a->flags & XTENSA_PROP_ALIGN)
	  && (GET_XTENSA_PROP_ALIGNMENT (a->flags)
	      != GET_XTENSA_PROP_ALIGNMENT (b->flags)))
	return (GET_XTENSA_PROP_ALIGNMENT (a->flags)
		- GET_XTENSA_PROP_ALIGNMENT (b->flags));
      
      if ((a->flags & XTENSA_PROP_UNREACHABLE)
	  != (b->flags & XTENSA_PROP_UNREACHABLE))
	return ((b->flags & XTENSA_PROP_UNREACHABLE)
		- (a->flags & XTENSA_PROP_UNREACHABLE));

      return (a->flags - b->flags);
    }

  return (a->address - b->address);
}


static int
property_table_matches (const void *ap, const void *bp)
{
  const property_table_entry *a = (const property_table_entry *) ap;
  const property_table_entry *b = (const property_table_entry *) bp;

  /* Check if one entry overlaps with the other.  */
  if ((b->address >= a->address && b->address < (a->address + a->size))
      || (a->address >= b->address && a->address < (b->address + b->size)))
    return 0;

  return (a->address - b->address);
}


/* Get the literal table or property table entries for the given
   section.  Sets TABLE_P and returns the number of entries.  On
   error, returns a negative value.  */

static int
xtensa_read_table_entries (bfd *abfd,
			   asection *section,
			   property_table_entry **table_p,
			   const char *sec_name,
			   bfd_boolean output_addr)
{
  asection *table_section;
  char *table_section_name;
  bfd_size_type table_size = 0;
  bfd_byte *table_data;
  property_table_entry *blocks;
  int blk, block_count;
  bfd_size_type num_records;
  Elf_Internal_Rela *internal_relocs;
  bfd_vma section_addr;
  flagword predef_flags;
  bfd_size_type table_entry_size;

  if (!section
      || !(section->flags & SEC_ALLOC)
      || (section->flags & SEC_DEBUGGING))
    {
      *table_p = NULL;
      return 0;
    }

  table_section_name = xtensa_get_property_section_name (section, sec_name);
  table_section = bfd_get_section_by_name (abfd, table_section_name);
  free (table_section_name);
  if (table_section)
    table_size = table_section->size;

  if (table_size == 0) 
    {
      *table_p = NULL;
      return 0;
    }

  predef_flags = xtensa_get_property_predef_flags (table_section);
  table_entry_size = 12;
  if (predef_flags)
    table_entry_size -= 4;

  num_records = table_size / table_entry_size;
  table_data = retrieve_contents (abfd, table_section, TRUE);
  blocks = (property_table_entry *)
    bfd_malloc (num_records * sizeof (property_table_entry));
  block_count = 0;

  if (output_addr)
    section_addr = section->output_section->vma + section->output_offset;
  else
    section_addr = section->vma;

  /* If the file has not yet been relocated, process the relocations
     and sort out the table entries that apply to the specified section.  */
  internal_relocs = retrieve_internal_relocs (abfd, table_section, TRUE);
  if (internal_relocs && !table_section->reloc_done)
    {
      unsigned i;

      for (i = 0; i < table_section->reloc_count; i++)
	{
	  Elf_Internal_Rela *rel = &internal_relocs[i];
	  unsigned long r_symndx;

	  if (ELF32_R_TYPE (rel->r_info) == R_XTENSA_NONE)
	    continue;

	  BFD_ASSERT (ELF32_R_TYPE (rel->r_info) == R_XTENSA_32);
	  r_symndx = ELF32_R_SYM (rel->r_info);

	  if (get_elf_r_symndx_section (abfd, r_symndx) == section)
	    {
	      bfd_vma sym_off = get_elf_r_symndx_offset (abfd, r_symndx);
	      BFD_ASSERT (sym_off == 0);
	      blocks[block_count].address =
		(section_addr + sym_off + rel->r_addend
		 + bfd_get_32 (abfd, table_data + rel->r_offset));
	      blocks[block_count].size =
		bfd_get_32 (abfd, table_data + rel->r_offset + 4);
	      if (predef_flags)
		blocks[block_count].flags = predef_flags;
	      else
		blocks[block_count].flags =
		  bfd_get_32 (abfd, table_data + rel->r_offset + 8);
	      block_count++;
	    }
	}
    }
  else
    {
      /* The file has already been relocated and the addresses are
	 already in the table.  */
      bfd_vma off;
      bfd_size_type section_limit = bfd_get_section_limit (abfd, section);

      for (off = 0; off < table_size; off += table_entry_size) 
	{
	  bfd_vma address = bfd_get_32 (abfd, table_data + off);

	  if (address >= section_addr
	      && address < section_addr + section_limit)
	    {
	      blocks[block_count].address = address;
	      blocks[block_count].size =
		bfd_get_32 (abfd, table_data + off + 4);
	      if (predef_flags)
		blocks[block_count].flags = predef_flags;
	      else
		blocks[block_count].flags =
		  bfd_get_32 (abfd, table_data + off + 8);
	      block_count++;
	    }
	}
    }

  release_contents (table_section, table_data);
  release_internal_relocs (table_section, internal_relocs);

  if (block_count > 0)
    {
      /* Now sort them into address order for easy reference.  */
      qsort (blocks, block_count, sizeof (property_table_entry),
	     property_table_compare);

      /* Check that the table contents are valid.  Problems may occur,
         for example, if an unrelocated object file is stripped.  */
      for (blk = 1; blk < block_count; blk++)
	{
	  /* The only circumstance where two entries may legitimately
	     have the same address is when one of them is a zero-size
	     placeholder to mark a place where fill can be inserted.
	     The zero-size entry should come first.  */
	  if (blocks[blk - 1].address == blocks[blk].address &&
	      blocks[blk - 1].size != 0)
	    {
	      (*_bfd_error_handler) (_("%B(%A): invalid property table"),
				     abfd, section);
	      bfd_set_error (bfd_error_bad_value);
	      free (blocks);
	      return -1;
	    }
	}
    }

  *table_p = blocks;
  return block_count;
}


static property_table_entry *
elf_xtensa_find_property_entry (property_table_entry *property_table,
				int property_table_size,
				bfd_vma addr)
{
  property_table_entry entry;
  property_table_entry *rv;

  if (property_table_size == 0)
    return NULL;

  entry.address = addr;
  entry.size = 1;
  entry.flags = 0;

  rv = bsearch (&entry, property_table, property_table_size,
		sizeof (property_table_entry), property_table_matches);
  return rv;
}


static bfd_boolean
elf_xtensa_in_literal_pool (property_table_entry *lit_table,
			    int lit_table_size,
			    bfd_vma addr)
{
  if (elf_xtensa_find_property_entry (lit_table, lit_table_size, addr))
    return TRUE;

  return FALSE;
}


/* Look through the relocs for a section during the first phase, and
   calculate needed space in the dynamic reloc sections.  */

static bfd_boolean
elf_xtensa_check_relocs (bfd *abfd,
			 struct bfd_link_info *info,
			 asection *sec,
			 const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;
  const Elf_Internal_Rela *rel_end;

  if (info->relocatable)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      unsigned int r_type;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;

      r_symndx = ELF32_R_SYM (rel->r_info);
      r_type = ELF32_R_TYPE (rel->r_info);

      if (r_symndx >= NUM_SHDR_ENTRIES (symtab_hdr))
	{
	  (*_bfd_error_handler) (_("%B: bad symbol index: %d"),
				 abfd, r_symndx);
	  return FALSE;
	}

      if (r_symndx < symtab_hdr->sh_info)
	h = NULL;
      else
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      switch (r_type)
	{
	case R_XTENSA_32:
	  if (h == NULL)
	    goto local_literal;

	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      if (h->got.refcount <= 0)
		h->got.refcount = 1;
	      else
		h->got.refcount += 1;
	    }
	  break;

	case R_XTENSA_PLT:
	  /* If this relocation is against a local symbol, then it's
	     exactly the same as a normal local GOT entry.  */
	  if (h == NULL)
	    goto local_literal;

	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      if (h->plt.refcount <= 0)
		{
		  h->needs_plt = 1;
		  h->plt.refcount = 1;
		}
	      else
		h->plt.refcount += 1;

	      /* Keep track of the total PLT relocation count even if we
		 don't yet know whether the dynamic sections will be
		 created.  */
	      plt_reloc_count += 1;

	      if (elf_hash_table (info)->dynamic_sections_created)
		{
		  if (!add_extra_plt_sections (elf_hash_table (info)->dynobj,
					       plt_reloc_count))
		    return FALSE;
		}
	    }
	  break;

	local_literal:
	  if ((sec->flags & SEC_ALLOC) != 0)
	    {
	      bfd_signed_vma *local_got_refcounts;

	      /* This is a global offset table entry for a local symbol.  */
	      local_got_refcounts = elf_local_got_refcounts (abfd);
	      if (local_got_refcounts == NULL)
		{
		  bfd_size_type size;

		  size = symtab_hdr->sh_info;
		  size *= sizeof (bfd_signed_vma);
		  local_got_refcounts =
		    (bfd_signed_vma *) bfd_zalloc (abfd, size);
		  if (local_got_refcounts == NULL)
		    return FALSE;
		  elf_local_got_refcounts (abfd) = local_got_refcounts;
		}
	      local_got_refcounts[r_symndx] += 1;
	    }
	  break;

	case R_XTENSA_OP0:
	case R_XTENSA_OP1:
	case R_XTENSA_OP2:
	case R_XTENSA_SLOT0_OP:
	case R_XTENSA_SLOT1_OP:
	case R_XTENSA_SLOT2_OP:
	case R_XTENSA_SLOT3_OP:
	case R_XTENSA_SLOT4_OP:
	case R_XTENSA_SLOT5_OP:
	case R_XTENSA_SLOT6_OP:
	case R_XTENSA_SLOT7_OP:
	case R_XTENSA_SLOT8_OP:
	case R_XTENSA_SLOT9_OP:
	case R_XTENSA_SLOT10_OP:
	case R_XTENSA_SLOT11_OP:
	case R_XTENSA_SLOT12_OP:
	case R_XTENSA_SLOT13_OP:
	case R_XTENSA_SLOT14_OP:
	case R_XTENSA_SLOT0_ALT:
	case R_XTENSA_SLOT1_ALT:
	case R_XTENSA_SLOT2_ALT:
	case R_XTENSA_SLOT3_ALT:
	case R_XTENSA_SLOT4_ALT:
	case R_XTENSA_SLOT5_ALT:
	case R_XTENSA_SLOT6_ALT:
	case R_XTENSA_SLOT7_ALT:
	case R_XTENSA_SLOT8_ALT:
	case R_XTENSA_SLOT9_ALT:
	case R_XTENSA_SLOT10_ALT:
	case R_XTENSA_SLOT11_ALT:
	case R_XTENSA_SLOT12_ALT:
	case R_XTENSA_SLOT13_ALT:
	case R_XTENSA_SLOT14_ALT:
	case R_XTENSA_ASM_EXPAND:
	case R_XTENSA_ASM_SIMPLIFY:
	case R_XTENSA_DIFF8:
	case R_XTENSA_DIFF16:
	case R_XTENSA_DIFF32:
	  /* Nothing to do for these.  */
	  break;

	case R_XTENSA_GNU_VTINHERIT:
	  /* This relocation describes the C++ object vtable hierarchy.
	     Reconstruct it for later use during GC.  */
	  if (!bfd_elf_gc_record_vtinherit (abfd, sec, h, rel->r_offset))
	    return FALSE;
	  break;

	case R_XTENSA_GNU_VTENTRY:
	  /* This relocation describes which C++ vtable entries are actually
	     used.  Record for later use during GC.  */
	  if (!bfd_elf_gc_record_vtentry (abfd, sec, h, rel->r_addend))
	    return FALSE;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}


static void
elf_xtensa_make_sym_local (struct bfd_link_info *info,
			   struct elf_link_hash_entry *h)
{
  if (info->shared)
    {
      if (h->plt.refcount > 0)
	{
	  /* Will use RELATIVE relocs instead of JMP_SLOT relocs.  */
	  if (h->got.refcount < 0)
	    h->got.refcount = 0;
	  h->got.refcount += h->plt.refcount;
	  h->plt.refcount = 0;
	}
    }
  else
    {
      /* Don't need any dynamic relocations at all.  */
      h->plt.refcount = 0;
      h->got.refcount = 0;
    }
}


static void
elf_xtensa_hide_symbol (struct bfd_link_info *info,
			struct elf_link_hash_entry *h,
			bfd_boolean force_local)
{
  /* For a shared link, move the plt refcount to the got refcount to leave
     space for RELATIVE relocs.  */
  elf_xtensa_make_sym_local (info, h);

  _bfd_elf_link_hash_hide_symbol (info, h, force_local);
}


/* Return the section that should be marked against GC for a given
   relocation.  */

static asection *
elf_xtensa_gc_mark_hook (asection *sec,
			 struct bfd_link_info *info ATTRIBUTE_UNUSED,
			 Elf_Internal_Rela *rel,
			 struct elf_link_hash_entry *h,
			 Elf_Internal_Sym *sym)
{
  if (h)
    {
      switch (ELF32_R_TYPE (rel->r_info))
	{
	case R_XTENSA_GNU_VTINHERIT:
	case R_XTENSA_GNU_VTENTRY:
	  break;

	default:
	  switch (h->root.type)
	    {
	    case bfd_link_hash_defined:
	    case bfd_link_hash_defweak:
	      return h->root.u.def.section;

	    case bfd_link_hash_common:
	      return h->root.u.c.p->section;

	    default:
	      break;
	    }
	}
    }
  else
    return bfd_section_from_elf_index (sec->owner, sym->st_shndx);

  return NULL;
}


/* Update the GOT & PLT entry reference counts
   for the section being removed.  */

static bfd_boolean
elf_xtensa_gc_sweep_hook (bfd *abfd,
			  struct bfd_link_info *info ATTRIBUTE_UNUSED,
			  asection *sec,
			  const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  bfd_signed_vma *local_got_refcounts;
  const Elf_Internal_Rela *rel, *relend;

  if ((sec->flags & SEC_ALLOC) == 0)
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);
  local_got_refcounts = elf_local_got_refcounts (abfd);

  relend = relocs + sec->reloc_count;
  for (rel = relocs; rel < relend; rel++)
    {
      unsigned long r_symndx;
      unsigned int r_type;
      struct elf_link_hash_entry *h = NULL;

      r_symndx = ELF32_R_SYM (rel->r_info);
      if (r_symndx >= symtab_hdr->sh_info)
	{
	  h = sym_hashes[r_symndx - symtab_hdr->sh_info];
	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;
	}

      r_type = ELF32_R_TYPE (rel->r_info);
      switch (r_type)
	{
	case R_XTENSA_32:
	  if (h == NULL)
	    goto local_literal;
	  if (h->got.refcount > 0)
	    h->got.refcount--;
	  break;

	case R_XTENSA_PLT:
	  if (h == NULL)
	    goto local_literal;
	  if (h->plt.refcount > 0)
	    h->plt.refcount--;
	  break;

	local_literal:
	  if (local_got_refcounts[r_symndx] > 0)
	    local_got_refcounts[r_symndx] -= 1;
	  break;

	default:
	  break;
	}
    }

  return TRUE;
}


/* Create all the dynamic sections.  */

static bfd_boolean
elf_xtensa_create_dynamic_sections (bfd *dynobj, struct bfd_link_info *info)
{
  flagword flags, noalloc_flags;
  asection *s;

  /* First do all the standard stuff.  */
  if (! _bfd_elf_create_dynamic_sections (dynobj, info))
    return FALSE;

  /* Create any extra PLT sections in case check_relocs has already
     been called on all the non-dynamic input files.  */
  if (!add_extra_plt_sections (dynobj, plt_reloc_count))
    return FALSE;

  noalloc_flags = (SEC_HAS_CONTENTS | SEC_IN_MEMORY
		   | SEC_LINKER_CREATED | SEC_READONLY);
  flags = noalloc_flags | SEC_ALLOC | SEC_LOAD;

  /* Mark the ".got.plt" section READONLY.  */
  s = bfd_get_section_by_name (dynobj, ".got.plt");
  if (s == NULL
      || ! bfd_set_section_flags (dynobj, s, flags))
    return FALSE;

  /* Create ".rela.got".  */
  s = bfd_make_section_with_flags (dynobj, ".rela.got", flags);
  if (s == NULL
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  /* Create ".got.loc" (literal tables for use by dynamic linker).  */
  s = bfd_make_section_with_flags (dynobj, ".got.loc", flags);
  if (s == NULL
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  /* Create ".xt.lit.plt" (literal table for ".got.plt*").  */
  s = bfd_make_section_with_flags (dynobj, ".xt.lit.plt",
				   noalloc_flags);
  if (s == NULL
      || ! bfd_set_section_alignment (dynobj, s, 2))
    return FALSE;

  return TRUE;
}


static bfd_boolean
add_extra_plt_sections (bfd *dynobj, int count)
{
  int chunk;

  /* Iterate over all chunks except 0 which uses the standard ".plt" and
     ".got.plt" sections.  */
  for (chunk = count / PLT_ENTRIES_PER_CHUNK; chunk > 0; chunk--)
    {
      char *sname;
      flagword flags;
      asection *s;

      /* Stop when we find a section has already been created.  */
      if (elf_xtensa_get_plt_section (dynobj, chunk))
	break;

      flags = (SEC_ALLOC | SEC_LOAD | SEC_HAS_CONTENTS | SEC_IN_MEMORY
	       | SEC_LINKER_CREATED | SEC_READONLY);

      sname = (char *) bfd_malloc (10);
      sprintf (sname, ".plt.%u", chunk);
      s = bfd_make_section_with_flags (dynobj, sname,
				       flags | SEC_CODE);
      if (s == NULL
	  || ! bfd_set_section_alignment (dynobj, s, 2))
	return FALSE;

      sname = (char *) bfd_malloc (14);
      sprintf (sname, ".got.plt.%u", chunk);
      s = bfd_make_section_with_flags (dynobj, sname, flags);
      if (s == NULL
	  || ! bfd_set_section_alignment (dynobj, s, 2))
	return FALSE;
    }

  return TRUE;
}


/* Adjust a symbol defined by a dynamic object and referenced by a
   regular object.  The current definition is in some section of the
   dynamic object, but we're not including those sections.  We have to
   change the definition to something the rest of the link can
   understand.  */

static bfd_boolean
elf_xtensa_adjust_dynamic_symbol (struct bfd_link_info *info ATTRIBUTE_UNUSED,
				  struct elf_link_hash_entry *h)
{
  /* If this is a weak symbol, and there is a real definition, the
     processor independent code will have arranged for us to see the
     real definition first, and we can just use the same value.  */
  if (h->u.weakdef)
    {
      BFD_ASSERT (h->u.weakdef->root.type == bfd_link_hash_defined
		  || h->u.weakdef->root.type == bfd_link_hash_defweak);
      h->root.u.def.section = h->u.weakdef->root.u.def.section;
      h->root.u.def.value = h->u.weakdef->root.u.def.value;
      return TRUE;
    }

  /* This is a reference to a symbol defined by a dynamic object.  The
     reference must go through the GOT, so there's no need for COPY relocs,
     .dynbss, etc.  */

  return TRUE;
}


static bfd_boolean
elf_xtensa_fix_refcounts (struct elf_link_hash_entry *h, void *arg)
{
  struct bfd_link_info *info = (struct bfd_link_info *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (! xtensa_elf_dynamic_symbol_p (h, info))
    elf_xtensa_make_sym_local (info, h);

  return TRUE;
}


static bfd_boolean
elf_xtensa_allocate_plt_size (struct elf_link_hash_entry *h, void *arg)
{
  asection *srelplt = (asection *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->plt.refcount > 0)
    srelplt->size += (h->plt.refcount * sizeof (Elf32_External_Rela));

  return TRUE;
}


static bfd_boolean
elf_xtensa_allocate_got_size (struct elf_link_hash_entry *h, void *arg)
{
  asection *srelgot = (asection *) arg;

  if (h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;

  if (h->got.refcount > 0)
    srelgot->size += (h->got.refcount * sizeof (Elf32_External_Rela));

  return TRUE;
}


static void
elf_xtensa_allocate_local_got_size (struct bfd_link_info *info,
				    asection *srelgot)
{
  bfd *i;

  for (i = info->input_bfds; i; i = i->link_next)
    {
      bfd_signed_vma *local_got_refcounts;
      bfd_size_type j, cnt;
      Elf_Internal_Shdr *symtab_hdr;

      local_got_refcounts = elf_local_got_refcounts (i);
      if (!local_got_refcounts)
	continue;

      symtab_hdr = &elf_tdata (i)->symtab_hdr;
      cnt = symtab_hdr->sh_info;

      for (j = 0; j < cnt; ++j)
	{
	  if (local_got_refcounts[j] > 0)
	    srelgot->size += (local_got_refcounts[j]
			      * sizeof (Elf32_External_Rela));
	}
    }
}


/* Set the sizes of the dynamic sections.  */

static bfd_boolean
elf_xtensa_size_dynamic_sections (bfd *output_bfd ATTRIBUTE_UNUSED,
				  struct bfd_link_info *info)
{
  bfd *dynobj, *abfd;
  asection *s, *srelplt, *splt, *sgotplt, *srelgot, *spltlittbl, *sgotloc;
  bfd_boolean relplt, relgot;
  int plt_entries, plt_chunks, chunk;

  plt_entries = 0;
  plt_chunks = 0;
  srelgot = 0;

  dynobj = elf_hash_table (info)->dynobj;
  if (dynobj == NULL)
    abort ();

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Set the contents of the .interp section to the interpreter.  */
      if (info->executable)
	{
	  s = bfd_get_section_by_name (dynobj, ".interp");
	  if (s == NULL)
	    abort ();
	  s->size = sizeof ELF_DYNAMIC_INTERPRETER;
	  s->contents = (unsigned char *) ELF_DYNAMIC_INTERPRETER;
	}

      /* Allocate room for one word in ".got".  */
      s = bfd_get_section_by_name (dynobj, ".got");
      if (s == NULL)
	abort ();
      s->size = 4;

      /* Adjust refcounts for symbols that we now know are not "dynamic".  */
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_fix_refcounts,
			      (void *) info);

      /* Allocate space in ".rela.got" for literals that reference
	 global symbols.  */
      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
      if (srelgot == NULL)
	abort ();
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_allocate_got_size,
			      (void *) srelgot);

      /* If we are generating a shared object, we also need space in
	 ".rela.got" for R_XTENSA_RELATIVE relocs for literals that
	 reference local symbols.  */
      if (info->shared)
	elf_xtensa_allocate_local_got_size (info, srelgot);

      /* Allocate space in ".rela.plt" for literals that have PLT entries.  */
      srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
      if (srelplt == NULL)
	abort ();
      elf_link_hash_traverse (elf_hash_table (info),
			      elf_xtensa_allocate_plt_size,
			      (void *) srelplt);

      /* Allocate space in ".plt" to match the size of ".rela.plt".  For
	 each PLT entry, we need the PLT code plus a 4-byte literal.
	 For each chunk of ".plt", we also need two more 4-byte
	 literals, two corresponding entries in ".rela.got", and an
	 8-byte entry in ".xt.lit.plt".  */
      spltlittbl = bfd_get_section_by_name (dynobj, ".xt.lit.plt");
      if (spltlittbl == NULL)
	abort ();

      plt_entries = srelplt->size / sizeof (Elf32_External_Rela);
      plt_chunks =
	(plt_entries + PLT_ENTRIES_PER_CHUNK - 1) / PLT_ENTRIES_PER_CHUNK;

      /* Iterate over all the PLT chunks, including any extra sections
	 created earlier because the initial count of PLT relocations
	 was an overestimate.  */
      for (chunk = 0;
	   (splt = elf_xtensa_get_plt_section (dynobj, chunk)) != NULL;
	   chunk++)
	{
	  int chunk_entries;

	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  if (sgotplt == NULL)
	    abort ();

	  if (chunk < plt_chunks - 1)
	    chunk_entries = PLT_ENTRIES_PER_CHUNK;
	  else if (chunk == plt_chunks - 1)
	    chunk_entries = plt_entries - (chunk * PLT_ENTRIES_PER_CHUNK);
	  else
	    chunk_entries = 0;

	  if (chunk_entries != 0)
	    {
	      sgotplt->size = 4 * (chunk_entries + 2);
	      splt->size = PLT_ENTRY_SIZE * chunk_entries;
	      srelgot->size += 2 * sizeof (Elf32_External_Rela);
	      spltlittbl->size += 8;
	    }
	  else
	    {
	      sgotplt->size = 0;
	      splt->size = 0;
	    }
	}

      /* Allocate space in ".got.loc" to match the total size of all the
	 literal tables.  */
      sgotloc = bfd_get_section_by_name (dynobj, ".got.loc");
      if (sgotloc == NULL)
	abort ();
      sgotloc->size = spltlittbl->size;
      for (abfd = info->input_bfds; abfd != NULL; abfd = abfd->link_next)
	{
	  if (abfd->flags & DYNAMIC)
	    continue;
	  for (s = abfd->sections; s != NULL; s = s->next)
	    {
	      if (! elf_discarded_section (s)
		  && xtensa_is_littable_section (s)
		  && s != spltlittbl)
		sgotloc->size += s->size;
	    }
	}
    }

  /* Allocate memory for dynamic sections.  */
  relplt = FALSE;
  relgot = FALSE;
  for (s = dynobj->sections; s != NULL; s = s->next)
    {
      const char *name;

      if ((s->flags & SEC_LINKER_CREATED) == 0)
	continue;

      /* It's OK to base decisions on the section name, because none
	 of the dynobj section names depend upon the input files.  */
      name = bfd_get_section_name (dynobj, s);

      if (strncmp (name, ".rela", 5) == 0)
	{
	  if (s->size != 0)
	    {
	      if (strcmp (name, ".rela.plt") == 0)
		relplt = TRUE;
	      else if (strcmp (name, ".rela.got") == 0)
		relgot = TRUE;

	      /* We use the reloc_count field as a counter if we need
		 to copy relocs into the output file.  */
	      s->reloc_count = 0;
	    }
	}
      else if (strncmp (name, ".plt.", 5) != 0
	       && strncmp (name, ".got.plt.", 9) != 0
	       && strcmp (name, ".got") != 0
	       && strcmp (name, ".plt") != 0
	       && strcmp (name, ".got.plt") != 0
	       && strcmp (name, ".xt.lit.plt") != 0
	       && strcmp (name, ".got.loc") != 0)
	{
	  /* It's not one of our sections, so don't allocate space.  */
	  continue;
	}

      if (s->size == 0)
	{
	  /* If we don't need this section, strip it from the output
	     file.  We must create the ".plt*" and ".got.plt*"
	     sections in create_dynamic_sections and/or check_relocs
	     based on a conservative estimate of the PLT relocation
	     count, because the sections must be created before the
	     linker maps input sections to output sections.  The
	     linker does that before size_dynamic_sections, where we
	     compute the exact size of the PLT, so there may be more
	     of these sections than are actually needed.  */
	  s->flags |= SEC_EXCLUDE;
	}
      else if ((s->flags & SEC_HAS_CONTENTS) != 0)
	{
	  /* Allocate memory for the section contents.  */
	  s->contents = (bfd_byte *) bfd_zalloc (dynobj, s->size);
	  if (s->contents == NULL)
	    return FALSE;
	}
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      /* Add the special XTENSA_RTLD relocations now.  The offsets won't be
	 known until finish_dynamic_sections, but we need to get the relocs
	 in place before they are sorted.  */
      if (srelgot == NULL)
	abort ();
      for (chunk = 0; chunk < plt_chunks; chunk++)
	{
	  Elf_Internal_Rela irela;
	  bfd_byte *loc;

	  irela.r_offset = 0;
	  irela.r_info = ELF32_R_INFO (0, R_XTENSA_RTLD);
	  irela.r_addend = 0;

	  loc = (srelgot->contents
		 + srelgot->reloc_count * sizeof (Elf32_External_Rela));
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  bfd_elf32_swap_reloca_out (output_bfd, &irela,
				     loc + sizeof (Elf32_External_Rela));
	  srelgot->reloc_count += 2;
	}

      /* Add some entries to the .dynamic section.  We fill in the
	 values later, in elf_xtensa_finish_dynamic_sections, but we
	 must add the entries now so that we get the correct size for
	 the .dynamic section.  The DT_DEBUG entry is filled in by the
	 dynamic linker and used by the debugger.  */
#define add_dynamic_entry(TAG, VAL) \
  _bfd_elf_add_dynamic_entry (info, TAG, VAL)

      if (! info->shared)
	{
	  if (!add_dynamic_entry (DT_DEBUG, 0))
	    return FALSE;
	}

      if (relplt)
	{
	  if (!add_dynamic_entry (DT_PLTGOT, 0)
	      || !add_dynamic_entry (DT_PLTRELSZ, 0)
	      || !add_dynamic_entry (DT_PLTREL, DT_RELA)
	      || !add_dynamic_entry (DT_JMPREL, 0))
	    return FALSE;
	}

      if (relgot)
	{
	  if (!add_dynamic_entry (DT_RELA, 0)
	      || !add_dynamic_entry (DT_RELASZ, 0)
	      || !add_dynamic_entry (DT_RELAENT, sizeof (Elf32_External_Rela)))
	    return FALSE;
	}

      if (!add_dynamic_entry (DT_XTENSA_GOT_LOC_OFF, 0)
	  || !add_dynamic_entry (DT_XTENSA_GOT_LOC_SZ, 0))
	return FALSE;
    }
#undef add_dynamic_entry

  return TRUE;
}


/* Remove any PT_LOAD segments with no allocated sections.  Prior to
   binutils 2.13, this function used to remove the non-SEC_ALLOC
   sections from PT_LOAD segments, but that task has now been moved
   into elf.c.  We still need this function to remove any empty
   segments that result, but there's nothing Xtensa-specific about
   this and it probably ought to be moved into elf.c as well.  */

static bfd_boolean
elf_xtensa_modify_segment_map (bfd *abfd,
			       struct bfd_link_info *info ATTRIBUTE_UNUSED)
{
  struct elf_segment_map **m_p;

  m_p = &elf_tdata (abfd)->segment_map;
  while (*m_p)
    {
      if ((*m_p)->p_type == PT_LOAD && (*m_p)->count == 0)
	*m_p = (*m_p)->next;
      else
	m_p = &(*m_p)->next;
    }
  return TRUE;
}


/* Perform the specified relocation.  The instruction at (contents + address)
   is modified to set one operand to represent the value in "relocation".  The
   operand position is determined by the relocation type recorded in the
   howto.  */

#define CALL_SEGMENT_BITS (30)
#define CALL_SEGMENT_SIZE (1 << CALL_SEGMENT_BITS)

static bfd_reloc_status_type
elf_xtensa_do_reloc (reloc_howto_type *howto,
		     bfd *abfd,
		     asection *input_section,
		     bfd_vma relocation,
		     bfd_byte *contents,
		     bfd_vma address,
		     bfd_boolean is_weak_undef,
		     char **error_message)
{
  xtensa_format fmt;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;
  static xtensa_insnbuf ibuff = NULL;
  static xtensa_insnbuf sbuff = NULL;
  bfd_vma self_address = 0;
  bfd_size_type input_size;
  int opnd, slot;
  uint32 newval;

  if (!ibuff)
    {
      ibuff = xtensa_insnbuf_alloc (isa);
      sbuff = xtensa_insnbuf_alloc (isa);
    }

  input_size = bfd_get_section_limit (abfd, input_section);

  switch (howto->type)
    {
    case R_XTENSA_NONE:
    case R_XTENSA_DIFF8:
    case R_XTENSA_DIFF16:
    case R_XTENSA_DIFF32:
      return bfd_reloc_ok;

    case R_XTENSA_ASM_EXPAND:
      if (!is_weak_undef)
	{
	  /* Check for windowed CALL across a 1GB boundary.  */
	  xtensa_opcode opcode =
	    get_expanded_call_opcode (contents + address,
				      input_size - address, 0);
	  if (is_windowed_call_opcode (opcode))
	    {
	      self_address = (input_section->output_section->vma
			      + input_section->output_offset
			      + address);
	      if ((self_address >> CALL_SEGMENT_BITS)
		  != (relocation >> CALL_SEGMENT_BITS)) 
		{
		  *error_message = "windowed longcall crosses 1GB boundary; "
		    "return may fail";
		  return bfd_reloc_dangerous;
		}
	    }
	}
      return bfd_reloc_ok;

    case R_XTENSA_ASM_SIMPLIFY:
      {
        /* Convert the L32R/CALLX to CALL.  */
	bfd_reloc_status_type retval =
	  elf_xtensa_do_asm_simplify (contents, address, input_size,
				      error_message);
	if (retval != bfd_reloc_ok)
	  return bfd_reloc_dangerous;

	/* The CALL needs to be relocated.  Continue below for that part.  */
	address += 3;
	howto = &elf_howto_table[(unsigned) R_XTENSA_SLOT0_OP ];
      }
      break;

    case R_XTENSA_32:
    case R_XTENSA_PLT:
      {
	bfd_vma x;
	x = bfd_get_32 (abfd, contents + address);
	x = x + relocation;
	bfd_put_32 (abfd, x, contents + address);
      }
      return bfd_reloc_ok;
    }

  /* Only instruction slot-specific relocations handled below.... */
  slot = get_relocation_slot (howto->type);
  if (slot == XTENSA_UNDEFINED)
    {
      *error_message = "unexpected relocation";
      return bfd_reloc_dangerous;
    }

  /* Read the instruction into a buffer and decode the opcode.  */
  xtensa_insnbuf_from_chars (isa, ibuff, contents + address,
			     input_size - address);
  fmt = xtensa_format_decode (isa, ibuff);
  if (fmt == XTENSA_UNDEFINED)
    {
      *error_message = "cannot decode instruction format";
      return bfd_reloc_dangerous;
    }

  xtensa_format_get_slot (isa, fmt, slot, ibuff, sbuff);

  opcode = xtensa_opcode_decode (isa, fmt, slot, sbuff);
  if (opcode == XTENSA_UNDEFINED)
    {
      *error_message = "cannot decode instruction opcode";
      return bfd_reloc_dangerous;
    }

  /* Check for opcode-specific "alternate" relocations.  */
  if (is_alt_relocation (howto->type))
    {
      if (opcode == get_l32r_opcode ())
	{
	  /* Handle the special-case of non-PC-relative L32R instructions.  */
	  bfd *output_bfd = input_section->output_section->owner;
	  asection *lit4_sec = bfd_get_section_by_name (output_bfd, ".lit4");
	  if (!lit4_sec)
	    {
	      *error_message = "relocation references missing .lit4 section";
	      return bfd_reloc_dangerous;
	    }
	  self_address = ((lit4_sec->vma & ~0xfff)
			  + 0x40000 - 3); /* -3 to compensate for do_reloc */
	  newval = relocation;
	  opnd = 1;
	}
      else if (opcode == get_const16_opcode ())
	{
	  /* ALT used for high 16 bits.  */
	  newval = relocation >> 16;
	  opnd = 1;
	}
      else
	{
	  /* No other "alternate" relocations currently defined.  */
	  *error_message = "unexpected relocation";
	  return bfd_reloc_dangerous;
	}
    }
  else /* Not an "alternate" relocation.... */
    {
      if (opcode == get_const16_opcode ())
	{
	  newval = relocation & 0xffff;
	  opnd = 1;
	}
      else
	{
	  /* ...normal PC-relative relocation.... */

	  /* Determine which operand is being relocated.  */
	  opnd = get_relocation_opnd (opcode, howto->type);
	  if (opnd == XTENSA_UNDEFINED)
	    {
	      *error_message = "unexpected relocation";
	      return bfd_reloc_dangerous;
	    }

	  if (!howto->pc_relative)
	    {
	      *error_message = "expected PC-relative relocation";
	      return bfd_reloc_dangerous;
	    }

	  /* Calculate the PC address for this instruction.  */
	  self_address = (input_section->output_section->vma
			  + input_section->output_offset
			  + address);

	  newval = relocation;
	}
    }

  /* Apply the relocation.  */
  if (xtensa_operand_do_reloc (isa, opcode, opnd, &newval, self_address)
      || xtensa_operand_encode (isa, opcode, opnd, &newval)
      || xtensa_operand_set_field (isa, opcode, opnd, fmt, slot,
				   sbuff, newval))
    {
      const char *opname = xtensa_opcode_name (isa, opcode);
      const char *msg;

      msg = "cannot encode";
      if (is_direct_call_opcode (opcode))
	{
	  if ((relocation & 0x3) != 0)
	    msg = "misaligned call target";
	  else
	    msg = "call target out of range";
	}
      else if (opcode == get_l32r_opcode ())
	{
	  if ((relocation & 0x3) != 0)
	    msg = "misaligned literal target";
	  else if (is_alt_relocation (howto->type))
	    msg = "literal target out of range (too many literals)";
	  else if (self_address > relocation)
	    msg = "literal target out of range (try using text-section-literals)";
	  else
	    msg = "literal placed after use";
	}

      *error_message = vsprint_msg (opname, ": %s", strlen (msg) + 2, msg);
      return bfd_reloc_dangerous;
    }

  /* Check for calls across 1GB boundaries.  */
  if (is_direct_call_opcode (opcode)
      && is_windowed_call_opcode (opcode))
    {
      if ((self_address >> CALL_SEGMENT_BITS)
	  != (relocation >> CALL_SEGMENT_BITS)) 
	{
	  *error_message =
	    "windowed call crosses 1GB boundary; return may fail";
	  return bfd_reloc_dangerous;
	}
    }

  /* Write the modified instruction back out of the buffer.  */
  xtensa_format_set_slot (isa, fmt, slot, ibuff, sbuff);
  xtensa_insnbuf_to_chars (isa, ibuff, contents + address,
			   input_size - address);
  return bfd_reloc_ok;
}


static char *
vsprint_msg (const char *origmsg, const char *fmt, int arglen, ...)
{
  /* To reduce the size of the memory leak,
     we only use a single message buffer.  */
  static bfd_size_type alloc_size = 0;
  static char *message = NULL;
  bfd_size_type orig_len, len = 0;
  bfd_boolean is_append;

  VA_OPEN (ap, arglen);
  VA_FIXEDARG (ap, const char *, origmsg);
  
  is_append = (origmsg == message);  

  orig_len = strlen (origmsg);
  len = orig_len + strlen (fmt) + arglen + 20;
  if (len > alloc_size)
    {
      message = (char *) bfd_realloc (message, len);
      alloc_size = len;
    }
  if (!is_append)
    memcpy (message, origmsg, orig_len);
  vsprintf (message + orig_len, fmt, ap);
  VA_CLOSE (ap);
  return message;
}


/* This function is registered as the "special_function" in the
   Xtensa howto for handling simplify operations.
   bfd_perform_relocation / bfd_install_relocation use it to
   perform (install) the specified relocation.  Since this replaces the code
   in bfd_perform_relocation, it is basically an Xtensa-specific,
   stripped-down version of bfd_perform_relocation.  */

static bfd_reloc_status_type
bfd_elf_xtensa_reloc (bfd *abfd,
		      arelent *reloc_entry,
		      asymbol *symbol,
		      void *data,
		      asection *input_section,
		      bfd *output_bfd,
		      char **error_message)
{
  bfd_vma relocation;
  bfd_reloc_status_type flag;
  bfd_size_type octets = reloc_entry->address * bfd_octets_per_byte (abfd);
  bfd_vma output_base = 0;
  reloc_howto_type *howto = reloc_entry->howto;
  asection *reloc_target_output_section;
  bfd_boolean is_weak_undef;

  if (!xtensa_default_isa)
    xtensa_default_isa = xtensa_isa_init (0, 0);

  /* ELF relocs are against symbols.  If we are producing relocatable
     output, and the reloc is against an external symbol, the resulting
     reloc will also be against the same symbol.  In such a case, we
     don't want to change anything about the way the reloc is handled,
     since it will all be done at final link time.  This test is similar
     to what bfd_elf_generic_reloc does except that it lets relocs with
     howto->partial_inplace go through even if the addend is non-zero.
     (The real problem is that partial_inplace is set for XTENSA_32
     relocs to begin with, but that's a long story and there's little we
     can do about it now....)  */

  if (output_bfd && (symbol->flags & BSF_SECTION_SYM) == 0)
    {
      reloc_entry->address += input_section->output_offset;
      return bfd_reloc_ok;
    }

  /* Is the address of the relocation really within the section?  */
  if (reloc_entry->address > bfd_get_section_limit (abfd, input_section))
    return bfd_reloc_outofrange;

  /* Work out which section the relocation is targeted at and the
     initial relocation command value.  */

  /* Get symbol value.  (Common symbols are special.)  */
  if (bfd_is_com_section (symbol->section))
    relocation = 0;
  else
    relocation = symbol->value;

  reloc_target_output_section = symbol->section->output_section;

  /* Convert input-section-relative symbol value to absolute.  */
  if ((output_bfd && !howto->partial_inplace)
      || reloc_target_output_section == NULL)
    output_base = 0;
  else
    output_base = reloc_target_output_section->vma;

  relocation += output_base + symbol->section->output_offset;

  /* Add in supplied addend.  */
  relocation += reloc_entry->addend;

  /* Here the variable relocation holds the final address of the
     symbol we are relocating against, plus any addend.  */
  if (output_bfd)
    {
      if (!howto->partial_inplace)
	{
	  /* This is a partial relocation, and we want to apply the relocation
	     to the reloc entry rather than the raw data.  Everything except
	     relocations against section symbols has already been handled
	     above.  */

	  BFD_ASSERT (symbol->flags & BSF_SECTION_SYM);
	  reloc_entry->addend = relocation;
	  reloc_entry->address += input_section->output_offset;
	  return bfd_reloc_ok;
	}
      else
	{
	  reloc_entry->address += input_section->output_offset;
	  reloc_entry->addend = 0;
	}
    }

  is_weak_undef = (bfd_is_und_section (symbol->section)
		   && (symbol->flags & BSF_WEAK) != 0);
  flag = elf_xtensa_do_reloc (howto, abfd, input_section, relocation,
			      (bfd_byte *) data, (bfd_vma) octets,
			      is_weak_undef, error_message);

  if (flag == bfd_reloc_dangerous)
    {
      /* Add the symbol name to the error message.  */
      if (! *error_message)
	*error_message = "";
      *error_message = vsprint_msg (*error_message, ": (%s + 0x%lx)",
				    strlen (symbol->name) + 17,
				    symbol->name,
				    (unsigned long) reloc_entry->addend);
    }

  return flag;
}


/* Set up an entry in the procedure linkage table.  */

static bfd_vma
elf_xtensa_create_plt_entry (bfd *dynobj,
			     bfd *output_bfd,
			     unsigned reloc_index)
{
  asection *splt, *sgotplt;
  bfd_vma plt_base, got_base;
  bfd_vma code_offset, lit_offset;
  int chunk;

  chunk = reloc_index / PLT_ENTRIES_PER_CHUNK;
  splt = elf_xtensa_get_plt_section (dynobj, chunk);
  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
  BFD_ASSERT (splt != NULL && sgotplt != NULL);

  plt_base = splt->output_section->vma + splt->output_offset;
  got_base = sgotplt->output_section->vma + sgotplt->output_offset;

  lit_offset = 8 + (reloc_index % PLT_ENTRIES_PER_CHUNK) * 4;
  code_offset = (reloc_index % PLT_ENTRIES_PER_CHUNK) * PLT_ENTRY_SIZE;

  /* Fill in the literal entry.  This is the offset of the dynamic
     relocation entry.  */
  bfd_put_32 (output_bfd, reloc_index * sizeof (Elf32_External_Rela),
	      sgotplt->contents + lit_offset);

  /* Fill in the entry in the procedure linkage table.  */
  memcpy (splt->contents + code_offset,
	  (bfd_big_endian (output_bfd)
	   ? elf_xtensa_be_plt_entry
	   : elf_xtensa_le_plt_entry),
	  PLT_ENTRY_SIZE);
  bfd_put_16 (output_bfd, l32r_offset (got_base + 0,
				       plt_base + code_offset + 3),
	      splt->contents + code_offset + 4);
  bfd_put_16 (output_bfd, l32r_offset (got_base + 4,
				       plt_base + code_offset + 6),
	      splt->contents + code_offset + 7);
  bfd_put_16 (output_bfd, l32r_offset (got_base + lit_offset,
				       plt_base + code_offset + 9),
	      splt->contents + code_offset + 10);

  return plt_base + code_offset;
}


/* Relocate an Xtensa ELF section.  This is invoked by the linker for
   both relocatable and final links.  */

static bfd_boolean
elf_xtensa_relocate_section (bfd *output_bfd,
			     struct bfd_link_info *info,
			     bfd *input_bfd,
			     asection *input_section,
			     bfd_byte *contents,
			     Elf_Internal_Rela *relocs,
			     Elf_Internal_Sym *local_syms,
			     asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;
  struct elf_link_hash_entry **sym_hashes;
  asection *srelgot, *srelplt;
  bfd *dynobj;
  property_table_entry *lit_table = 0;
  int ltblsize = 0;
  char *error_message = NULL;
  bfd_size_type input_size;

  if (!xtensa_default_isa)
    xtensa_default_isa = xtensa_isa_init (0, 0);

  dynobj = elf_hash_table (info)->dynobj;
  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);

  srelgot = NULL;
  srelplt = NULL;
  if (dynobj)
    {
      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");;
      srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
    }

  if (elf_hash_table (info)->dynamic_sections_created)
    {
      ltblsize = xtensa_read_table_entries (input_bfd, input_section,
					    &lit_table, XTENSA_LIT_SEC_NAME,
					    TRUE);
      if (ltblsize < 0)
	return FALSE;
    }

  input_size = bfd_get_section_limit (input_bfd, input_section);

  rel = relocs;
  relend = relocs + input_section->reloc_count;
  for (; rel < relend; rel++)
    {
      int r_type;
      reloc_howto_type *howto;
      unsigned long r_symndx;
      struct elf_link_hash_entry *h;
      Elf_Internal_Sym *sym;
      asection *sec;
      bfd_vma relocation;
      bfd_reloc_status_type r;
      bfd_boolean is_weak_undef;
      bfd_boolean unresolved_reloc;
      bfd_boolean warned;

      r_type = ELF32_R_TYPE (rel->r_info);
      if (r_type == (int) R_XTENSA_GNU_VTINHERIT
	  || r_type == (int) R_XTENSA_GNU_VTENTRY)
	continue;

      if (r_type < 0 || r_type >= (int) R_XTENSA_max)
	{
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}
      howto = &elf_howto_table[r_type];

      r_symndx = ELF32_R_SYM (rel->r_info);

      if (info->relocatable)
	{
	  /* This is a relocatable link.
	     1) If the reloc is against a section symbol, adjust
	     according to the output section.
	     2) If there is a new target for this relocation,
	     the new target will be in the same output section.
	     We adjust the relocation by the output section
	     difference.  */

	  if (relaxing_section)
	    {
	      /* Check if this references a section in another input file.  */
	      if (!do_fix_for_relocatable_link (rel, input_bfd, input_section,
						contents))
		return FALSE;
	      r_type = ELF32_R_TYPE (rel->r_info);
	    }

	  if (r_type == R_XTENSA_ASM_SIMPLIFY)
	    {
	      char *error_message = NULL;
	      /* Convert ASM_SIMPLIFY into the simpler relocation
		 so that they never escape a relaxing link.  */
	      r = contract_asm_expansion (contents, input_size, rel,
					  &error_message);
	      if (r != bfd_reloc_ok)
		{
		  if (!((*info->callbacks->reloc_dangerous)
			(info, error_message, input_bfd, input_section,
			 rel->r_offset)))
		    return FALSE;
		}
	      r_type = ELF32_R_TYPE (rel->r_info);
	    }

	  /* This is a relocatable link, so we don't have to change
	     anything unless the reloc is against a section symbol,
	     in which case we have to adjust according to where the
	     section symbol winds up in the output section.  */
	  if (r_symndx < symtab_hdr->sh_info)
	    {
	      sym = local_syms + r_symndx;
	      if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
		{
		  sec = local_sections[r_symndx];
		  rel->r_addend += sec->output_offset + sym->st_value;
		}
	    }

	  /* If there is an addend with a partial_inplace howto,
	     then move the addend to the contents.  This is a hack
	     to work around problems with DWARF in relocatable links
	     with some previous version of BFD.  Now we can't easily get
	     rid of the hack without breaking backward compatibility.... */
	  if (rel->r_addend)
	    {
	      howto = &elf_howto_table[r_type];
	      if (howto->partial_inplace)
		{
		  r = elf_xtensa_do_reloc (howto, input_bfd, input_section,
					   rel->r_addend, contents,
					   rel->r_offset, FALSE,
					   &error_message);
		  if (r != bfd_reloc_ok)
		    {
		      if (!((*info->callbacks->reloc_dangerous)
			    (info, error_message, input_bfd, input_section,
			     rel->r_offset)))
			return FALSE;
		    }
		  rel->r_addend = 0;
		}
	    }

	  /* Done with work for relocatable link; continue with next reloc.  */
	  continue;
	}

      /* This is a final link.  */

      h = NULL;
      sym = NULL;
      sec = NULL;
      is_weak_undef = FALSE;
      unresolved_reloc = FALSE;
      warned = FALSE;

      if (howto->partial_inplace)
	{
	  /* Because R_XTENSA_32 was made partial_inplace to fix some
	     problems with DWARF info in partial links, there may be
	     an addend stored in the contents.  Take it out of there
	     and move it back into the addend field of the reloc.  */
	  rel->r_addend += bfd_get_32 (input_bfd, contents + rel->r_offset);
	  bfd_put_32 (input_bfd, 0, contents + rel->r_offset);
	}

      if (r_symndx < symtab_hdr->sh_info)
	{
	  sym = local_syms + r_symndx;
	  sec = local_sections[r_symndx];
	  relocation = _bfd_elf_rela_local_sym (output_bfd, sym, &sec, rel);
	}
      else
	{
	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned);

	  if (relocation == 0
	      && !unresolved_reloc
	      && h->root.type == bfd_link_hash_undefweak)
	    is_weak_undef = TRUE;
	}

      if (relaxing_section)
	{
	  /* Check if this references a section in another input file.  */
	  do_fix_for_final_link (rel, input_bfd, input_section, contents,
				 &relocation);

	  /* Update some already cached values.  */
	  r_type = ELF32_R_TYPE (rel->r_info);
	  howto = &elf_howto_table[r_type];
	}

      /* Sanity check the address.  */
      if (rel->r_offset >= input_size
	  && ELF32_R_TYPE (rel->r_info) != R_XTENSA_NONE)
	{
	  (*_bfd_error_handler)
	    (_("%B(%A+0x%lx): relocation offset out of range (size=0x%x)"),
	     input_bfd, input_section, rel->r_offset, input_size);
	  bfd_set_error (bfd_error_bad_value);
	  return FALSE;
	}

      /* Generate dynamic relocations.  */
      if (elf_hash_table (info)->dynamic_sections_created)
	{
	  bfd_boolean dynamic_symbol = xtensa_elf_dynamic_symbol_p (h, info);

	  if (dynamic_symbol && is_operand_relocation (r_type))
	    {
	      /* This is an error.  The symbol's real value won't be known
		 until runtime and it's likely to be out of range anyway.  */
	      const char *name = h->root.root.string;
	      error_message = vsprint_msg ("invalid relocation for dynamic "
					   "symbol", ": %s",
					   strlen (name) + 2, name);
	      if (!((*info->callbacks->reloc_dangerous)
		    (info, error_message, input_bfd, input_section,
		     rel->r_offset)))
		return FALSE;
	    }
	  else if ((r_type == R_XTENSA_32 || r_type == R_XTENSA_PLT)
		   && (input_section->flags & SEC_ALLOC) != 0
		   && (dynamic_symbol || info->shared))
	    {
	      Elf_Internal_Rela outrel;
	      bfd_byte *loc;
	      asection *srel;

	      if (dynamic_symbol && r_type == R_XTENSA_PLT)
		srel = srelplt;
	      else
		srel = srelgot;

	      BFD_ASSERT (srel != NULL);

	      outrel.r_offset =
		_bfd_elf_section_offset (output_bfd, info,
					 input_section, rel->r_offset);

	      if ((outrel.r_offset | 1) == (bfd_vma) -1)
		memset (&outrel, 0, sizeof outrel);
	      else
		{
		  outrel.r_offset += (input_section->output_section->vma
				      + input_section->output_offset);

		  /* Complain if the relocation is in a read-only section
		     and not in a literal pool.  */
		  if ((input_section->flags & SEC_READONLY) != 0
		      && !elf_xtensa_in_literal_pool (lit_table, ltblsize,
						      outrel.r_offset))
		    {
		      error_message =
			_("dynamic relocation in read-only section");
		      if (!((*info->callbacks->reloc_dangerous)
			    (info, error_message, input_bfd, input_section,
			     rel->r_offset)))
			return FALSE;
		    }

		  if (dynamic_symbol)
		    {
		      outrel.r_addend = rel->r_addend;
		      rel->r_addend = 0;

		      if (r_type == R_XTENSA_32)
			{
			  outrel.r_info =
			    ELF32_R_INFO (h->dynindx, R_XTENSA_GLOB_DAT);
			  relocation = 0;
			}
		      else /* r_type == R_XTENSA_PLT */
			{
			  outrel.r_info =
			    ELF32_R_INFO (h->dynindx, R_XTENSA_JMP_SLOT);

			  /* Create the PLT entry and set the initial
			     contents of the literal entry to the address of
			     the PLT entry.  */
			  relocation =
			    elf_xtensa_create_plt_entry (dynobj, output_bfd,
							 srel->reloc_count);
			}
		      unresolved_reloc = FALSE;
		    }
		  else
		    {
		      /* Generate a RELATIVE relocation.  */
		      outrel.r_info = ELF32_R_INFO (0, R_XTENSA_RELATIVE);
		      outrel.r_addend = 0;
		    }
		}

	      loc = (srel->contents
		     + srel->reloc_count++ * sizeof (Elf32_External_Rela));
	      bfd_elf32_swap_reloca_out (output_bfd, &outrel, loc);
	      BFD_ASSERT (sizeof (Elf32_External_Rela) * srel->reloc_count
			  <= srel->size);
	    }
	}

      /* Dynamic relocs are not propagated for SEC_DEBUGGING sections
	 because such sections are not SEC_ALLOC and thus ld.so will
	 not process them.  */
      if (unresolved_reloc
	  && !((input_section->flags & SEC_DEBUGGING) != 0
	       && h->def_dynamic))
	(*_bfd_error_handler)
	  (_("%B(%A+0x%lx): unresolvable %s relocation against symbol `%s'"),
	   input_bfd,
	   input_section,
	   (long) rel->r_offset,
	   howto->name,
	   h->root.root.string);

      /* There's no point in calling bfd_perform_relocation here.
	 Just go directly to our "special function".  */
      r = elf_xtensa_do_reloc (howto, input_bfd, input_section,
			       relocation + rel->r_addend,
			       contents, rel->r_offset, is_weak_undef,
			       &error_message);

      if (r != bfd_reloc_ok && !warned)
	{
	  const char *name;

	  BFD_ASSERT (r == bfd_reloc_dangerous || r == bfd_reloc_other);
	  BFD_ASSERT (error_message != NULL);

	  if (h)
	    name = h->root.root.string;
	  else
	    {
	      name = bfd_elf_string_from_elf_section
		(input_bfd, symtab_hdr->sh_link, sym->st_name);
	      if (name && *name == '\0')
		name = bfd_section_name (input_bfd, sec);
	    }
	  if (name)
	    {
	      if (rel->r_addend == 0)
		error_message = vsprint_msg (error_message, ": %s",
					     strlen (name) + 2, name);
	      else
		error_message = vsprint_msg (error_message, ": (%s+0x%x)",
					     strlen (name) + 22,
					     name, (int)rel->r_addend);
	    }

	  if (!((*info->callbacks->reloc_dangerous)
		(info, error_message, input_bfd, input_section,
		 rel->r_offset)))
	    return FALSE;
	}
    }

  if (lit_table)
    free (lit_table);

  input_section->reloc_done = TRUE;

  return TRUE;
}


/* Finish up dynamic symbol handling.  There's not much to do here since
   the PLT and GOT entries are all set up by relocate_section.  */

static bfd_boolean
elf_xtensa_finish_dynamic_symbol (bfd *output_bfd ATTRIBUTE_UNUSED,
				  struct bfd_link_info *info ATTRIBUTE_UNUSED,
				  struct elf_link_hash_entry *h,
				  Elf_Internal_Sym *sym)
{
  if (h->needs_plt
      && !h->def_regular)
    {
      /* Mark the symbol as undefined, rather than as defined in
	 the .plt section.  Leave the value alone.  */
      sym->st_shndx = SHN_UNDEF;
    }

  /* Mark _DYNAMIC and _GLOBAL_OFFSET_TABLE_ as absolute.  */
  if (strcmp (h->root.root.string, "_DYNAMIC") == 0
      || h == elf_hash_table (info)->hgot)
    sym->st_shndx = SHN_ABS;

  return TRUE;
}


/* Combine adjacent literal table entries in the output.  Adjacent
   entries within each input section may have been removed during
   relaxation, but we repeat the process here, even though it's too late
   to shrink the output section, because it's important to minimize the
   number of literal table entries to reduce the start-up work for the
   runtime linker.  Returns the number of remaining table entries or -1
   on error.  */

static int
elf_xtensa_combine_prop_entries (bfd *output_bfd,
				 asection *sxtlit,
				 asection *sgotloc)
{
  bfd_byte *contents;
  property_table_entry *table;
  bfd_size_type section_size, sgotloc_size;
  bfd_vma offset;
  int n, m, num;

  section_size = sxtlit->size;
  BFD_ASSERT (section_size % 8 == 0);
  num = section_size / 8;

  sgotloc_size = sgotloc->size;
  if (sgotloc_size != section_size)
    {
      (*_bfd_error_handler)
	(_("internal inconsistency in size of .got.loc section"));
      return -1;
    }

  table = bfd_malloc (num * sizeof (property_table_entry));
  if (table == 0)
    return -1;

  /* The ".xt.lit.plt" section has the SEC_IN_MEMORY flag set and this
     propagates to the output section, where it doesn't really apply and
     where it breaks the following call to bfd_malloc_and_get_section.  */
  sxtlit->flags &= ~SEC_IN_MEMORY;

  if (!bfd_malloc_and_get_section (output_bfd, sxtlit, &contents))
    {
      if (contents != 0)
	free (contents);
      free (table);
      return -1;
    }

  /* There should never be any relocations left at this point, so this
     is quite a bit easier than what is done during relaxation.  */

  /* Copy the raw contents into a property table array and sort it.  */
  offset = 0;
  for (n = 0; n < num; n++)
    {
      table[n].address = bfd_get_32 (output_bfd, &contents[offset]);
      table[n].size = bfd_get_32 (output_bfd, &contents[offset + 4]);
      offset += 8;
    }
  qsort (table, num, sizeof (property_table_entry), property_table_compare);

  for (n = 0; n < num; n++)
    {
      bfd_boolean remove = FALSE;

      if (table[n].size == 0)
	remove = TRUE;
      else if (n > 0 &&
	       (table[n-1].address + table[n-1].size == table[n].address))
	{
	  table[n-1].size += table[n].size;
	  remove = TRUE;
	}

      if (remove)
	{
	  for (m = n; m < num - 1; m++)
	    {
	      table[m].address = table[m+1].address;
	      table[m].size = table[m+1].size;
	    }

	  n--;
	  num--;
	}
    }

  /* Copy the data back to the raw contents.  */
  offset = 0;
  for (n = 0; n < num; n++)
    {
      bfd_put_32 (output_bfd, table[n].address, &contents[offset]);
      bfd_put_32 (output_bfd, table[n].size, &contents[offset + 4]);
      offset += 8;
    }

  /* Clear the removed bytes.  */
  if ((bfd_size_type) (num * 8) < section_size)
    memset (&contents[num * 8], 0, section_size - num * 8);

  if (! bfd_set_section_contents (output_bfd, sxtlit, contents, 0,
				  section_size))
    return -1;

  /* Copy the contents to ".got.loc".  */
  memcpy (sgotloc->contents, contents, section_size);

  free (contents);
  free (table);
  return num;
}


/* Finish up the dynamic sections.  */

static bfd_boolean
elf_xtensa_finish_dynamic_sections (bfd *output_bfd,
				    struct bfd_link_info *info)
{
  bfd *dynobj;
  asection *sdyn, *srelplt, *sgot, *sxtlit, *sgotloc;
  Elf32_External_Dyn *dyncon, *dynconend;
  int num_xtlit_entries;

  if (! elf_hash_table (info)->dynamic_sections_created)
    return TRUE;

  dynobj = elf_hash_table (info)->dynobj;
  sdyn = bfd_get_section_by_name (dynobj, ".dynamic");
  BFD_ASSERT (sdyn != NULL);

  /* Set the first entry in the global offset table to the address of
     the dynamic section.  */
  sgot = bfd_get_section_by_name (dynobj, ".got");
  if (sgot)
    {
      BFD_ASSERT (sgot->size == 4);
      if (sdyn == NULL)
	bfd_put_32 (output_bfd, 0, sgot->contents);
      else
	bfd_put_32 (output_bfd,
		    sdyn->output_section->vma + sdyn->output_offset,
		    sgot->contents);
    }

  srelplt = bfd_get_section_by_name (dynobj, ".rela.plt");
  if (srelplt && srelplt->size != 0)
    {
      asection *sgotplt, *srelgot, *spltlittbl;
      int chunk, plt_chunks, plt_entries;
      Elf_Internal_Rela irela;
      bfd_byte *loc;
      unsigned rtld_reloc;

      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");;
      BFD_ASSERT (srelgot != NULL);

      spltlittbl = bfd_get_section_by_name (dynobj, ".xt.lit.plt");
      BFD_ASSERT (spltlittbl != NULL);

      /* Find the first XTENSA_RTLD relocation.  Presumably the rest
	 of them follow immediately after....  */
      for (rtld_reloc = 0; rtld_reloc < srelgot->reloc_count; rtld_reloc++)
	{
	  loc = srelgot->contents + rtld_reloc * sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  if (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD)
	    break;
	}
      BFD_ASSERT (rtld_reloc < srelgot->reloc_count);

      plt_entries = srelplt->size / sizeof (Elf32_External_Rela);
      plt_chunks =
	(plt_entries + PLT_ENTRIES_PER_CHUNK - 1) / PLT_ENTRIES_PER_CHUNK;

      for (chunk = 0; chunk < plt_chunks; chunk++)
	{
	  int chunk_entries = 0;

	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  BFD_ASSERT (sgotplt != NULL);

	  /* Emit special RTLD relocations for the first two entries in
	     each chunk of the .got.plt section.  */

	  loc = srelgot->contents + rtld_reloc * sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  BFD_ASSERT (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD);
	  irela.r_offset = (sgotplt->output_section->vma
			    + sgotplt->output_offset);
	  irela.r_addend = 1; /* tell rtld to set value to resolver function */
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  rtld_reloc += 1;
	  BFD_ASSERT (rtld_reloc <= srelgot->reloc_count);

	  /* Next literal immediately follows the first.  */
	  loc += sizeof (Elf32_External_Rela);
	  bfd_elf32_swap_reloca_in (output_bfd, loc, &irela);
	  BFD_ASSERT (ELF32_R_TYPE (irela.r_info) == R_XTENSA_RTLD);
	  irela.r_offset = (sgotplt->output_section->vma
			    + sgotplt->output_offset + 4);
	  /* Tell rtld to set value to object's link map.  */
	  irela.r_addend = 2;
	  bfd_elf32_swap_reloca_out (output_bfd, &irela, loc);
	  rtld_reloc += 1;
	  BFD_ASSERT (rtld_reloc <= srelgot->reloc_count);

	  /* Fill in the literal table.  */
	  if (chunk < plt_chunks - 1)
	    chunk_entries = PLT_ENTRIES_PER_CHUNK;
	  else
	    chunk_entries = plt_entries - (chunk * PLT_ENTRIES_PER_CHUNK);

	  BFD_ASSERT ((unsigned) (chunk + 1) * 8 <= spltlittbl->size);
	  bfd_put_32 (output_bfd,
		      sgotplt->output_section->vma + sgotplt->output_offset,
		      spltlittbl->contents + (chunk * 8) + 0);
	  bfd_put_32 (output_bfd,
		      8 + (chunk_entries * 4),
		      spltlittbl->contents + (chunk * 8) + 4);
	}

      /* All the dynamic relocations have been emitted at this point.
	 Make sure the relocation sections are the correct size.  */
      if (srelgot->size != (sizeof (Elf32_External_Rela)
			    * srelgot->reloc_count)
	  || srelplt->size != (sizeof (Elf32_External_Rela)
			       * srelplt->reloc_count))
	abort ();

     /* The .xt.lit.plt section has just been modified.  This must
	happen before the code below which combines adjacent literal
	table entries, and the .xt.lit.plt contents have to be forced to
	the output here.  */
      if (! bfd_set_section_contents (output_bfd,
				      spltlittbl->output_section,
				      spltlittbl->contents,
				      spltlittbl->output_offset,
				      spltlittbl->size))
	return FALSE;
      /* Clear SEC_HAS_CONTENTS so the contents won't be output again.  */
      spltlittbl->flags &= ~SEC_HAS_CONTENTS;
    }

  /* Combine adjacent literal table entries.  */
  BFD_ASSERT (! info->relocatable);
  sxtlit = bfd_get_section_by_name (output_bfd, ".xt.lit");
  sgotloc = bfd_get_section_by_name (dynobj, ".got.loc");
  BFD_ASSERT (sxtlit && sgotloc);
  num_xtlit_entries =
    elf_xtensa_combine_prop_entries (output_bfd, sxtlit, sgotloc);
  if (num_xtlit_entries < 0)
    return FALSE;

  dyncon = (Elf32_External_Dyn *) sdyn->contents;
  dynconend = (Elf32_External_Dyn *) (sdyn->contents + sdyn->size);
  for (; dyncon < dynconend; dyncon++)
    {
      Elf_Internal_Dyn dyn;
      const char *name;
      asection *s;

      bfd_elf32_swap_dyn_in (dynobj, dyncon, &dyn);

      switch (dyn.d_tag)
	{
	default:
	  break;

	case DT_XTENSA_GOT_LOC_SZ:
	  dyn.d_un.d_val = num_xtlit_entries;
	  break;

	case DT_XTENSA_GOT_LOC_OFF:
	  name = ".got.loc";
	  goto get_vma;
	case DT_PLTGOT:
	  name = ".got";
	  goto get_vma;
	case DT_JMPREL:
	  name = ".rela.plt";
	get_vma:
	  s = bfd_get_section_by_name (output_bfd, name);
	  BFD_ASSERT (s);
	  dyn.d_un.d_ptr = s->vma;
	  break;

	case DT_PLTRELSZ:
	  s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	  BFD_ASSERT (s);
	  dyn.d_un.d_val = s->size;
	  break;

	case DT_RELASZ:
	  /* Adjust RELASZ to not include JMPREL.  This matches what
	     glibc expects and what is done for several other ELF
	     targets (e.g., i386, alpha), but the "correct" behavior
	     seems to be unresolved.  Since the linker script arranges
	     for .rela.plt to follow all other relocation sections, we
	     don't have to worry about changing the DT_RELA entry.  */
	  s = bfd_get_section_by_name (output_bfd, ".rela.plt");
	  if (s)
	    dyn.d_un.d_val -= s->size;
	  break;
	}

      bfd_elf32_swap_dyn_out (output_bfd, &dyn, dyncon);
    }

  return TRUE;
}


/* Functions for dealing with the e_flags field.  */

/* Merge backend specific data from an object file to the output
   object file when linking.  */

static bfd_boolean
elf_xtensa_merge_private_bfd_data (bfd *ibfd, bfd *obfd)
{
  unsigned out_mach, in_mach;
  flagword out_flag, in_flag;

  /* Check if we have the same endianess.  */
  if (!_bfd_generic_verify_endian_match (ibfd, obfd))
    return FALSE;

  /* Don't even pretend to support mixed-format linking.  */
  if (bfd_get_flavour (ibfd) != bfd_target_elf_flavour
      || bfd_get_flavour (obfd) != bfd_target_elf_flavour)
    return FALSE;

  out_flag = elf_elfheader (obfd)->e_flags;
  in_flag = elf_elfheader (ibfd)->e_flags;

  out_mach = out_flag & EF_XTENSA_MACH;
  in_mach = in_flag & EF_XTENSA_MACH;
  if (out_mach != in_mach)
    {
      (*_bfd_error_handler)
	(_("%B: incompatible machine type. Output is 0x%x. Input is 0x%x"),
	 ibfd, out_mach, in_mach);
      bfd_set_error (bfd_error_wrong_format);
      return FALSE;
    }

  if (! elf_flags_init (obfd))
    {
      elf_flags_init (obfd) = TRUE;
      elf_elfheader (obfd)->e_flags = in_flag;

      if (bfd_get_arch (obfd) == bfd_get_arch (ibfd)
	  && bfd_get_arch_info (obfd)->the_default)
	return bfd_set_arch_mach (obfd, bfd_get_arch (ibfd),
				  bfd_get_mach (ibfd));

      return TRUE;
    }

  if ((out_flag & EF_XTENSA_XT_INSN) != (in_flag & EF_XTENSA_XT_INSN)) 
    elf_elfheader (obfd)->e_flags &= (~ EF_XTENSA_XT_INSN);

  if ((out_flag & EF_XTENSA_XT_LIT) != (in_flag & EF_XTENSA_XT_LIT)) 
    elf_elfheader (obfd)->e_flags &= (~ EF_XTENSA_XT_LIT);

  return TRUE;
}


static bfd_boolean
elf_xtensa_set_private_flags (bfd *abfd, flagword flags)
{
  BFD_ASSERT (!elf_flags_init (abfd)
	      || elf_elfheader (abfd)->e_flags == flags);

  elf_elfheader (abfd)->e_flags |= flags;
  elf_flags_init (abfd) = TRUE;

  return TRUE;
}


static bfd_boolean
elf_xtensa_print_private_bfd_data (bfd *abfd, void *farg)
{
  FILE *f = (FILE *) farg;
  flagword e_flags = elf_elfheader (abfd)->e_flags;

  fprintf (f, "\nXtensa header:\n");
  if ((e_flags & EF_XTENSA_MACH) == E_XTENSA_MACH)
    fprintf (f, "\nMachine     = Base\n");
  else
    fprintf (f, "\nMachine Id  = 0x%x\n", e_flags & EF_XTENSA_MACH);

  fprintf (f, "Insn tables = %s\n",
	   (e_flags & EF_XTENSA_XT_INSN) ? "true" : "false");

  fprintf (f, "Literal tables = %s\n",
	   (e_flags & EF_XTENSA_XT_LIT) ? "true" : "false");

  return _bfd_elf_print_private_bfd_data (abfd, farg);
}


/* Set the right machine number for an Xtensa ELF file.  */

static bfd_boolean
elf_xtensa_object_p (bfd *abfd)
{
  int mach;
  unsigned long arch = elf_elfheader (abfd)->e_flags & EF_XTENSA_MACH;

  switch (arch)
    {
    case E_XTENSA_MACH:
      mach = bfd_mach_xtensa;
      break;
    default:
      return FALSE;
    }

  (void) bfd_default_set_arch_mach (abfd, bfd_arch_xtensa, mach);
  return TRUE;
}


/* The final processing done just before writing out an Xtensa ELF object
   file.  This gets the Xtensa architecture right based on the machine
   number.  */

static void
elf_xtensa_final_write_processing (bfd *abfd,
				   bfd_boolean linker ATTRIBUTE_UNUSED)
{
  int mach;
  unsigned long val;

  switch (mach = bfd_get_mach (abfd))
    {
    case bfd_mach_xtensa:
      val = E_XTENSA_MACH;
      break;
    default:
      return;
    }

  elf_elfheader (abfd)->e_flags &=  (~ EF_XTENSA_MACH);
  elf_elfheader (abfd)->e_flags |= val;
}


static enum elf_reloc_type_class
elf_xtensa_reloc_type_class (const Elf_Internal_Rela *rela)
{
  switch ((int) ELF32_R_TYPE (rela->r_info))
    {
    case R_XTENSA_RELATIVE:
      return reloc_class_relative;
    case R_XTENSA_JMP_SLOT:
      return reloc_class_plt;
    default:
      return reloc_class_normal;
    }
}


static bfd_boolean
elf_xtensa_discard_info_for_section (bfd *abfd,
				     struct elf_reloc_cookie *cookie,
				     struct bfd_link_info *info,
				     asection *sec)
{
  bfd_byte *contents;
  bfd_vma section_size;
  bfd_vma offset, actual_offset;
  size_t removed_bytes = 0;

  section_size = sec->size;
  if (section_size == 0 || section_size % 8 != 0)
    return FALSE;

  if (sec->output_section
      && bfd_is_abs_section (sec->output_section))
    return FALSE;

  contents = retrieve_contents (abfd, sec, info->keep_memory);
  if (!contents)
    return FALSE;

  cookie->rels = retrieve_internal_relocs (abfd, sec, info->keep_memory);
  if (!cookie->rels)
    {
      release_contents (sec, contents);
      return FALSE;
    }

  cookie->rel = cookie->rels;
  cookie->relend = cookie->rels + sec->reloc_count;

  for (offset = 0; offset < section_size; offset += 8)
    {
      actual_offset = offset - removed_bytes;

      /* The ...symbol_deleted_p function will skip over relocs but it
	 won't adjust their offsets, so do that here.  */
      while (cookie->rel < cookie->relend
	     && cookie->rel->r_offset < offset)
	{
	  cookie->rel->r_offset -= removed_bytes;
	  cookie->rel++;
	}

      while (cookie->rel < cookie->relend
	     && cookie->rel->r_offset == offset)
	{
	  if (bfd_elf_reloc_symbol_deleted_p (offset, cookie))
	    {
	      /* Remove the table entry.  (If the reloc type is NONE, then
		 the entry has already been merged with another and deleted
		 during relaxation.)  */
	      if (ELF32_R_TYPE (cookie->rel->r_info) != R_XTENSA_NONE)
		{
		  /* Shift the contents up.  */
		  if (offset + 8 < section_size)
		    memmove (&contents[actual_offset],
			     &contents[actual_offset+8],
			     section_size - offset - 8);
		  removed_bytes += 8;
		}

	      /* Remove this relocation.  */
	      cookie->rel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	    }

	  /* Adjust the relocation offset for previous removals.  This
	     should not be done before calling ...symbol_deleted_p
	     because it might mess up the offset comparisons there.
	     Make sure the offset doesn't underflow in the case where
	     the first entry is removed.  */
	  if (cookie->rel->r_offset >= removed_bytes)
	    cookie->rel->r_offset -= removed_bytes;
	  else
	    cookie->rel->r_offset = 0;

	  cookie->rel++;
	}
    }

  if (removed_bytes != 0)
    {
      /* Adjust any remaining relocs (shouldn't be any).  */
      for (; cookie->rel < cookie->relend; cookie->rel++)
	{
	  if (cookie->rel->r_offset >= removed_bytes)
	    cookie->rel->r_offset -= removed_bytes;
	  else
	    cookie->rel->r_offset = 0;
	}

      /* Clear the removed bytes.  */
      memset (&contents[section_size - removed_bytes], 0, removed_bytes);

      pin_contents (sec, contents);
      pin_internal_relocs (sec, cookie->rels);

      /* Shrink size.  */
      sec->size = section_size - removed_bytes;

      if (xtensa_is_littable_section (sec))
	{
	  bfd *dynobj = elf_hash_table (info)->dynobj;
	  if (dynobj)
	    {
	      asection *sgotloc =
		bfd_get_section_by_name (dynobj, ".got.loc");
	      if (sgotloc)
		sgotloc->size -= removed_bytes;
	    }
	}
    }
  else
    {
      release_contents (sec, contents);
      release_internal_relocs (sec, cookie->rels);
    }

  return (removed_bytes != 0);
}


static bfd_boolean
elf_xtensa_discard_info (bfd *abfd,
			 struct elf_reloc_cookie *cookie,
			 struct bfd_link_info *info)
{
  asection *sec;
  bfd_boolean changed = FALSE;

  for (sec = abfd->sections; sec != NULL; sec = sec->next)
    {
      if (xtensa_is_property_section (sec))
	{
	  if (elf_xtensa_discard_info_for_section (abfd, cookie, info, sec))
	    changed = TRUE;
	}
    }

  return changed;
}


static bfd_boolean
elf_xtensa_ignore_discarded_relocs (asection *sec)
{
  return xtensa_is_property_section (sec);
}


/* Support for core dump NOTE sections.  */

static bfd_boolean
elf_xtensa_grok_prstatus (bfd *abfd, Elf_Internal_Note *note)
{
  int offset;
  unsigned int size;

  /* The size for Xtensa is variable, so don't try to recognize the format
     based on the size.  Just assume this is GNU/Linux.  */

  /* pr_cursig */
  elf_tdata (abfd)->core_signal = bfd_get_16 (abfd, note->descdata + 12);

  /* pr_pid */
  elf_tdata (abfd)->core_pid = bfd_get_32 (abfd, note->descdata + 24);

  /* pr_reg */
  offset = 72;
  size = note->descsz - offset - 4;

  /* Make a ".reg/999" section.  */
  return _bfd_elfcore_make_pseudosection (abfd, ".reg",
					  size, note->descpos + offset);
}


static bfd_boolean
elf_xtensa_grok_psinfo (bfd *abfd, Elf_Internal_Note *note)
{
  switch (note->descsz)
    {
      default:
	return FALSE;

      case 128:		/* GNU/Linux elf_prpsinfo */
	elf_tdata (abfd)->core_program
	 = _bfd_elfcore_strndup (abfd, note->descdata + 32, 16);
	elf_tdata (abfd)->core_command
	 = _bfd_elfcore_strndup (abfd, note->descdata + 48, 80);
    }

  /* Note that for some reason, a spurious space is tacked
     onto the end of the args in some (at least one anyway)
     implementations, so strip it off if it exists.  */

  {
    char *command = elf_tdata (abfd)->core_command;
    int n = strlen (command);

    if (0 < n && command[n - 1] == ' ')
      command[n - 1] = '\0';
  }

  return TRUE;
}


/* Generic Xtensa configurability stuff.  */

static xtensa_opcode callx0_op = XTENSA_UNDEFINED;
static xtensa_opcode callx4_op = XTENSA_UNDEFINED;
static xtensa_opcode callx8_op = XTENSA_UNDEFINED;
static xtensa_opcode callx12_op = XTENSA_UNDEFINED;
static xtensa_opcode call0_op = XTENSA_UNDEFINED;
static xtensa_opcode call4_op = XTENSA_UNDEFINED;
static xtensa_opcode call8_op = XTENSA_UNDEFINED;
static xtensa_opcode call12_op = XTENSA_UNDEFINED;

static void
init_call_opcodes (void)
{
  if (callx0_op == XTENSA_UNDEFINED)
    {
      callx0_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx0");
      callx4_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx4");
      callx8_op  = xtensa_opcode_lookup (xtensa_default_isa, "callx8");
      callx12_op = xtensa_opcode_lookup (xtensa_default_isa, "callx12");
      call0_op   = xtensa_opcode_lookup (xtensa_default_isa, "call0");
      call4_op   = xtensa_opcode_lookup (xtensa_default_isa, "call4");
      call8_op   = xtensa_opcode_lookup (xtensa_default_isa, "call8");
      call12_op  = xtensa_opcode_lookup (xtensa_default_isa, "call12");
    }
}


static bfd_boolean
is_indirect_call_opcode (xtensa_opcode opcode)
{
  init_call_opcodes ();
  return (opcode == callx0_op
	  || opcode == callx4_op
	  || opcode == callx8_op
	  || opcode == callx12_op);
}


static bfd_boolean
is_direct_call_opcode (xtensa_opcode opcode)
{
  init_call_opcodes ();
  return (opcode == call0_op
	  || opcode == call4_op
	  || opcode == call8_op
	  || opcode == call12_op);
}


static bfd_boolean
is_windowed_call_opcode (xtensa_opcode opcode)
{
  init_call_opcodes ();
  return (opcode == call4_op
	  || opcode == call8_op
	  || opcode == call12_op
	  || opcode == callx4_op
	  || opcode == callx8_op
	  || opcode == callx12_op);
}


static xtensa_opcode
get_const16_opcode (void)
{
  static bfd_boolean done_lookup = FALSE;
  static xtensa_opcode const16_opcode = XTENSA_UNDEFINED;
  if (!done_lookup)
    {
      const16_opcode = xtensa_opcode_lookup (xtensa_default_isa, "const16");
      done_lookup = TRUE;
    }
  return const16_opcode;
}


static xtensa_opcode
get_l32r_opcode (void)
{
  static xtensa_opcode l32r_opcode = XTENSA_UNDEFINED;
  static bfd_boolean done_lookup = FALSE;

  if (!done_lookup)
    {
      l32r_opcode = xtensa_opcode_lookup (xtensa_default_isa, "l32r");
      done_lookup = TRUE;
    }
  return l32r_opcode;
}


static bfd_vma
l32r_offset (bfd_vma addr, bfd_vma pc)
{
  bfd_vma offset;

  offset = addr - ((pc+3) & -4);
  BFD_ASSERT ((offset & ((1 << 2) - 1)) == 0);
  offset = (signed int) offset >> 2;
  BFD_ASSERT ((signed int) offset >> 16 == -1);
  return offset;
}


static int
get_relocation_opnd (xtensa_opcode opcode, int r_type)
{
  xtensa_isa isa = xtensa_default_isa;
  int last_immed, last_opnd, opi;

  if (opcode == XTENSA_UNDEFINED)
    return XTENSA_UNDEFINED;

  /* Find the last visible PC-relative immediate operand for the opcode.
     If there are no PC-relative immediates, then choose the last visible
     immediate; otherwise, fail and return XTENSA_UNDEFINED.  */
  last_immed = XTENSA_UNDEFINED;
  last_opnd = xtensa_opcode_num_operands (isa, opcode);
  for (opi = last_opnd - 1; opi >= 0; opi--)
    {
      if (xtensa_operand_is_visible (isa, opcode, opi) == 0)
	continue;
      if (xtensa_operand_is_PCrelative (isa, opcode, opi) == 1)
	{
	  last_immed = opi;
	  break;
	}
      if (last_immed == XTENSA_UNDEFINED
	  && xtensa_operand_is_register (isa, opcode, opi) == 0)
	last_immed = opi;
    }
  if (last_immed < 0)
    return XTENSA_UNDEFINED;

  /* If the operand number was specified in an old-style relocation,
     check for consistency with the operand computed above.  */
  if (r_type >= R_XTENSA_OP0 && r_type <= R_XTENSA_OP2)
    {
      int reloc_opnd = r_type - R_XTENSA_OP0;
      if (reloc_opnd != last_immed)
	return XTENSA_UNDEFINED;
    }

  return last_immed;
}


int
get_relocation_slot (int r_type)
{
  switch (r_type)
    {
    case R_XTENSA_OP0:
    case R_XTENSA_OP1:
    case R_XTENSA_OP2:
      return 0;

    default:
      if (r_type >= R_XTENSA_SLOT0_OP && r_type <= R_XTENSA_SLOT14_OP)
	return r_type - R_XTENSA_SLOT0_OP;
      if (r_type >= R_XTENSA_SLOT0_ALT && r_type <= R_XTENSA_SLOT14_ALT)
	return r_type - R_XTENSA_SLOT0_ALT;
      break;
    }

  return XTENSA_UNDEFINED;
}


/* Get the opcode for a relocation.  */

static xtensa_opcode
get_relocation_opcode (bfd *abfd,
		       asection *sec,
		       bfd_byte *contents,
		       Elf_Internal_Rela *irel)
{
  static xtensa_insnbuf ibuff = NULL;
  static xtensa_insnbuf sbuff = NULL;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_format fmt;
  int slot;

  if (contents == NULL)
    return XTENSA_UNDEFINED;

  if (bfd_get_section_limit (abfd, sec) <= irel->r_offset)
    return XTENSA_UNDEFINED;

  if (ibuff == NULL)
    {
      ibuff = xtensa_insnbuf_alloc (isa);
      sbuff = xtensa_insnbuf_alloc (isa);
    }

  /* Decode the instruction.  */
  xtensa_insnbuf_from_chars (isa, ibuff, &contents[irel->r_offset],
			     sec->size - irel->r_offset);
  fmt = xtensa_format_decode (isa, ibuff);
  slot = get_relocation_slot (ELF32_R_TYPE (irel->r_info));
  if (slot == XTENSA_UNDEFINED)
    return XTENSA_UNDEFINED;
  xtensa_format_get_slot (isa, fmt, slot, ibuff, sbuff);
  return xtensa_opcode_decode (isa, fmt, slot, sbuff);
}


bfd_boolean
is_l32r_relocation (bfd *abfd,
		    asection *sec,
		    bfd_byte *contents,
		    Elf_Internal_Rela *irel)
{
  xtensa_opcode opcode;
  if (!is_operand_relocation (ELF32_R_TYPE (irel->r_info)))
    return FALSE;
  opcode = get_relocation_opcode (abfd, sec, contents, irel);
  return (opcode == get_l32r_opcode ());
}


static bfd_size_type
get_asm_simplify_size (bfd_byte *contents,
		       bfd_size_type content_len,
		       bfd_size_type offset)
{
  bfd_size_type insnlen, size = 0;

  /* Decode the size of the next two instructions.  */
  insnlen = insn_decode_len (contents, content_len, offset);
  if (insnlen == 0)
    return 0;

  size += insnlen;
  
  insnlen = insn_decode_len (contents, content_len, offset + size);
  if (insnlen == 0)
    return 0;

  size += insnlen;
  return size;
}


bfd_boolean
is_alt_relocation (int r_type)
{
  return (r_type >= R_XTENSA_SLOT0_ALT
	  && r_type <= R_XTENSA_SLOT14_ALT);
}


bfd_boolean
is_operand_relocation (int r_type)
{
  switch (r_type)
    {
    case R_XTENSA_OP0:
    case R_XTENSA_OP1:
    case R_XTENSA_OP2:
      return TRUE;

    default:
      if (r_type >= R_XTENSA_SLOT0_OP && r_type <= R_XTENSA_SLOT14_OP)
	return TRUE;
      if (r_type >= R_XTENSA_SLOT0_ALT && r_type <= R_XTENSA_SLOT14_ALT)
	return TRUE;
      break;
    }

  return FALSE;
}

      
#define MIN_INSN_LENGTH 2

/* Return 0 if it fails to decode.  */

bfd_size_type
insn_decode_len (bfd_byte *contents,
		 bfd_size_type content_len,
		 bfd_size_type offset)
{
  int insn_len;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_format fmt;
  static xtensa_insnbuf ibuff = NULL;

  if (offset + MIN_INSN_LENGTH > content_len)
    return 0;

  if (ibuff == NULL)
    ibuff = xtensa_insnbuf_alloc (isa);
  xtensa_insnbuf_from_chars (isa, ibuff, &contents[offset],
			     content_len - offset);
  fmt = xtensa_format_decode (isa, ibuff);
  if (fmt == XTENSA_UNDEFINED)
    return 0;
  insn_len = xtensa_format_length (isa, fmt);
  if (insn_len ==  XTENSA_UNDEFINED)
    return 0;
  return insn_len;
}


/* Decode the opcode for a single slot instruction.
   Return 0 if it fails to decode or the instruction is multi-slot.  */

xtensa_opcode
insn_decode_opcode (bfd_byte *contents,
		    bfd_size_type content_len,
		    bfd_size_type offset,
		    int slot)
{
  xtensa_isa isa = xtensa_default_isa;
  xtensa_format fmt;
  static xtensa_insnbuf insnbuf = NULL;
  static xtensa_insnbuf slotbuf = NULL;

  if (offset + MIN_INSN_LENGTH > content_len)
    return XTENSA_UNDEFINED;

  if (insnbuf == NULL)
    {
      insnbuf = xtensa_insnbuf_alloc (isa);
      slotbuf = xtensa_insnbuf_alloc (isa);
    }

  xtensa_insnbuf_from_chars (isa, insnbuf, &contents[offset],
			     content_len - offset);
  fmt = xtensa_format_decode (isa, insnbuf);
  if (fmt == XTENSA_UNDEFINED)
    return XTENSA_UNDEFINED;

  if (slot >= xtensa_format_num_slots (isa, fmt))
    return XTENSA_UNDEFINED;

  xtensa_format_get_slot (isa, fmt, slot, insnbuf, slotbuf);
  return xtensa_opcode_decode (isa, fmt, slot, slotbuf);
}


/* The offset is the offset in the contents.
   The address is the address of that offset.  */

static bfd_boolean
check_branch_target_aligned (bfd_byte *contents,
			     bfd_size_type content_length,
			     bfd_vma offset,
			     bfd_vma address)
{
  bfd_size_type insn_len = insn_decode_len (contents, content_length, offset);
  if (insn_len == 0)
    return FALSE;
  return check_branch_target_aligned_address (address, insn_len);
}


static bfd_boolean
check_loop_aligned (bfd_byte *contents,
		    bfd_size_type content_length,
		    bfd_vma offset,
		    bfd_vma address)
{
  bfd_size_type loop_len, insn_len;
  xtensa_opcode opcode =
    insn_decode_opcode (contents, content_length, offset, 0);
  BFD_ASSERT (opcode != XTENSA_UNDEFINED);
  if (opcode != XTENSA_UNDEFINED)
    return FALSE;
  BFD_ASSERT (xtensa_opcode_is_loop (xtensa_default_isa, opcode));
  if (!xtensa_opcode_is_loop (xtensa_default_isa, opcode))
    return FALSE;

  loop_len = insn_decode_len (contents, content_length, offset);
  BFD_ASSERT (loop_len != 0);
  if (loop_len == 0)
    return FALSE;

  insn_len = insn_decode_len (contents, content_length, offset + loop_len);
  BFD_ASSERT (insn_len != 0);
  if (insn_len == 0)
    return FALSE;

  return check_branch_target_aligned_address (address + loop_len, insn_len);
}


static bfd_boolean
check_branch_target_aligned_address (bfd_vma addr, int len)
{
  if (len == 8)
    return (addr % 8 == 0);
  return ((addr >> 2) == ((addr + len - 1) >> 2));
}


/* Instruction widening and narrowing.  */

/* When FLIX is available we need to access certain instructions only
   when they are 16-bit or 24-bit instructions.  This table caches
   information about such instructions by walking through all the
   opcodes and finding the smallest single-slot format into which each
   can be encoded.  */

static xtensa_format *op_single_fmt_table = NULL;


static void
init_op_single_format_table (void)
{
  xtensa_isa isa = xtensa_default_isa;
  xtensa_insnbuf ibuf;
  xtensa_opcode opcode;
  xtensa_format fmt;
  int num_opcodes;

  if (op_single_fmt_table)
    return;

  ibuf = xtensa_insnbuf_alloc (isa);
  num_opcodes = xtensa_isa_num_opcodes (isa);

  op_single_fmt_table = (xtensa_format *)
    bfd_malloc (sizeof (xtensa_format) * num_opcodes);
  for (opcode = 0; opcode < num_opcodes; opcode++)
    {
      op_single_fmt_table[opcode] = XTENSA_UNDEFINED;
      for (fmt = 0; fmt < xtensa_isa_num_formats (isa); fmt++)
	{
	  if (xtensa_format_num_slots (isa, fmt) == 1
	      && xtensa_opcode_encode (isa, fmt, 0, ibuf, opcode) == 0)
	    {
	      xtensa_opcode old_fmt = op_single_fmt_table[opcode];
	      int fmt_length = xtensa_format_length (isa, fmt);
	      if (old_fmt == XTENSA_UNDEFINED
		  || fmt_length < xtensa_format_length (isa, old_fmt))
		op_single_fmt_table[opcode] = fmt;
	    }
	}
    }
  xtensa_insnbuf_free (isa, ibuf);
}


static xtensa_format
get_single_format (xtensa_opcode opcode)
{
  init_op_single_format_table ();
  return op_single_fmt_table[opcode];
}


/* For the set of narrowable instructions we do NOT include the
   narrowings beqz -> beqz.n or bnez -> bnez.n because of complexities
   involved during linker relaxation that may require these to
   re-expand in some conditions.  Also, the narrowing "or" -> mov.n
   requires special case code to ensure it only works when op1 == op2.  */

struct string_pair
{
  const char *wide;
  const char *narrow;
};

struct string_pair narrowable[] =
{
  { "add", "add.n" },
  { "addi", "addi.n" },
  { "addmi", "addi.n" },
  { "l32i", "l32i.n" },
  { "movi", "movi.n" },
  { "ret", "ret.n" },
  { "retw", "retw.n" },
  { "s32i", "s32i.n" },
  { "or", "mov.n" } /* special case only when op1 == op2 */
};

struct string_pair widenable[] =
{
  { "add", "add.n" },
  { "addi", "addi.n" },
  { "addmi", "addi.n" },
  { "beqz", "beqz.n" },
  { "bnez", "bnez.n" },
  { "l32i", "l32i.n" },
  { "movi", "movi.n" },
  { "ret", "ret.n" },
  { "retw", "retw.n" },
  { "s32i", "s32i.n" },
  { "or", "mov.n" } /* special case only when op1 == op2 */
};


/* Attempt to narrow an instruction.  Return true if the narrowing is
   valid.  If the do_it parameter is non-zero, then perform the action
   in-place directly into the contents.  Otherwise, do not modify the
   contents.  The set of valid narrowing are specified by a string table
   but require some special case operand checks in some cases.  */

static bfd_boolean
narrow_instruction (bfd_byte *contents,
		    bfd_size_type content_length,
		    bfd_size_type offset,
		    bfd_boolean do_it)
{
  xtensa_opcode opcode;
  bfd_size_type insn_len, opi;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_format fmt, o_fmt;

  static xtensa_insnbuf insnbuf = NULL;
  static xtensa_insnbuf slotbuf = NULL;
  static xtensa_insnbuf o_insnbuf = NULL;
  static xtensa_insnbuf o_slotbuf = NULL;

  if (insnbuf == NULL)
    {
      insnbuf = xtensa_insnbuf_alloc (isa);
      slotbuf = xtensa_insnbuf_alloc (isa);
      o_insnbuf = xtensa_insnbuf_alloc (isa);
      o_slotbuf = xtensa_insnbuf_alloc (isa);
    }

  BFD_ASSERT (offset < content_length);

  if (content_length < 2)
    return FALSE;

  /* We will hand-code a few of these for a little while.
     These have all been specified in the assembler aleady.  */
  xtensa_insnbuf_from_chars (isa, insnbuf, &contents[offset],
			     content_length - offset);
  fmt = xtensa_format_decode (isa, insnbuf);
  if (xtensa_format_num_slots (isa, fmt) != 1)
    return FALSE;

  if (xtensa_format_get_slot (isa, fmt, 0, insnbuf, slotbuf) != 0)
    return FALSE;

  opcode = xtensa_opcode_decode (isa, fmt, 0, slotbuf);
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;
  insn_len = xtensa_format_length (isa, fmt);
  if (insn_len > content_length)
    return FALSE;

  for (opi = 0; opi < (sizeof (narrowable)/sizeof (struct string_pair)); ++opi)
    {
      bfd_boolean is_or = (strcmp ("or", narrowable[opi].wide) == 0);

      if (opcode == xtensa_opcode_lookup (isa, narrowable[opi].wide))
	{
	  uint32 value, newval;
	  int i, operand_count, o_operand_count;
	  xtensa_opcode o_opcode;

	  /* Address does not matter in this case.  We might need to
	     fix it to handle branches/jumps.  */
	  bfd_vma self_address = 0;

	  o_opcode = xtensa_opcode_lookup (isa, narrowable[opi].narrow);
	  if (o_opcode == XTENSA_UNDEFINED)
	    return FALSE;
	  o_fmt = get_single_format (o_opcode);
	  if (o_fmt == XTENSA_UNDEFINED)
	    return FALSE;

	  if (xtensa_format_length (isa, fmt) != 3
	      || xtensa_format_length (isa, o_fmt) != 2)
	    return FALSE;

	  xtensa_format_encode (isa, o_fmt, o_insnbuf);
	  operand_count = xtensa_opcode_num_operands (isa, opcode);
	  o_operand_count = xtensa_opcode_num_operands (isa, o_opcode);

	  if (xtensa_opcode_encode (isa, o_fmt, 0, o_slotbuf, o_opcode) != 0)
	    return FALSE;

	  if (!is_or)
	    {
	      if (xtensa_opcode_num_operands (isa, o_opcode) != operand_count)
		return FALSE;
	    }
	  else
	    {
	      uint32 rawval0, rawval1, rawval2;

	      if (o_operand_count + 1 != operand_count)
		return FALSE;
	      if (xtensa_operand_get_field (isa, opcode, 0,
					    fmt, 0, slotbuf, &rawval0) != 0)
		return FALSE;
	      if (xtensa_operand_get_field (isa, opcode, 1,
					    fmt, 0, slotbuf, &rawval1) != 0)
		return FALSE;
	      if (xtensa_operand_get_field (isa, opcode, 2,
					    fmt, 0, slotbuf, &rawval2) != 0)
		return FALSE;

	      if (rawval1 != rawval2)
		return FALSE;
	      if (rawval0 == rawval1) /* it is a nop */
		return FALSE;
	    }

	  for (i = 0; i < o_operand_count; ++i)
	    {
	      if (xtensa_operand_get_field (isa, opcode, i, fmt, 0,
					    slotbuf, &value)
		  || xtensa_operand_decode (isa, opcode, i, &value))
		return FALSE;

	      /* PC-relative branches need adjustment, but
		 the PC-rel operand will always have a relocation.  */
	      newval = value;
	      if (xtensa_operand_do_reloc (isa, o_opcode, i, &newval,
					   self_address)
		  || xtensa_operand_encode (isa, o_opcode, i, &newval)
		  || xtensa_operand_set_field (isa, o_opcode, i, o_fmt, 0,
					       o_slotbuf, newval))
		return FALSE;
	    }

	  if (xtensa_format_set_slot (isa, o_fmt, 0,
				      o_insnbuf, o_slotbuf) != 0)
	    return FALSE;

	  if (do_it)
	    xtensa_insnbuf_to_chars (isa, o_insnbuf, contents + offset,
				     content_length - offset);
	  return TRUE;
	}
    }
  return FALSE;
}


/* Attempt to widen an instruction.  Return true if the widening is
   valid.  If the do_it parameter is non-zero, then the action should
   be performed inplace into the contents.  Otherwise, do not modify
   the contents.  The set of valid widenings are specified by a string
   table but require some special case operand checks in some
   cases.  */

static bfd_boolean
widen_instruction (bfd_byte *contents,
		   bfd_size_type content_length,
		   bfd_size_type offset,
		   bfd_boolean do_it)
{
  xtensa_opcode opcode;
  bfd_size_type insn_len, opi;
  xtensa_isa isa = xtensa_default_isa;
  xtensa_format fmt, o_fmt;

  static xtensa_insnbuf insnbuf = NULL;
  static xtensa_insnbuf slotbuf = NULL;
  static xtensa_insnbuf o_insnbuf = NULL;
  static xtensa_insnbuf o_slotbuf = NULL;

  if (insnbuf == NULL)
    {
      insnbuf = xtensa_insnbuf_alloc (isa);
      slotbuf = xtensa_insnbuf_alloc (isa);
      o_insnbuf = xtensa_insnbuf_alloc (isa);
      o_slotbuf = xtensa_insnbuf_alloc (isa);
    }

  BFD_ASSERT (offset < content_length);

  if (content_length < 2)
    return FALSE;

  /* We will hand code a few of these for a little while.
     These have all been specified in the assembler aleady.  */
  xtensa_insnbuf_from_chars (isa, insnbuf, &contents[offset],
			     content_length - offset);
  fmt = xtensa_format_decode (isa, insnbuf);
  if (xtensa_format_num_slots (isa, fmt) != 1)
    return FALSE;

  if (xtensa_format_get_slot (isa, fmt, 0, insnbuf, slotbuf) != 0)
    return FALSE;

  opcode = xtensa_opcode_decode (isa, fmt, 0, slotbuf);
  if (opcode == XTENSA_UNDEFINED)
    return FALSE;
  insn_len = xtensa_format_length (isa, fmt);
  if (insn_len > content_length)
    return FALSE;

  for (opi = 0; opi < (sizeof (widenable)/sizeof (struct string_pair)); ++opi)
    {
      bfd_boolean is_or = (strcmp ("or", widenable[opi].wide) == 0);
      bfd_boolean is_branch = (strcmp ("beqz", widenable[opi].wide) == 0
			       || strcmp ("bnez", widenable[opi].wide) == 0);

      if (opcode == xtensa_opcode_lookup (isa, widenable[opi].narrow))
	{
	  uint32 value, newval;
	  int i, operand_count, o_operand_count, check_operand_count;
	  xtensa_opcode o_opcode;

	  /* Address does not matter in this case.  We might need to fix it
	     to handle branches/jumps.  */
	  bfd_vma self_address = 0;

	  o_opcode = xtensa_opcode_lookup (isa, widenable[opi].wide);
	  if (o_opcode == XTENSA_UNDEFINED)
	    return FALSE;
	  o_fmt = get_single_format (o_opcode);
	  if (o_fmt == XTENSA_UNDEFINED)
	    return FALSE;

	  if (xtensa_format_length (isa, fmt) != 2
	      || xtensa_format_length (isa, o_fmt) != 3)
	    return FALSE;

	  xtensa_format_encode (isa, o_fmt, o_insnbuf);
	  operand_count = xtensa_opcode_num_operands (isa, opcode);
	  o_operand_count = xtensa_opcode_num_operands (isa, o_opcode);
	  check_operand_count = o_operand_count;

	  if (xtensa_opcode_encode (isa, o_fmt, 0, o_slotbuf, o_opcode) != 0)
	    return FALSE;

	  if (!is_or)
	    {
	      if (xtensa_opcode_num_operands (isa, o_opcode) != operand_count)
		return FALSE;
	    }
	  else
	    {
	      uint32 rawval0, rawval1;

	      if (o_operand_count != operand_count + 1)
		return FALSE;
	      if (xtensa_operand_get_field (isa, opcode, 0,
					    fmt, 0, slotbuf, &rawval0) != 0)
		return FALSE;
	      if (xtensa_operand_get_field (isa, opcode, 1,
					    fmt, 0, slotbuf, &rawval1) != 0)
		return FALSE;
	      if (rawval0 == rawval1) /* it is a nop */
		return FALSE;
	    }
	  if (is_branch)
	    check_operand_count--;

	  for (i = 0; i < check_operand_count; ++i)
	    {
	      int new_i = i;
	      if (is_or && i == o_operand_count - 1)
		new_i = i - 1;
	      if (xtensa_operand_get_field (isa, opcode, new_i, fmt, 0,
					    slotbuf, &value)
		  || xtensa_operand_decode (isa, opcode, new_i, &value))
		return FALSE;

	      /* PC-relative branches need adjustment, but
		 the PC-rel operand will always have a relocation.  */
	      newval = value;
	      if (xtensa_operand_do_reloc (isa, o_opcode, i, &newval,
					   self_address)
		  || xtensa_operand_encode (isa, o_opcode, i, &newval)
		  || xtensa_operand_set_field (isa, o_opcode, i, o_fmt, 0,
					       o_slotbuf, newval))
		return FALSE;
	    }

	  if (xtensa_format_set_slot (isa, o_fmt, 0, o_insnbuf, o_slotbuf))
	    return FALSE;

	  if (do_it)
	    xtensa_insnbuf_to_chars (isa, o_insnbuf, contents + offset,
				     content_length - offset);
	  return TRUE;
	}
    }
  return FALSE;
}


/* Code for transforming CALLs at link-time.  */

static bfd_reloc_status_type
elf_xtensa_do_asm_simplify (bfd_byte *contents,
			    bfd_vma address,
			    bfd_vma content_length,
			    char **error_message)
{
  static xtensa_insnbuf insnbuf = NULL;
  static xtensa_insnbuf slotbuf = NULL;
  xtensa_format core_format = XTENSA_UNDEFINED;
  xtensa_opcode opcode;
  xtensa_opcode direct_call_opcode;
  xtensa_isa isa = xtensa_default_isa;
  bfd_byte *chbuf = contents + address;
  int opn;

  if (insnbuf == NULL)
    {
      insnbuf = xtensa_insnbuf_alloc (isa);
      slotbuf = xtensa_insnbuf_alloc (isa);
    }

  if (content_length < address)
    {
      *error_message = _("Attempt to convert L32R/CALLX to CALL failed");
      return bfd_reloc_other;
    }

  opcode = get_expanded_call_opcode (chbuf, content_length - address, 0);
  direct_call_opcode = swap_callx_for_call_opcode (opcode);
  if (direct_call_opcode == XTENSA_UNDEFINED)
    {
      *error_message = _("Attempt to convert L32R/CALLX to CALL failed");
      return bfd_reloc_other;
    }
  
  /* Assemble a NOP ("or a1, a1, a1") into the 0 byte offset.  */
  core_format = xtensa_format_lookup (isa, "x24");
  opcode = xtensa_opcode_lookup (isa, "or");
  xtensa_opcode_encode (isa, core_format, 0, slotbuf, opcode);
  for (opn = 0; opn < 3; opn++) 
    {
      uint32 regno = 1;
      xtensa_operand_encode (isa, opcode, opn, &regno);
      xtensa_operand_set_field (isa, opcode, opn, core_format, 0,
				slotbuf, regno);
    }
  xtensa_format_encode (isa, core_format, insnbuf);
  xtensa_format_set_slot (isa, core_format, 0, insnbuf, slotbuf);
  xtensa_insnbuf_to_chars (isa, insnbuf, chbuf, content_length - address);

  /* Assemble a CALL ("callN 0") into the 3 byte offset.  */
  xtensa_opcode_encode (isa, core_format, 0, slotbuf, direct_call_opcode);
  xtensa_operand_set_field (isa, opcode, 0, core_format, 0, slotbuf, 0);

  xtensa_format_encode (isa, core_format, insnbuf);
  xtensa_format_set_slot (isa, core_format, 0, insnbuf, slotbuf);
  xtensa_insnbuf_to_chars (isa, insnbuf, chbuf + 3,
			   content_length - address - 3);

  return bfd_reloc_ok;
}


static bfd_reloc_status_type
contract_asm_expansion (bfd_byte *contents,
			bfd_vma content_length,
			Elf_Internal_Rela *irel,
			char **error_message)
{
  bfd_reloc_status_type retval =
    elf_xtensa_do_asm_simplify (contents, irel->r_offset, content_length,
				error_message);

  if (retval != bfd_reloc_ok)
    return bfd_reloc_dangerous;

  /* Update the irel->r_offset field so that the right immediate and
     the right instruction are modified during the relocation.  */
  irel->r_offset += 3;
  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info), R_XTENSA_SLOT0_OP);
  return bfd_reloc_ok;
}


static xtensa_opcode
swap_callx_for_call_opcode (xtensa_opcode opcode)
{
  init_call_opcodes ();

  if (opcode == callx0_op) return call0_op;
  if (opcode == callx4_op) return call4_op;
  if (opcode == callx8_op) return call8_op;
  if (opcode == callx12_op) return call12_op;

  /* Return XTENSA_UNDEFINED if the opcode is not an indirect call.  */
  return XTENSA_UNDEFINED;
}


/* Check if "buf" is pointing to a "L32R aN; CALLX aN" or "CONST16 aN;
   CONST16 aN; CALLX aN" sequence, and if so, return the CALLX opcode.
   If not, return XTENSA_UNDEFINED.  */

#define L32R_TARGET_REG_OPERAND 0
#define CONST16_TARGET_REG_OPERAND 0
#define CALLN_SOURCE_OPERAND 0

static xtensa_opcode 
get_expanded_call_opcode (bfd_byte *buf, int bufsize, bfd_boolean *p_uses_l32r)
{
  static xtensa_insnbuf insnbuf = NULL;
  static xtensa_insnbuf slotbuf = NULL;
  xtensa_format fmt;
  xtensa_opcode opcode;
  xtensa_isa isa = xtensa_default_isa;
  uint32 regno, const16_regno, call_regno;
  int offset = 0;

  if (insnbuf == NULL)
    {
      insnbuf = xtensa_insnbuf_alloc (isa);
      slotbuf = xtensa_insnbuf_alloc (isa);
    }

  xtensa_insnbuf_from_chars (isa, insnbuf, buf, bufsize);
  fmt = xtensa_format_decode (isa, insnbuf);
  if (fmt == XTENSA_UNDEFINED
      || xtensa_format_get_slot (isa, fmt, 0, insnbuf, slotbuf))
    return XTENSA_UNDEFINED;

  opcode = xtensa_opcode_decode (isa, fmt, 0, slotbuf);
  if (opcode == XTENSA_UNDEFINED)
    return XTENSA_UNDEFINED;

  if (opcode == get_l32r_opcode ())
    {
      if (p_uses_l32r)
	*p_uses_l32r = TRUE;
      if (xtensa_operand_get_field (isa, opcode, L32R_TARGET_REG_OPERAND,
				    fmt, 0, slotbuf, &regno)
	  || xtensa_operand_decode (isa, opcode, L32R_TARGET_REG_OPERAND,
				    &regno))
	return XTENSA_UNDEFINED;
    }
  else if (opcode == get_const16_opcode ())
    {
      if (p_uses_l32r)
	*p_uses_l32r = FALSE;
      if (xtensa_operand_get_field (isa, opcode, CONST16_TARGET_REG_OPERAND,
				    fmt, 0, slotbuf, &regno)
	  || xtensa_operand_decode (isa, opcode, CONST16_TARGET_REG_OPERAND,
				    &regno))
	return XTENSA_UNDEFINED;

      /* Check that the next instruction is also CONST16.  */
      offset += xtensa_format_length (isa, fmt);
      xtensa_insnbuf_from_chars (isa, insnbuf, buf + offset, bufsize - offset);
      fmt = xtensa_format_decode (isa, insnbuf);
      if (fmt == XTENSA_UNDEFINED
	  || xtensa_format_get_slot (isa, fmt, 0, insnbuf, slotbuf))
	return XTENSA_UNDEFINED;
      opcode = xtensa_opcode_decode (isa, fmt, 0, slotbuf);
      if (opcode != get_const16_opcode ())
	return XTENSA_UNDEFINED;

      if (xtensa_operand_get_field (isa, opcode, CONST16_TARGET_REG_OPERAND,
				    fmt, 0, slotbuf, &const16_regno)
	  || xtensa_operand_decode (isa, opcode, CONST16_TARGET_REG_OPERAND,
				    &const16_regno)
	  || const16_regno != regno)
	return XTENSA_UNDEFINED;
    }
  else
    return XTENSA_UNDEFINED;

  /* Next instruction should be an CALLXn with operand 0 == regno.  */
  offset += xtensa_format_length (isa, fmt);
  xtensa_insnbuf_from_chars (isa, insnbuf, buf + offset, bufsize - offset);
  fmt = xtensa_format_decode (isa, insnbuf);
  if (fmt == XTENSA_UNDEFINED
      || xtensa_format_get_slot (isa, fmt, 0, insnbuf, slotbuf))
    return XTENSA_UNDEFINED;
  opcode = xtensa_opcode_decode (isa, fmt, 0, slotbuf);
  if (opcode == XTENSA_UNDEFINED 
      || !is_indirect_call_opcode (opcode))
    return XTENSA_UNDEFINED;

  if (xtensa_operand_get_field (isa, opcode, CALLN_SOURCE_OPERAND,
				fmt, 0, slotbuf, &call_regno)
      || xtensa_operand_decode (isa, opcode, CALLN_SOURCE_OPERAND,
				&call_regno))
    return XTENSA_UNDEFINED;

  if (call_regno != regno)
    return XTENSA_UNDEFINED;

  return opcode;
}


/* Data structures used during relaxation.  */

/* r_reloc: relocation values.  */

/* Through the relaxation process, we need to keep track of the values
   that will result from evaluating relocations.  The standard ELF
   relocation structure is not sufficient for this purpose because we're
   operating on multiple input files at once, so we need to know which
   input file a relocation refers to.  The r_reloc structure thus
   records both the input file (bfd) and ELF relocation.

   For efficiency, an r_reloc also contains a "target_offset" field to
   cache the target-section-relative offset value that is represented by
   the relocation.
   
   The r_reloc also contains a virtual offset that allows multiple
   inserted literals to be placed at the same "address" with
   different offsets.  */

typedef struct r_reloc_struct r_reloc;

struct r_reloc_struct
{
  bfd *abfd;
  Elf_Internal_Rela rela;
  bfd_vma target_offset;
  bfd_vma virtual_offset;
};


/* The r_reloc structure is included by value in literal_value, but not
   every literal_value has an associated relocation -- some are simple
   constants.  In such cases, we set all the fields in the r_reloc
   struct to zero.  The r_reloc_is_const function should be used to
   detect this case.  */

static bfd_boolean
r_reloc_is_const (const r_reloc *r_rel)
{
  return (r_rel->abfd == NULL);
}


static bfd_vma
r_reloc_get_target_offset (const r_reloc *r_rel)
{
  bfd_vma target_offset;
  unsigned long r_symndx;

  BFD_ASSERT (!r_reloc_is_const (r_rel));
  r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  target_offset = get_elf_r_symndx_offset (r_rel->abfd, r_symndx);
  return (target_offset + r_rel->rela.r_addend);
}


static struct elf_link_hash_entry *
r_reloc_get_hash_entry (const r_reloc *r_rel)
{
  unsigned long r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  return get_elf_r_symndx_hash_entry (r_rel->abfd, r_symndx);
}


static asection *
r_reloc_get_section (const r_reloc *r_rel)
{
  unsigned long r_symndx = ELF32_R_SYM (r_rel->rela.r_info);
  return get_elf_r_symndx_section (r_rel->abfd, r_symndx);
}


static bfd_boolean
r_reloc_is_defined (const r_reloc *r_rel)
{
  asection *sec;
  if (r_rel == NULL)
    return FALSE;

  sec = r_reloc_get_section (r_rel);
  if (sec == bfd_abs_section_ptr
      || sec == bfd_com_section_ptr
      || sec == bfd_und_section_ptr)
    return FALSE;
  return TRUE;
}


static void
r_reloc_init (r_reloc *r_rel,
	      bfd *abfd,
	      Elf_Internal_Rela *irel,
	      bfd_byte *contents,
	      bfd_size_type content_length)
{
  int r_type;
  reloc_howto_type *howto;

  if (irel)
    {
      r_rel->rela = *irel;
      r_rel->abfd = abfd;
      r_rel->target_offset = r_reloc_get_target_offset (r_rel);
      r_rel->virtual_offset = 0;
      r_type = ELF32_R_TYPE (r_rel->rela.r_info);
      howto = &elf_howto_table[r_type];
      if (howto->partial_inplace)
	{
	  bfd_vma inplace_val;
	  BFD_ASSERT (r_rel->rela.r_offset < content_length);

	  inplace_val = bfd_get_32 (abfd, &contents[r_rel->rela.r_offset]);
	  r_rel->target_offset += inplace_val;
	}
    }
  else
    memset (r_rel, 0, sizeof (r_reloc));
}


#if DEBUG

static void
print_r_reloc (FILE *fp, const r_reloc *r_rel)
{
  if (r_reloc_is_defined (r_rel))
    {
      asection *sec = r_reloc_get_section (r_rel);
      fprintf (fp, " %s(%s + ", sec->owner->filename, sec->name);
    }
  else if (r_reloc_get_hash_entry (r_rel))
    fprintf (fp, " %s + ", r_reloc_get_hash_entry (r_rel)->root.root.string);
  else
    fprintf (fp, " ?? + ");

  fprintf_vma (fp, r_rel->target_offset);
  if (r_rel->virtual_offset)
    {
      fprintf (fp, " + ");
      fprintf_vma (fp, r_rel->virtual_offset);
    }
    
  fprintf (fp, ")");
}

#endif /* DEBUG */


/* source_reloc: relocations that reference literals.  */

/* To determine whether literals can be coalesced, we need to first
   record all the relocations that reference the literals.  The
   source_reloc structure below is used for this purpose.  The
   source_reloc entries are kept in a per-literal-section array, sorted
   by offset within the literal section (i.e., target offset).

   The source_sec and r_rel.rela.r_offset fields identify the source of
   the relocation.  The r_rel field records the relocation value, i.e.,
   the offset of the literal being referenced.  The opnd field is needed
   to determine the range of the immediate field to which the relocation
   applies, so we can determine whether another literal with the same
   value is within range.  The is_null field is true when the relocation
   is being removed (e.g., when an L32R is being removed due to a CALLX
   that is converted to a direct CALL).  */

typedef struct source_reloc_struct source_reloc;

struct source_reloc_struct
{
  asection *source_sec;
  r_reloc r_rel;
  xtensa_opcode opcode;
  int opnd;
  bfd_boolean is_null;
  bfd_boolean is_abs_literal;
};


static void
init_source_reloc (source_reloc *reloc,
		   asection *source_sec,
		   const r_reloc *r_rel,
		   xtensa_opcode opcode,
		   int opnd,
		   bfd_boolean is_abs_literal)
{
  reloc->source_sec = source_sec;
  reloc->r_rel = *r_rel;
  reloc->opcode = opcode;
  reloc->opnd = opnd;
  reloc->is_null = FALSE;
  reloc->is_abs_literal = is_abs_literal;
}


/* Find the source_reloc for a particular source offset and relocation
   type.  Note that the array is sorted by _target_ offset, so this is
   just a linear search.  */

static source_reloc *
find_source_reloc (source_reloc *src_relocs,
		   int src_count,
		   asection *sec,
		   Elf_Internal_Rela *irel)
{
  int i;

  for (i = 0; i < src_count; i++)
    {
      if (src_relocs[i].source_sec == sec
	  && src_relocs[i].r_rel.rela.r_offset == irel->r_offset
	  && (ELF32_R_TYPE (src_relocs[i].r_rel.rela.r_info)
	      == ELF32_R_TYPE (irel->r_info)))
	return &src_relocs[i];
    }

  return NULL;
}


static int
source_reloc_compare (const void *ap, const void *bp)
{
  const source_reloc *a = (const source_reloc *) ap;
  const source_reloc *b = (const source_reloc *) bp;

  if (a->r_rel.target_offset != b->r_rel.target_offset)
    return (a->r_rel.target_offset - b->r_rel.target_offset);

  /* We don't need to sort on these criteria for correctness,
     but enforcing a more strict ordering prevents unstable qsort
     from behaving differently with different implementations.
     Without the code below we get correct but different results
     on Solaris 2.7 and 2.8.  We would like to always produce the
     same results no matter the host. */

  if ((!a->is_null) - (!b->is_null))
    return ((!a->is_null) - (!b->is_null));
  return internal_reloc_compare (&a->r_rel.rela, &b->r_rel.rela);
}


/* Literal values and value hash tables.  */

/* Literals with the same value can be coalesced.  The literal_value
   structure records the value of a literal: the "r_rel" field holds the
   information from the relocation on the literal (if there is one) and
   the "value" field holds the contents of the literal word itself.

   The value_map structure records a literal value along with the
   location of a literal holding that value.  The value_map hash table
   is indexed by the literal value, so that we can quickly check if a
   particular literal value has been seen before and is thus a candidate
   for coalescing.  */

typedef struct literal_value_struct literal_value;
typedef struct value_map_struct value_map;
typedef struct value_map_hash_table_struct value_map_hash_table;

struct literal_value_struct
{
  r_reloc r_rel; 
  unsigned long value;
  bfd_boolean is_abs_literal;
};

struct value_map_struct
{
  literal_value val;			/* The literal value.  */
  r_reloc loc;				/* Location of the literal.  */
  value_map *next;
};

struct value_map_hash_table_struct
{
  unsigned bucket_count;
  value_map **buckets;
  unsigned count;
  bfd_boolean has_last_loc;
  r_reloc last_loc;
};


static void
init_literal_value (literal_value *lit,
		    const r_reloc *r_rel,
		    unsigned long value,
		    bfd_boolean is_abs_literal)
{
  lit->r_rel = *r_rel;
  lit->value = value;
  lit->is_abs_literal = is_abs_literal;
}


static bfd_boolean
literal_value_equal (const literal_value *src1,
		     const literal_value *src2,
		     bfd_boolean final_static_link)
{
  struct elf_link_hash_entry *h1, *h2;

  if (r_reloc_is_const (&src1->r_rel) != r_reloc_is_const (&src2->r_rel)) 
    return FALSE;

  if (r_reloc_is_const (&src1->r_rel))
    return (src1->value == src2->value);

  if (ELF32_R_TYPE (src1->r_rel.rela.r_info)
      != ELF32_R_TYPE (src2->r_rel.rela.r_info))
    return FALSE;

  if (src1->r_rel.target_offset != src2->r_rel.target_offset)
    return FALSE;
   
  if (src1->r_rel.virtual_offset != src2->r_rel.virtual_offset)
    return FALSE;

  if (src1->value != src2->value)
    return FALSE;
  
  /* Now check for the same section (if defined) or the same elf_hash
     (if undefined or weak).  */
  h1 = r_reloc_get_hash_entry (&src1->r_rel);
  h2 = r_reloc_get_hash_entry (&src2->r_rel);
  if (r_reloc_is_defined (&src1->r_rel)
      && (final_static_link
	  || ((!h1 || h1->root.type != bfd_link_hash_defweak)
	      && (!h2 || h2->root.type != bfd_link_hash_defweak))))
    {
      if (r_reloc_get_section (&src1->r_rel)
	  != r_reloc_get_section (&src2->r_rel))
	return FALSE;
    }
  else
    {
      /* Require that the hash entries (i.e., symbols) be identical.  */
      if (h1 != h2 || h1 == 0)
	return FALSE;
    }

  if (src1->is_abs_literal != src2->is_abs_literal)
    return FALSE;

  return TRUE;
}


/* Must be power of 2.  */
#define INITIAL_HASH_RELOC_BUCKET_COUNT 1024

static value_map_hash_table *
value_map_hash_table_init (void)
{
  value_map_hash_table *values;

  values = (value_map_hash_table *)
    bfd_zmalloc (sizeof (value_map_hash_table));
  values->bucket_count = INITIAL_HASH_RELOC_BUCKET_COUNT;
  values->count = 0;
  values->buckets = (value_map **)
    bfd_zmalloc (sizeof (value_map *) * values->bucket_count);
  if (values->buckets == NULL) 
    {
      free (values);
      return NULL;
    }
  values->has_last_loc = FALSE;

  return values;
}


static void
value_map_hash_table_delete (value_map_hash_table *table)
{
  free (table->buckets);
  free (table);
}


static unsigned
hash_bfd_vma (bfd_vma val)
{
  return (val >> 2) + (val >> 10);
}


static unsigned
literal_value_hash (const literal_value *src)
{
  unsigned hash_val;

  hash_val = hash_bfd_vma (src->value);
  if (!r_reloc_is_const (&src->r_rel))
    {
      void *sec_or_hash;

      hash_val += hash_bfd_vma (src->is_abs_literal * 1000);
      hash_val += hash_bfd_vma (src->r_rel.target_offset);
      hash_val += hash_bfd_vma (src->r_rel.virtual_offset);
  
      /* Now check for the same section and the same elf_hash.  */
      if (r_reloc_is_defined (&src->r_rel))
	sec_or_hash = r_reloc_get_section (&src->r_rel);
      else
	sec_or_hash = r_reloc_get_hash_entry (&src->r_rel);
      hash_val += hash_bfd_vma ((bfd_vma) (size_t) sec_or_hash);
    }
  return hash_val;
}


/* Check if the specified literal_value has been seen before.  */

static value_map *
value_map_get_cached_value (value_map_hash_table *map,
			    const literal_value *val,
			    bfd_boolean final_static_link)
{
  value_map *map_e;
  value_map *bucket;
  unsigned idx;

  idx = literal_value_hash (val);
  idx = idx & (map->bucket_count - 1);
  bucket = map->buckets[idx];
  for (map_e = bucket; map_e; map_e = map_e->next)
    {
      if (literal_value_equal (&map_e->val, val, final_static_link))
	return map_e;
    }
  return NULL;
}


/* Record a new literal value.  It is illegal to call this if VALUE
   already has an entry here.  */

static value_map *
add_value_map (value_map_hash_table *map,
	       const literal_value *val,
	       const r_reloc *loc,
	       bfd_boolean final_static_link)
{
  value_map **bucket_p;
  unsigned idx;

  value_map *val_e = (value_map *) bfd_zmalloc (sizeof (value_map));
  if (val_e == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  BFD_ASSERT (!value_map_get_cached_value (map, val, final_static_link));
  val_e->val = *val;
  val_e->loc = *loc;

  idx = literal_value_hash (val);
  idx = idx & (map->bucket_count - 1);
  bucket_p = &map->buckets[idx];

  val_e->next = *bucket_p;
  *bucket_p = val_e;
  map->count++;
  /* FIXME: Consider resizing the hash table if we get too many entries.  */
  
  return val_e;
}


/* Lists of text actions (ta_) for narrowing, widening, longcall
   conversion, space fill, code & literal removal, etc.  */

/* The following text actions are generated:

   "ta_remove_insn"         remove an instruction or instructions
   "ta_remove_longcall"     convert longcall to call
   "ta_convert_longcall"    convert longcall to nop/call
   "ta_narrow_insn"         narrow a wide instruction
   "ta_widen"               widen a narrow instruction
   "ta_fill"                add fill or remove fill
      removed < 0 is a fill; branches to the fill address will be
	changed to address + fill size (e.g., address - removed)
      removed >= 0 branches to the fill address will stay unchanged
   "ta_remove_literal"      remove a literal; this action is
			    indicated when a literal is removed
                            or replaced.
   "ta_add_literal"         insert a new literal; this action is
                            indicated when a literal has been moved.
                            It may use a virtual_offset because
			    multiple literals can be placed at the
                            same location.

   For each of these text actions, we also record the number of bytes
   removed by performing the text action.  In the case of a "ta_widen"
   or a "ta_fill" that adds space, the removed_bytes will be negative.  */

typedef struct text_action_struct text_action;
typedef struct text_action_list_struct text_action_list;
typedef enum text_action_enum_t text_action_t;

enum text_action_enum_t
{
  ta_none,
  ta_remove_insn,        /* removed = -size */
  ta_remove_longcall,    /* removed = -size */
  ta_convert_longcall,   /* removed = 0 */
  ta_narrow_insn,        /* removed = -1 */
  ta_widen_insn,         /* removed = +1 */
  ta_fill,               /* removed = +size */
  ta_remove_literal,
  ta_add_literal
};


/* Structure for a text action record.  */
struct text_action_struct
{
  text_action_t action;
  asection *sec;	/* Optional */
  bfd_vma offset;
  bfd_vma virtual_offset;  /* Zero except for adding literals.  */
  int removed_bytes;
  literal_value value;	/* Only valid when adding literals.  */

  text_action *next;
};


/* List of all of the actions taken on a text section.  */
struct text_action_list_struct
{
  text_action *head;
};


static text_action *
find_fill_action (text_action_list *l, asection *sec, bfd_vma offset)
{
  text_action **m_p;

  /* It is not necessary to fill at the end of a section.  */
  if (sec->size == offset)
    return NULL;

  for (m_p = &l->head; *m_p && (*m_p)->offset <= offset; m_p = &(*m_p)->next)
    {
      text_action *t = *m_p;
      /* When the action is another fill at the same address,
	 just increase the size.  */
      if (t->offset == offset && t->action == ta_fill)
	return t;
    }
  return NULL;
}


static int
compute_removed_action_diff (const text_action *ta,
			     asection *sec,
			     bfd_vma offset,
			     int removed,
			     int removable_space)
{
  int new_removed;
  int current_removed = 0;

  if (ta)
    current_removed = ta->removed_bytes;

  BFD_ASSERT (ta == NULL || ta->offset == offset);
  BFD_ASSERT (ta == NULL || ta->action == ta_fill);

  /* It is not necessary to fill at the end of a section.  Clean this up.  */
  if (sec->size == offset)
    new_removed = removable_space - 0;
  else
    {
      int space;
      int added = -removed - current_removed;
      /* Ignore multiples of the section alignment.  */
      added = ((1 << sec->alignment_power) - 1) & added;
      new_removed = (-added);

      /* Modify for removable.  */
      space = removable_space - new_removed;
      new_removed = (removable_space
		     - (((1 << sec->alignment_power) - 1) & space));
    }
  return (new_removed - current_removed);
}


static void
adjust_fill_action (text_action *ta, int fill_diff)
{
  ta->removed_bytes += fill_diff;
}


/* Add a modification action to the text.  For the case of adding or
   removing space, modify any current fill and assume that
   "unreachable_space" bytes can be freely contracted.  Note that a
   negative removed value is a fill.  */

static void 
text_action_add (text_action_list *l,
		 text_action_t action,
		 asection *sec,
		 bfd_vma offset,
		 int removed)
{
  text_action **m_p;
  text_action *ta;

  /* It is not necessary to fill at the end of a section.  */
  if (action == ta_fill && sec->size == offset)
    return;

  /* It is not necessary to fill 0 bytes.  */
  if (action == ta_fill && removed == 0)
    return;

  for (m_p = &l->head; *m_p && (*m_p)->offset <= offset; m_p = &(*m_p)->next)
    {
      text_action *t = *m_p;
      /* When the action is another fill at the same address,
	 just increase the size.  */
      if (t->offset == offset && t->action == ta_fill && action == ta_fill)
	{
	  t->removed_bytes += removed;
	  return;
	}
    }

  /* Create a new record and fill it up.  */
  ta = (text_action *) bfd_zmalloc (sizeof (text_action));
  ta->action = action;
  ta->sec = sec;
  ta->offset = offset;
  ta->removed_bytes = removed;
  ta->next = (*m_p);
  *m_p = ta;
}


static void
text_action_add_literal (text_action_list *l,
			 text_action_t action,
			 const r_reloc *loc,
			 const literal_value *value,
			 int removed)
{
  text_action **m_p;
  text_action *ta;
  asection *sec = r_reloc_get_section (loc);
  bfd_vma offset = loc->target_offset;
  bfd_vma virtual_offset = loc->virtual_offset;

  BFD_ASSERT (action == ta_add_literal);

  for (m_p = &l->head; *m_p != NULL; m_p = &(*m_p)->next)
    {
      if ((*m_p)->offset > offset
	  && ((*m_p)->offset != offset
	      || (*m_p)->virtual_offset > virtual_offset))
	break;
    }

  /* Create a new record and fill it up.  */
  ta = (text_action *) bfd_zmalloc (sizeof (text_action));
  ta->action = action;
  ta->sec = sec;
  ta->offset = offset;
  ta->virtual_offset = virtual_offset;
  ta->value = *value;
  ta->removed_bytes = removed;
  ta->next = (*m_p);
  *m_p = ta;
}


static bfd_vma 
offset_with_removed_text (text_action_list *action_list, bfd_vma offset)
{
  text_action *r;
  int removed = 0;

  for (r = action_list->head; r && r->offset <= offset; r = r->next)
    {
      if (r->offset < offset
	  || (r->action == ta_fill && r->removed_bytes < 0))
	removed += r->removed_bytes;
    }

  return (offset - removed);
}


static unsigned
action_list_count (text_action_list *action_list)
{
  text_action *r = action_list->head;
  unsigned count = 0;
  for (r = action_list->head; r != NULL; r = r->next)
    {
      count++;
    }
  return count;
}


static bfd_vma
offset_with_removed_text_before_fill (text_action_list *action_list,
				      bfd_vma offset)
{
  text_action *r;
  int removed = 0;

  for (r = action_list->head; r && r->offset < offset; r = r->next)
    removed += r->removed_bytes;

  return (offset - removed);
}


/* The find_insn_action routine will only find non-fill actions.  */

static text_action *
find_insn_action (text_action_list *action_list, bfd_vma offset)
{
  text_action *t;
  for (t = action_list->head; t; t = t->next)
    {
      if (t->offset == offset)
	{
	  switch (t->action)
	    {
	    case ta_none:
	    case ta_fill:
	      break;
	    case ta_remove_insn:
	    case ta_remove_longcall:
	    case ta_convert_longcall:
	    case ta_narrow_insn:
	    case ta_widen_insn:
	      return t;
	    case ta_remove_literal:
	    case ta_add_literal:
	      BFD_ASSERT (0);
	      break;
	    }
	}
    }
  return NULL;
}


#if DEBUG

static void
print_action_list (FILE *fp, text_action_list *action_list)
{
  text_action *r;

  fprintf (fp, "Text Action\n");
  for (r = action_list->head; r != NULL; r = r->next)
    {
      const char *t = "unknown";
      switch (r->action)
	{
	case ta_remove_insn:
	  t = "remove_insn"; break;
	case ta_remove_longcall:
	  t = "remove_longcall"; break;
	case ta_convert_longcall:
	  t = "remove_longcall"; break;
	case ta_narrow_insn:
	  t = "narrow_insn"; break;
	case ta_widen_insn:
	  t = "widen_insn"; break;
	case ta_fill:
	  t = "fill"; break;
	case ta_none:
	  t = "none"; break;
	case ta_remove_literal:
	  t = "remove_literal"; break;
	case ta_add_literal:
	  t = "add_literal"; break;
	}

      fprintf (fp, "%s: %s[0x%lx] \"%s\" %d\n",
	       r->sec->owner->filename,
	       r->sec->name, r->offset, t, r->removed_bytes);
    }
}

#endif /* DEBUG */


/* Lists of literals being coalesced or removed.  */

/* In the usual case, the literal identified by "from" is being
   coalesced with another literal identified by "to".  If the literal is
   unused and is being removed altogether, "to.abfd" will be NULL.
   The removed_literal entries are kept on a per-section list, sorted
   by the "from" offset field.  */

typedef struct removed_literal_struct removed_literal;
typedef struct removed_literal_list_struct removed_literal_list;

struct removed_literal_struct
{
  r_reloc from;
  r_reloc to;
  removed_literal *next;
};

struct removed_literal_list_struct
{
  removed_literal *head;
  removed_literal *tail;
};


/* Record that the literal at "from" is being removed.  If "to" is not
   NULL, the "from" literal is being coalesced with the "to" literal.  */

static void
add_removed_literal (removed_literal_list *removed_list,
		     const r_reloc *from,
		     const r_reloc *to)
{
  removed_literal *r, *new_r, *next_r;

  new_r = (removed_literal *) bfd_zmalloc (sizeof (removed_literal));

  new_r->from = *from;
  if (to)
    new_r->to = *to;
  else
    new_r->to.abfd = NULL;
  new_r->next = NULL;
  
  r = removed_list->head;
  if (r == NULL) 
    {
      removed_list->head = new_r;
      removed_list->tail = new_r;
    }
  /* Special check for common case of append.  */
  else if (removed_list->tail->from.target_offset < from->target_offset)
    {
      removed_list->tail->next = new_r;
      removed_list->tail = new_r;
    }
  else
    {
      while (r->from.target_offset < from->target_offset && r->next) 
	{
	  r = r->next;
	}
      next_r = r->next;
      r->next = new_r;
      new_r->next = next_r;
      if (next_r == NULL)
	removed_list->tail = new_r;
    }
}


/* Check if the list of removed literals contains an entry for the
   given address.  Return the entry if found.  */

static removed_literal *
find_removed_literal (removed_literal_list *removed_list, bfd_vma addr)
{
  removed_literal *r = removed_list->head;
  while (r && r->from.target_offset < addr)
    r = r->next;
  if (r && r->from.target_offset == addr)
    return r;
  return NULL;
}


#if DEBUG

static void
print_removed_literals (FILE *fp, removed_literal_list *removed_list)
{
  removed_literal *r;
  r = removed_list->head;
  if (r)
    fprintf (fp, "Removed Literals\n");
  for (; r != NULL; r = r->next)
    {
      print_r_reloc (fp, &r->from);
      fprintf (fp, " => ");
      if (r->to.abfd == NULL)
	fprintf (fp, "REMOVED");
      else
	print_r_reloc (fp, &r->to);
      fprintf (fp, "\n");
    }
}

#endif /* DEBUG */


/* Per-section data for relaxation.  */

typedef struct reloc_bfd_fix_struct reloc_bfd_fix;

struct xtensa_relax_info_struct
{
  bfd_boolean is_relaxable_literal_section;
  bfd_boolean is_relaxable_asm_section;
  int visited;				/* Number of times visited.  */

  source_reloc *src_relocs;		/* Array[src_count].  */
  int src_count;
  int src_next;				/* Next src_relocs entry to assign.  */

  removed_literal_list removed_list;
  text_action_list action_list;

  reloc_bfd_fix *fix_list;
  reloc_bfd_fix *fix_array;
  unsigned fix_array_count;

  /* Support for expanding the reloc array that is stored
     in the section structure.  If the relocations have been
     reallocated, the newly allocated relocations will be referenced
     here along with the actual size allocated.  The relocation
     count will always be found in the section structure.  */
  Elf_Internal_Rela *allocated_relocs; 
  unsigned relocs_count;
  unsigned allocated_relocs_count;
};

struct elf_xtensa_section_data
{
  struct bfd_elf_section_data elf;
  xtensa_relax_info relax_info;
};


static bfd_boolean
elf_xtensa_new_section_hook (bfd *abfd, asection *sec)
{
  struct elf_xtensa_section_data *sdata;
  bfd_size_type amt = sizeof (*sdata);

  sdata = (struct elf_xtensa_section_data *) bfd_zalloc (abfd, amt);
  if (sdata == NULL)
    return FALSE;
  sec->used_by_bfd = (void *) sdata;

  return _bfd_elf_new_section_hook (abfd, sec);
}


static xtensa_relax_info *
get_xtensa_relax_info (asection *sec)
{
  struct elf_xtensa_section_data *section_data;

  /* No info available if no section or if it is an output section.  */
  if (!sec || sec == sec->output_section)
    return NULL;

  section_data = (struct elf_xtensa_section_data *) elf_section_data (sec);
  return &section_data->relax_info;
}


static void
init_xtensa_relax_info (asection *sec)
{
  xtensa_relax_info *relax_info = get_xtensa_relax_info (sec);

  relax_info->is_relaxable_literal_section = FALSE;
  relax_info->is_relaxable_asm_section = FALSE;
  relax_info->visited = 0;

  relax_info->src_relocs = NULL;
  relax_info->src_count = 0;
  relax_info->src_next = 0;

  relax_info->removed_list.head = NULL;
  relax_info->removed_list.tail = NULL;

  relax_info->action_list.head = NULL;

  relax_info->fix_list = NULL;
  relax_info->fix_array = NULL;
  relax_info->fix_array_count = 0;

  relax_info->allocated_relocs = NULL; 
  relax_info->relocs_count = 0;
  relax_info->allocated_relocs_count = 0;
}


/* Coalescing literals may require a relocation to refer to a section in
   a different input file, but the standard relocation information
   cannot express that.  Instead, the reloc_bfd_fix structures are used
   to "fix" the relocations that refer to sections in other input files.
   These structures are kept on per-section lists.  The "src_type" field
   records the relocation type in case there are multiple relocations on
   the same location.  FIXME: This is ugly; an alternative might be to
   add new symbols with the "owner" field to some other input file.  */

struct reloc_bfd_fix_struct
{
  asection *src_sec;
  bfd_vma src_offset;
  unsigned src_type;			/* Relocation type.  */
  
  bfd *target_abfd;
  asection *target_sec;
  bfd_vma target_offset;
  bfd_boolean translated;
  
  reloc_bfd_fix *next;
};


static reloc_bfd_fix *
reloc_bfd_fix_init (asection *src_sec,
		    bfd_vma src_offset,
		    unsigned src_type,
		    bfd *target_abfd,
		    asection *target_sec,
		    bfd_vma target_offset,
		    bfd_boolean translated)
{
  reloc_bfd_fix *fix;

  fix = (reloc_bfd_fix *) bfd_malloc (sizeof (reloc_bfd_fix));
  fix->src_sec = src_sec;
  fix->src_offset = src_offset;
  fix->src_type = src_type;
  fix->target_abfd = target_abfd;
  fix->target_sec = target_sec;
  fix->target_offset = target_offset;
  fix->translated = translated;

  return fix;
}


static void
add_fix (asection *src_sec, reloc_bfd_fix *fix)
{
  xtensa_relax_info *relax_info;

  relax_info = get_xtensa_relax_info (src_sec);
  fix->next = relax_info->fix_list;
  relax_info->fix_list = fix;
}


static int
fix_compare (const void *ap, const void *bp)
{
  const reloc_bfd_fix *a = (const reloc_bfd_fix *) ap;
  const reloc_bfd_fix *b = (const reloc_bfd_fix *) bp;

  if (a->src_offset != b->src_offset)
    return (a->src_offset - b->src_offset);
  return (a->src_type - b->src_type);
}


static void
cache_fix_array (asection *sec)
{
  unsigned i, count = 0;
  reloc_bfd_fix *r;
  xtensa_relax_info *relax_info = get_xtensa_relax_info (sec);

  if (relax_info == NULL)
    return;
  if (relax_info->fix_list == NULL)
    return;

  for (r = relax_info->fix_list; r != NULL; r = r->next)
    count++;

  relax_info->fix_array =
    (reloc_bfd_fix *) bfd_malloc (sizeof (reloc_bfd_fix) * count);
  relax_info->fix_array_count = count;

  r = relax_info->fix_list;
  for (i = 0; i < count; i++, r = r->next)
    {
      relax_info->fix_array[count - 1 - i] = *r;
      relax_info->fix_array[count - 1 - i].next = NULL;
    }

  qsort (relax_info->fix_array, relax_info->fix_array_count,
	 sizeof (reloc_bfd_fix), fix_compare);
}


static reloc_bfd_fix *
get_bfd_fix (asection *sec, bfd_vma offset, unsigned type)
{
  xtensa_relax_info *relax_info = get_xtensa_relax_info (sec);
  reloc_bfd_fix *rv;
  reloc_bfd_fix key;

  if (relax_info == NULL)
    return NULL;
  if (relax_info->fix_list == NULL)
    return NULL;

  if (relax_info->fix_array == NULL)
    cache_fix_array (sec);

  key.src_offset = offset;
  key.src_type = type;
  rv = bsearch (&key, relax_info->fix_array,  relax_info->fix_array_count,
		sizeof (reloc_bfd_fix), fix_compare);
  return rv;
}


/* Section caching.  */

typedef struct section_cache_struct section_cache_t;

struct section_cache_struct
{
  asection *sec;

  bfd_byte *contents;		/* Cache of the section contents.  */
  bfd_size_type content_length;

  property_table_entry *ptbl;	/* Cache of the section property table.  */
  unsigned pte_count;

  Elf_Internal_Rela *relocs;	/* Cache of the section relocations.  */
  unsigned reloc_count;
};


static void
init_section_cache (section_cache_t *sec_cache)
{
  memset (sec_cache, 0, sizeof (*sec_cache));
}


static void
clear_section_cache (section_cache_t *sec_cache)
{
  if (sec_cache->sec)
    {
      release_contents (sec_cache->sec, sec_cache->contents);
      release_internal_relocs (sec_cache->sec, sec_cache->relocs);
      if (sec_cache->ptbl)
	free (sec_cache->ptbl);
      memset (sec_cache, 0, sizeof (sec_cache));
    }
}


static bfd_boolean
section_cache_section (section_cache_t *sec_cache,
		       asection *sec,
		       struct bfd_link_info *link_info)
{
  bfd *abfd;
  property_table_entry *prop_table = NULL;
  int ptblsize = 0;
  bfd_byte *contents = NULL;
  Elf_Internal_Rela *internal_relocs = NULL;
  bfd_size_type sec_size;

  if (sec == NULL)
    return FALSE;
  if (sec == sec_cache->sec)
    return TRUE;

  abfd = sec->owner;
  sec_size = bfd_get_section_limit (abfd, sec);

  /* Get the contents.  */
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    goto err;

  /* Get the relocations.  */
  internal_relocs = retrieve_internal_relocs (abfd, sec,
					      link_info->keep_memory);

  /* Get the entry table.  */
  ptblsize = xtensa_read_table_entries (abfd, sec, &prop_table,
					XTENSA_PROP_SEC_NAME, FALSE);
  if (ptblsize < 0)
    goto err;

  /* Fill in the new section cache.  */
  clear_section_cache (sec_cache);
  memset (sec_cache, 0, sizeof (sec_cache));

  sec_cache->sec = sec;
  sec_cache->contents = contents;
  sec_cache->content_length = sec_size;
  sec_cache->relocs = internal_relocs;
  sec_cache->reloc_count = sec->reloc_count;
  sec_cache->pte_count = ptblsize;
  sec_cache->ptbl = prop_table;

  return TRUE;

 err:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  if (prop_table)
    free (prop_table);
  return FALSE;
}


/* Extended basic blocks.  */

/* An ebb_struct represents an Extended Basic Block.  Within this
   range, we guarantee that all instructions are decodable, the
   property table entries are contiguous, and no property table
   specifies a segment that cannot have instructions moved.  This
   structure contains caches of the contents, property table and
   relocations for the specified section for easy use.  The range is
   specified by ranges of indices for the byte offset, property table
   offsets and relocation offsets.  These must be consistent.  */

typedef struct ebb_struct ebb_t;

struct ebb_struct
{
  asection *sec;

  bfd_byte *contents;		/* Cache of the section contents.  */
  bfd_size_type content_length;

  property_table_entry *ptbl;	/* Cache of the section property table.  */
  unsigned pte_count;

  Elf_Internal_Rela *relocs;	/* Cache of the section relocations.  */
  unsigned reloc_count;

  bfd_vma start_offset;		/* Offset in section.  */
  unsigned start_ptbl_idx;	/* Offset in the property table.  */
  unsigned start_reloc_idx;	/* Offset in the relocations.  */

  bfd_vma end_offset;
  unsigned end_ptbl_idx;
  unsigned end_reloc_idx;

  bfd_boolean ends_section;	/* Is this the last ebb in a section?  */

  /* The unreachable property table at the end of this set of blocks;
     NULL if the end is not an unreachable block.  */
  property_table_entry *ends_unreachable;
};


enum ebb_target_enum
{
  EBB_NO_ALIGN = 0,
  EBB_DESIRE_TGT_ALIGN,
  EBB_REQUIRE_TGT_ALIGN,
  EBB_REQUIRE_LOOP_ALIGN,
  EBB_REQUIRE_ALIGN
};


/* proposed_action_struct is similar to the text_action_struct except
   that is represents a potential transformation, not one that will
   occur.  We build a list of these for an extended basic block
   and use them to compute the actual actions desired.  We must be
   careful that the entire set of actual actions we perform do not
   break any relocations that would fit if the actions were not
   performed.  */

typedef struct proposed_action_struct proposed_action;

struct proposed_action_struct
{
  enum ebb_target_enum align_type; /* for the target alignment */
  bfd_vma alignment_pow;
  text_action_t action;
  bfd_vma offset;
  int removed_bytes;
  bfd_boolean do_action; /* If false, then we will not perform the action.  */
};


/* The ebb_constraint_struct keeps a set of proposed actions for an
   extended basic block.   */

typedef struct ebb_constraint_struct ebb_constraint;

struct ebb_constraint_struct
{
  ebb_t ebb;
  bfd_boolean start_movable;

  /* Bytes of extra space at the beginning if movable.  */
  int start_extra_space;

  enum ebb_target_enum start_align;

  bfd_boolean end_movable;

  /* Bytes of extra space at the end if movable.  */
  int end_extra_space;

  unsigned action_count;
  unsigned action_allocated;

  /* Array of proposed actions.  */
  proposed_action *actions;

  /* Action alignments -- one for each proposed action.  */
  enum ebb_target_enum *action_aligns;
};


static void
init_ebb_constraint (ebb_constraint *c)
{
  memset (c, 0, sizeof (ebb_constraint));
}


static void
free_ebb_constraint (ebb_constraint *c)
{
  if (c->actions)
    free (c->actions);
}


static void
init_ebb (ebb_t *ebb,
	  asection *sec,
	  bfd_byte *contents,
	  bfd_size_type content_length,
	  property_table_entry *prop_table,
	  unsigned ptblsize,
	  Elf_Internal_Rela *internal_relocs,
	  unsigned reloc_count)
{
  memset (ebb, 0, sizeof (ebb_t));
  ebb->sec = sec;
  ebb->contents = contents;
  ebb->content_length = content_length;
  ebb->ptbl = prop_table;
  ebb->pte_count = ptblsize;
  ebb->relocs = internal_relocs;
  ebb->reloc_count = reloc_count;
  ebb->start_offset = 0;
  ebb->end_offset = ebb->content_length - 1;
  ebb->start_ptbl_idx = 0;
  ebb->end_ptbl_idx = ptblsize;
  ebb->start_reloc_idx = 0;
  ebb->end_reloc_idx = reloc_count;
}


/* Extend the ebb to all decodable contiguous sections.  The algorithm
   for building a basic block around an instruction is to push it
   forward until we hit the end of a section, an unreachable block or
   a block that cannot be transformed.  Then we push it backwards
   searching for similar conditions.  */

static bfd_boolean extend_ebb_bounds_forward (ebb_t *);
static bfd_boolean extend_ebb_bounds_backward (ebb_t *);
static bfd_size_type insn_block_decodable_len
  (bfd_byte *, bfd_size_type, bfd_vma, bfd_size_type);

static bfd_boolean
extend_ebb_bounds (ebb_t *ebb)
{
  if (!extend_ebb_bounds_forward (ebb))
    return FALSE;
  if (!extend_ebb_bounds_backward (ebb))
    return FALSE;
  return TRUE;
}


static bfd_boolean
extend_ebb_bounds_forward (ebb_t *ebb)
{
  property_table_entry *the_entry, *new_entry;

  the_entry = &ebb->ptbl[ebb->end_ptbl_idx];

  /* Stop when (1) we cannot decode an instruction, (2) we are at
     the end of the property tables, (3) we hit a non-contiguous property
     table entry, (4) we hit a NO_TRANSFORM region.  */

  while (1)
    {
      bfd_vma entry_end;
      bfd_size_type insn_block_len;

      entry_end = the_entry->address - ebb->sec->vma + the_entry->size;
      insn_block_len =
	insn_block_decodable_len (ebb->contents, ebb->content_length,
				  ebb->end_offset,
				  entry_end - ebb->end_offset);
      if (insn_block_len != (entry_end - ebb->end_offset))
	{
	  (*_bfd_error_handler)
	    (_("%B(%A+0x%lx): could not decode instruction; possible configuration mismatch"),
	     ebb->sec->owner, ebb->sec, ebb->end_offset + insn_block_len);
	  return FALSE;
	}
      ebb->end_offset += insn_block_len;

      if (ebb->end_offset == ebb->sec->size)
	ebb->ends_section = TRUE;

      /* Update the reloc counter.  */
      while (ebb->end_reloc_idx + 1 < ebb->reloc_count
	     && (ebb->relocs[ebb->end_reloc_idx + 1].r_offset
		 < ebb->end_offset))
	{
	  ebb->end_reloc_idx++;
	}

      if (ebb->end_ptbl_idx + 1 == ebb->pte_count)
	return TRUE;

      new_entry = &ebb->ptbl[ebb->end_ptbl_idx + 1];
      if (((new_entry->flags & XTENSA_PROP_INSN) == 0)
	  || ((new_entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM) != 0)
	  || ((the_entry->flags & XTENSA_PROP_ALIGN) != 0))
	break;

      if (the_entry->address + the_entry->size != new_entry->address)
	break;

      the_entry = new_entry;
      ebb->end_ptbl_idx++;
    }

  /* Quick check for an unreachable or end of file just at the end.  */
  if (ebb->end_ptbl_idx + 1 == ebb->pte_count)
    {
      if (ebb->end_offset == ebb->content_length)
	ebb->ends_section = TRUE;
    }
  else
    {
      new_entry = &ebb->ptbl[ebb->end_ptbl_idx + 1];
      if ((new_entry->flags & XTENSA_PROP_UNREACHABLE) != 0
	  && the_entry->address + the_entry->size == new_entry->address)
	ebb->ends_unreachable = new_entry;
    }

  /* Any other ending requires exact alignment.  */
  return TRUE;
}


static bfd_boolean
extend_ebb_bounds_backward (ebb_t *ebb)
{
  property_table_entry *the_entry, *new_entry;

  the_entry = &ebb->ptbl[ebb->start_ptbl_idx];

  /* Stop when (1) we cannot decode the instructions in the current entry.
     (2) we are at the beginning of the property tables, (3) we hit a
     non-contiguous property table entry, (4) we hit a NO_TRANSFORM region.  */

  while (1)
    {
      bfd_vma block_begin;
      bfd_size_type insn_block_len;

      block_begin = the_entry->address - ebb->sec->vma;
      insn_block_len =
	insn_block_decodable_len (ebb->contents, ebb->content_length,
				  block_begin,
				  ebb->start_offset - block_begin);
      if (insn_block_len != ebb->start_offset - block_begin)
	{
	  (*_bfd_error_handler)
	    (_("%B(%A+0x%lx): could not decode instruction; possible configuration mismatch"),
	     ebb->sec->owner, ebb->sec, ebb->end_offset + insn_block_len);
	  return FALSE;
	}
      ebb->start_offset -= insn_block_len;

      /* Update the reloc counter.  */
      while (ebb->start_reloc_idx > 0
	     && (ebb->relocs[ebb->start_reloc_idx - 1].r_offset
		 >= ebb->start_offset))
	{
	  ebb->start_reloc_idx--;
	}

      if (ebb->start_ptbl_idx == 0)
	return TRUE;

      new_entry = &ebb->ptbl[ebb->start_ptbl_idx - 1];
      if ((new_entry->flags & XTENSA_PROP_INSN) == 0
	  || ((new_entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM) != 0)
	  || ((new_entry->flags & XTENSA_PROP_ALIGN) != 0))
	return TRUE;
      if (new_entry->address + new_entry->size != the_entry->address)
	return TRUE;

      the_entry = new_entry;
      ebb->start_ptbl_idx--;
    }
  return TRUE;
}


static bfd_size_type
insn_block_decodable_len (bfd_byte *contents,
			  bfd_size_type content_len,
			  bfd_vma block_offset,
			  bfd_size_type block_len)
{
  bfd_vma offset = block_offset;

  while (offset < block_offset + block_len)
    {
      bfd_size_type insn_len = 0;

      insn_len = insn_decode_len (contents, content_len, offset);
      if (insn_len == 0)
	return (offset - block_offset);
      offset += insn_len;
    }
  return (offset - block_offset);
}


static void
ebb_propose_action (ebb_constraint *c,
		    enum ebb_target_enum align_type,
		    bfd_vma alignment_pow,
		    text_action_t action,
		    bfd_vma offset,
		    int removed_bytes,
		    bfd_boolean do_action)
{
  proposed_action *act;

  if (c->action_allocated <= c->action_count)
    {
      unsigned new_allocated, i;
      proposed_action *new_actions;

      new_allocated = (c->action_count + 2) * 2;
      new_actions = (proposed_action *)
	bfd_zmalloc (sizeof (proposed_action) * new_allocated);

      for (i = 0; i < c->action_count; i++)
	new_actions[i] = c->actions[i];
      if (c->actions)
	free (c->actions);
      c->actions = new_actions;
      c->action_allocated = new_allocated;
    }

  act = &c->actions[c->action_count];
  act->align_type = align_type;
  act->alignment_pow = alignment_pow;
  act->action = action;
  act->offset = offset;
  act->removed_bytes = removed_bytes;
  act->do_action = do_action;

  c->action_count++;
}


/* Access to internal relocations, section contents and symbols.  */

/* During relaxation, we need to modify relocations, section contents,
   and symbol definitions, and we need to keep the original values from
   being reloaded from the input files, i.e., we need to "pin" the
   modified values in memory.  We also want to continue to observe the
   setting of the "keep-memory" flag.  The following functions wrap the
   standard BFD functions to take care of this for us.  */

static Elf_Internal_Rela *
retrieve_internal_relocs (bfd *abfd, asection *sec, bfd_boolean keep_memory)
{
  Elf_Internal_Rela *internal_relocs;

  if ((sec->flags & SEC_LINKER_CREATED) != 0)
    return NULL;

  internal_relocs = elf_section_data (sec)->relocs;
  if (internal_relocs == NULL)
    internal_relocs = (_bfd_elf_link_read_relocs
		       (abfd, sec, NULL, NULL, keep_memory));
  return internal_relocs;
}


static void
pin_internal_relocs (asection *sec, Elf_Internal_Rela *internal_relocs)
{
  elf_section_data (sec)->relocs = internal_relocs;
}


static void
release_internal_relocs (asection *sec, Elf_Internal_Rela *internal_relocs)
{
  if (internal_relocs
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
}


static bfd_byte *
retrieve_contents (bfd *abfd, asection *sec, bfd_boolean keep_memory)
{
  bfd_byte *contents;
  bfd_size_type sec_size;

  sec_size = bfd_get_section_limit (abfd, sec);
  contents = elf_section_data (sec)->this_hdr.contents;
  
  if (contents == NULL && sec_size != 0)
    {
      if (!bfd_malloc_and_get_section (abfd, sec, &contents))
	{
	  if (contents)
	    free (contents);
	  return NULL;
	}
      if (keep_memory) 
	elf_section_data (sec)->this_hdr.contents = contents;
    }
  return contents;
}


static void
pin_contents (asection *sec, bfd_byte *contents)
{
  elf_section_data (sec)->this_hdr.contents = contents;
}


static void
release_contents (asection *sec, bfd_byte *contents)
{
  if (contents && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
}


static Elf_Internal_Sym *
retrieve_local_syms (bfd *input_bfd)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf;
  size_t locsymcount;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  locsymcount = symtab_hdr->sh_info;

  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
  if (isymbuf == NULL && locsymcount != 0)
    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr, locsymcount, 0,
				    NULL, NULL, NULL);

  /* Save the symbols for this input file so they won't be read again.  */
  if (isymbuf && isymbuf != (Elf_Internal_Sym *) symtab_hdr->contents)
    symtab_hdr->contents = (unsigned char *) isymbuf;

  return isymbuf;
}


/* Code for link-time relaxation.  */

/* Initialization for relaxation: */
static bfd_boolean analyze_relocations (struct bfd_link_info *);
static bfd_boolean find_relaxable_sections
  (bfd *, asection *, struct bfd_link_info *, bfd_boolean *);
static bfd_boolean collect_source_relocs
  (bfd *, asection *, struct bfd_link_info *);
static bfd_boolean is_resolvable_asm_expansion
  (bfd *, asection *, bfd_byte *, Elf_Internal_Rela *, struct bfd_link_info *,
   bfd_boolean *);
static Elf_Internal_Rela *find_associated_l32r_irel
  (bfd *, asection *, bfd_byte *, Elf_Internal_Rela *, Elf_Internal_Rela *);
static bfd_boolean compute_text_actions
  (bfd *, asection *, struct bfd_link_info *);
static bfd_boolean compute_ebb_proposed_actions (ebb_constraint *);
static bfd_boolean compute_ebb_actions (ebb_constraint *);
static bfd_boolean check_section_ebb_pcrels_fit
  (bfd *, asection *, bfd_byte *, Elf_Internal_Rela *, const ebb_constraint *,
   const xtensa_opcode *);
static bfd_boolean check_section_ebb_reduces (const ebb_constraint *);
static void text_action_add_proposed
  (text_action_list *, const ebb_constraint *, asection *);
static int compute_fill_extra_space (property_table_entry *);

/* First pass: */
static bfd_boolean compute_removed_literals
  (bfd *, asection *, struct bfd_link_info *, value_map_hash_table *);
static Elf_Internal_Rela *get_irel_at_offset
  (asection *, Elf_Internal_Rela *, bfd_vma);
static bfd_boolean is_removable_literal 
  (const source_reloc *, int, const source_reloc *, int);
static bfd_boolean remove_dead_literal
  (bfd *, asection *, struct bfd_link_info *, Elf_Internal_Rela *,
   Elf_Internal_Rela *, source_reloc *, property_table_entry *, int); 
static bfd_boolean identify_literal_placement
  (bfd *, asection *, bfd_byte *, struct bfd_link_info *,
   value_map_hash_table *, bfd_boolean *, Elf_Internal_Rela *, int,
   source_reloc *, property_table_entry *, int, section_cache_t *,
   bfd_boolean);
static bfd_boolean relocations_reach (source_reloc *, int, const r_reloc *);
static bfd_boolean coalesce_shared_literal
  (asection *, source_reloc *, property_table_entry *, int, value_map *);
static bfd_boolean move_shared_literal
  (asection *, struct bfd_link_info *, source_reloc *, property_table_entry *,
   int, const r_reloc *, const literal_value *, section_cache_t *);

/* Second pass: */
static bfd_boolean relax_section (bfd *, asection *, struct bfd_link_info *);
static bfd_boolean translate_section_fixes (asection *);
static bfd_boolean translate_reloc_bfd_fix (reloc_bfd_fix *);
static void translate_reloc (const r_reloc *, r_reloc *);
static void shrink_dynamic_reloc_sections
  (struct bfd_link_info *, bfd *, asection *, Elf_Internal_Rela *);
static bfd_boolean move_literal
  (bfd *, struct bfd_link_info *, asection *, bfd_vma, bfd_byte *,
   xtensa_relax_info *, Elf_Internal_Rela **, const literal_value *);
static bfd_boolean relax_property_section
  (bfd *, asection *, struct bfd_link_info *);

/* Third pass: */
static bfd_boolean relax_section_symbols (bfd *, asection *);


static bfd_boolean 
elf_xtensa_relax_section (bfd *abfd,
			  asection *sec,
			  struct bfd_link_info *link_info,
			  bfd_boolean *again)
{
  static value_map_hash_table *values = NULL;
  static bfd_boolean relocations_analyzed = FALSE;
  xtensa_relax_info *relax_info;

  if (!relocations_analyzed)
    {
      /* Do some overall initialization for relaxation.  */
      values = value_map_hash_table_init ();
      if (values == NULL)
	return FALSE;
      relaxing_section = TRUE;
      if (!analyze_relocations (link_info))
	return FALSE;
      relocations_analyzed = TRUE;
    }
  *again = FALSE;

  /* Don't mess with linker-created sections.  */
  if ((sec->flags & SEC_LINKER_CREATED) != 0)
    return TRUE;

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info != NULL);

  switch (relax_info->visited)
    {
    case 0:
      /* Note: It would be nice to fold this pass into
	 analyze_relocations, but it is important for this step that the
	 sections be examined in link order.  */
      if (!compute_removed_literals (abfd, sec, link_info, values))
	return FALSE;
      *again = TRUE;
      break;

    case 1:
      if (values)
	value_map_hash_table_delete (values);
      values = NULL;
      if (!relax_section (abfd, sec, link_info))
	return FALSE;
      *again = TRUE;
      break;

    case 2:
      if (!relax_section_symbols (abfd, sec))
	return FALSE;
      break;
    }

  relax_info->visited++;
  return TRUE;
}


/* Initialization for relaxation.  */

/* This function is called once at the start of relaxation.  It scans
   all the input sections and marks the ones that are relaxable (i.e.,
   literal sections with L32R relocations against them), and then
   collects source_reloc information for all the relocations against
   those relaxable sections.  During this process, it also detects
   longcalls, i.e., calls relaxed by the assembler into indirect
   calls, that can be optimized back into direct calls.  Within each
   extended basic block (ebb) containing an optimized longcall, it
   computes a set of "text actions" that can be performed to remove
   the L32R associated with the longcall while optionally preserving
   branch target alignments.  */

static bfd_boolean
analyze_relocations (struct bfd_link_info *link_info)
{
  bfd *abfd;
  asection *sec;
  bfd_boolean is_relaxable = FALSE;

  /* Initialize the per-section relaxation info.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	init_xtensa_relax_info (sec);
      }

  /* Mark relaxable sections (and count relocations against each one).  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	if (!find_relaxable_sections (abfd, sec, link_info, &is_relaxable))
	  return FALSE;
      }

  /* Bail out if there are no relaxable sections.  */
  if (!is_relaxable)
    return TRUE;

  /* Allocate space for source_relocs.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	xtensa_relax_info *relax_info;

	relax_info = get_xtensa_relax_info (sec);
	if (relax_info->is_relaxable_literal_section
	    || relax_info->is_relaxable_asm_section)
	  {
	    relax_info->src_relocs = (source_reloc *)
	      bfd_malloc (relax_info->src_count * sizeof (source_reloc));
	  }
      }

  /* Collect info on relocations against each relaxable section.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	if (!collect_source_relocs (abfd, sec, link_info))
	  return FALSE;
      }

  /* Compute the text actions.  */
  for (abfd = link_info->input_bfds; abfd != NULL; abfd = abfd->link_next)
    for (sec = abfd->sections; sec != NULL; sec = sec->next)
      {
	if (!compute_text_actions (abfd, sec, link_info))
	  return FALSE;
      }

  return TRUE;
}


/* Find all the sections that might be relaxed.  The motivation for
   this pass is that collect_source_relocs() needs to record _all_ the
   relocations that target each relaxable section.  That is expensive
   and unnecessary unless the target section is actually going to be
   relaxed.  This pass identifies all such sections by checking if
   they have L32Rs pointing to them.  In the process, the total number
   of relocations targeting each section is also counted so that we
   know how much space to allocate for source_relocs against each
   relaxable literal section.  */

static bfd_boolean
find_relaxable_sections (bfd *abfd,
			 asection *sec,
			 struct bfd_link_info *link_info,
			 bfd_boolean *is_relaxable_p)
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;
  xtensa_relax_info *source_relax_info;

  internal_relocs = retrieve_internal_relocs (abfd, sec,
					      link_info->keep_memory);
  if (internal_relocs == NULL) 
    return ok;

  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec->size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  source_relax_info = get_xtensa_relax_info (sec);
  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      r_reloc r_rel;
      asection *target_sec;
      xtensa_relax_info *target_relax_info;

      /* If this section has not already been marked as "relaxable", and
	 if it contains any ASM_EXPAND relocations (marking expanded
	 longcalls) that can be optimized into direct calls, then mark
	 the section as "relaxable".  */
      if (source_relax_info
	  && !source_relax_info->is_relaxable_asm_section
	  && ELF32_R_TYPE (irel->r_info) == R_XTENSA_ASM_EXPAND)
	{
	  bfd_boolean is_reachable = FALSE;
	  if (is_resolvable_asm_expansion (abfd, sec, contents, irel,
					   link_info, &is_reachable)
	      && is_reachable)
	    {
	      source_relax_info->is_relaxable_asm_section = TRUE;
	      *is_relaxable_p = TRUE;
	    }
	}

      r_reloc_init (&r_rel, abfd, irel, contents,
		    bfd_get_section_limit (abfd, sec));

      target_sec = r_reloc_get_section (&r_rel);
      target_relax_info = get_xtensa_relax_info (target_sec);
      if (!target_relax_info)
	continue;

      /* Count PC-relative operand relocations against the target section.
         Note: The conditions tested here must match the conditions under
	 which init_source_reloc is called in collect_source_relocs().  */
      if (is_operand_relocation (ELF32_R_TYPE (irel->r_info))
	  && (!is_alt_relocation (ELF32_R_TYPE (irel->r_info))
	      || is_l32r_relocation (abfd, sec, contents, irel)))
	target_relax_info->src_count++;

      if (is_l32r_relocation (abfd, sec, contents, irel)
	  && r_reloc_is_defined (&r_rel))
	{
	  /* Mark the target section as relaxable.  */
	  target_relax_info->is_relaxable_literal_section = TRUE;
	  *is_relaxable_p = TRUE;
	}
    }

 error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


/* Record _all_ the relocations that point to relaxable sections, and
   get rid of ASM_EXPAND relocs by either converting them to
   ASM_SIMPLIFY or by removing them.  */

static bfd_boolean
collect_source_relocs (bfd *abfd,
		       asection *sec,
		       struct bfd_link_info *link_info)
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;
  bfd_size_type sec_size;

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  if (internal_relocs == NULL) 
    return ok;

  sec_size = bfd_get_section_limit (abfd, sec);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  /* Record relocations against relaxable literal sections.  */
  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      r_reloc r_rel;
      asection *target_sec;
      xtensa_relax_info *target_relax_info;

      r_reloc_init (&r_rel, abfd, irel, contents, sec_size);

      target_sec = r_reloc_get_section (&r_rel);
      target_relax_info = get_xtensa_relax_info (target_sec);

      if (target_relax_info
	  && (target_relax_info->is_relaxable_literal_section
	      || target_relax_info->is_relaxable_asm_section))
	{
	  xtensa_opcode opcode = XTENSA_UNDEFINED;
	  int opnd = -1;
	  bfd_boolean is_abs_literal = FALSE;

	  if (is_alt_relocation (ELF32_R_TYPE (irel->r_info)))
	    {
	      /* None of the current alternate relocs are PC-relative,
		 and only PC-relative relocs matter here.  However, we
		 still need to record the opcode for literal
		 coalescing.  */
	      opcode = get_relocation_opcode (abfd, sec, contents, irel);
	      if (opcode == get_l32r_opcode ())
		{
		  is_abs_literal = TRUE;
		  opnd = 1;
		}
	      else
		opcode = XTENSA_UNDEFINED;
	    }
	  else if (is_operand_relocation (ELF32_R_TYPE (irel->r_info)))
	    {
	      opcode = get_relocation_opcode (abfd, sec, contents, irel);
	      opnd = get_relocation_opnd (opcode, ELF32_R_TYPE (irel->r_info));
	    }

	  if (opcode != XTENSA_UNDEFINED)
	    {
	      int src_next = target_relax_info->src_next++;
	      source_reloc *s_reloc = &target_relax_info->src_relocs[src_next];

	      init_source_reloc (s_reloc, sec, &r_rel, opcode, opnd,
				 is_abs_literal);
	    }
	}
    }

  /* Now get rid of ASM_EXPAND relocations.  At this point, the
     src_relocs array for the target literal section may still be
     incomplete, but it must at least contain the entries for the L32R
     relocations associated with ASM_EXPANDs because they were just
     added in the preceding loop over the relocations.  */

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      bfd_boolean is_reachable;

      if (!is_resolvable_asm_expansion (abfd, sec, contents, irel, link_info,
					&is_reachable))
	continue;

      if (is_reachable)
	{
	  Elf_Internal_Rela *l32r_irel;
	  r_reloc r_rel;
	  asection *target_sec;
	  xtensa_relax_info *target_relax_info;

	  /* Mark the source_reloc for the L32R so that it will be
	     removed in compute_removed_literals(), along with the
	     associated literal.  */
	  l32r_irel = find_associated_l32r_irel (abfd, sec, contents,
						 irel, internal_relocs);
	  if (l32r_irel == NULL)
	    continue;

	  r_reloc_init (&r_rel, abfd, l32r_irel, contents, sec_size);

	  target_sec = r_reloc_get_section (&r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && (target_relax_info->is_relaxable_literal_section
		  || target_relax_info->is_relaxable_asm_section))
	    {
	      source_reloc *s_reloc;

	      /* Search the source_relocs for the entry corresponding to
		 the l32r_irel.  Note: The src_relocs array is not yet
		 sorted, but it wouldn't matter anyway because we're
		 searching by source offset instead of target offset.  */
	      s_reloc = find_source_reloc (target_relax_info->src_relocs, 
					   target_relax_info->src_next,
					   sec, l32r_irel);
	      BFD_ASSERT (s_reloc);
	      s_reloc->is_null = TRUE;
	    }

	  /* Convert this reloc to ASM_SIMPLIFY.  */
	  irel->r_info = ELF32_R_INFO (ELF32_R_SYM (irel->r_info),
				       R_XTENSA_ASM_SIMPLIFY);
	  l32r_irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);

	  pin_internal_relocs (sec, internal_relocs);
	}
      else
	{
	  /* It is resolvable but doesn't reach.  We resolve now
	     by eliminating the relocation -- the call will remain
	     expanded into L32R/CALLX.  */
	  irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	  pin_internal_relocs (sec, internal_relocs);
	}
    }

 error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


/* Return TRUE if the asm expansion can be resolved.  Generally it can
   be resolved on a final link or when a partial link locates it in the
   same section as the target.  Set "is_reachable" flag if the target of
   the call is within the range of a direct call, given the current VMA
   for this section and the target section.  */

bfd_boolean
is_resolvable_asm_expansion (bfd *abfd,
			     asection *sec,
			     bfd_byte *contents,
			     Elf_Internal_Rela *irel,
			     struct bfd_link_info *link_info,
			     bfd_boolean *is_reachable_p)
{
  asection *target_sec;
  bfd_vma target_offset;
  r_reloc r_rel;
  xtensa_opcode opcode, direct_call_opcode;
  bfd_vma self_address;
  bfd_vma dest_address;
  bfd_boolean uses_l32r;
  bfd_size_type sec_size;

  *is_reachable_p = FALSE;

  if (contents == NULL)
    return FALSE;

  if (ELF32_R_TYPE (irel->r_info) != R_XTENSA_ASM_EXPAND) 
    return FALSE;

  sec_size = bfd_get_section_limit (abfd, sec);
  opcode = get_expanded_call_opcode (contents + irel->r_offset,
				     sec_size - irel->r_offset, &uses_l32r);
  /* Optimization of longcalls that use CONST16 is not yet implemented.  */
  if (!uses_l32r)
    return FALSE;
  
  direct_call_opcode = swap_callx_for_call_opcode (opcode);
  if (direct_call_opcode == XTENSA_UNDEFINED)
    return FALSE;

  /* Check and see that the target resolves.  */
  r_reloc_init (&r_rel, abfd, irel, contents, sec_size);
  if (!r_reloc_is_defined (&r_rel))
    return FALSE;

  target_sec = r_reloc_get_section (&r_rel);
  target_offset = r_rel.target_offset;

  /* If the target is in a shared library, then it doesn't reach.  This
     isn't supposed to come up because the compiler should never generate
     non-PIC calls on systems that use shared libraries, but the linker
     shouldn't crash regardless.  */
  if (!target_sec->output_section)
    return FALSE;
      
  /* For relocatable sections, we can only simplify when the output
     section of the target is the same as the output section of the
     source.  */
  if (link_info->relocatable
      && (target_sec->output_section != sec->output_section
	  || is_reloc_sym_weak (abfd, irel)))
    return FALSE;

  self_address = (sec->output_section->vma
		  + sec->output_offset + irel->r_offset + 3);
  dest_address = (target_sec->output_section->vma
		  + target_sec->output_offset + target_offset);
      
  *is_reachable_p = pcrel_reloc_fits (direct_call_opcode, 0,
				      self_address, dest_address);

  if ((self_address >> CALL_SEGMENT_BITS) !=
      (dest_address >> CALL_SEGMENT_BITS))
    return FALSE;

  return TRUE;
}


static Elf_Internal_Rela *
find_associated_l32r_irel (bfd *abfd,
			   asection *sec,
			   bfd_byte *contents,
			   Elf_Internal_Rela *other_irel,
			   Elf_Internal_Rela *internal_relocs)
{
  unsigned i;

  for (i = 0; i < sec->reloc_count; i++) 
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];

      if (irel == other_irel)
	continue;
      if (irel->r_offset != other_irel->r_offset)
	continue;
      if (is_l32r_relocation (abfd, sec, contents, irel))
	return irel;
    }

  return NULL;
}


static xtensa_opcode *
build_reloc_opcodes (bfd *abfd,
		     asection *sec,
		     bfd_byte *contents,
		     Elf_Internal_Rela *internal_relocs)
{
  unsigned i;
  xtensa_opcode *reloc_opcodes =
    (xtensa_opcode *) bfd_malloc (sizeof (xtensa_opcode) * sec->reloc_count);
  for (i = 0; i < sec->reloc_count; i++)
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      reloc_opcodes[i] = get_relocation_opcode (abfd, sec, contents, irel);
    }
  return reloc_opcodes;
}


/* The compute_text_actions function will build a list of potential
   transformation actions for code in the extended basic block of each
   longcall that is optimized to a direct call.  From this list we
   generate a set of actions to actually perform that optimizes for
   space and, if not using size_opt, maintains branch target
   alignments.

   These actions to be performed are placed on a per-section list.
   The actual changes are performed by relax_section() in the second
   pass.  */

bfd_boolean
compute_text_actions (bfd *abfd,
		      asection *sec,
		      struct bfd_link_info *link_info)
{
  xtensa_opcode *reloc_opcodes = NULL;
  xtensa_relax_info *relax_info;
  bfd_byte *contents;
  Elf_Internal_Rela *internal_relocs;
  bfd_boolean ok = TRUE;
  unsigned i;
  property_table_entry *prop_table = 0;
  int ptblsize = 0;
  bfd_size_type sec_size;
  static bfd_boolean no_insn_move = FALSE;

  if (no_insn_move)
    return ok;

  /* Do nothing if the section contains no optimized longcalls.  */
  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);
  if (!relax_info->is_relaxable_asm_section)
    return ok;

  internal_relocs = retrieve_internal_relocs (abfd, sec,
					      link_info->keep_memory);

  if (internal_relocs)
    qsort (internal_relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
	   internal_reloc_compare);

  sec_size = bfd_get_section_limit (abfd, sec);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  ptblsize = xtensa_read_table_entries (abfd, sec, &prop_table,
					XTENSA_PROP_SEC_NAME, FALSE);
  if (ptblsize < 0)
    {
      ok = FALSE;
      goto error_return;
    }

  for (i = 0; i < sec->reloc_count; i++)
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      bfd_vma r_offset;
      property_table_entry *the_entry;
      int ptbl_idx;
      ebb_t *ebb;
      ebb_constraint ebb_table;
      bfd_size_type simplify_size;

      if (irel && ELF32_R_TYPE (irel->r_info) != R_XTENSA_ASM_SIMPLIFY)
	continue;
      r_offset = irel->r_offset;

      simplify_size = get_asm_simplify_size (contents, sec_size, r_offset);
      if (simplify_size == 0)
	{
	  (*_bfd_error_handler)
	    (_("%B(%A+0x%lx): could not decode instruction for XTENSA_ASM_SIMPLIFY relocation; possible configuration mismatch"),
	     sec->owner, sec, r_offset);
	  continue;
	}

      /* If the instruction table is not around, then don't do this
	 relaxation.  */
      the_entry = elf_xtensa_find_property_entry (prop_table, ptblsize,
						  sec->vma + irel->r_offset);
      if (the_entry == NULL || XTENSA_NO_NOP_REMOVAL)
	{
	  text_action_add (&relax_info->action_list,
			   ta_convert_longcall, sec, r_offset,
			   0);
	  continue;
	}

      /* If the next longcall happens to be at the same address as an
	 unreachable section of size 0, then skip forward.  */
      ptbl_idx = the_entry - prop_table;
      while ((the_entry->flags & XTENSA_PROP_UNREACHABLE)
	     && the_entry->size == 0
	     && ptbl_idx + 1 < ptblsize
	     && (prop_table[ptbl_idx + 1].address
		 == prop_table[ptbl_idx].address))
	{
	  ptbl_idx++;
	  the_entry++;
	}

      if (the_entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM)
	  /* NO_REORDER is OK */
	continue;

      init_ebb_constraint (&ebb_table);
      ebb = &ebb_table.ebb;
      init_ebb (ebb, sec, contents, sec_size, prop_table, ptblsize,
		internal_relocs, sec->reloc_count);
      ebb->start_offset = r_offset + simplify_size;
      ebb->end_offset = r_offset + simplify_size;
      ebb->start_ptbl_idx = ptbl_idx;
      ebb->end_ptbl_idx = ptbl_idx;
      ebb->start_reloc_idx = i;
      ebb->end_reloc_idx = i;

      /* Precompute the opcode for each relocation.  */
      if (reloc_opcodes == NULL)
	reloc_opcodes = build_reloc_opcodes (abfd, sec, contents,
					     internal_relocs);

      if (!extend_ebb_bounds (ebb)
	  || !compute_ebb_proposed_actions (&ebb_table)
	  || !compute_ebb_actions (&ebb_table)
	  || !check_section_ebb_pcrels_fit (abfd, sec, contents,
					    internal_relocs, &ebb_table,
					    reloc_opcodes)
	  || !check_section_ebb_reduces (&ebb_table))
	{
	  /* If anything goes wrong or we get unlucky and something does
	     not fit, with our plan because of expansion between
	     critical branches, just convert to a NOP.  */

	  text_action_add (&relax_info->action_list,
			   ta_convert_longcall, sec, r_offset, 0);
	  i = ebb_table.ebb.end_reloc_idx;
	  free_ebb_constraint (&ebb_table);
	  continue;
	}

      text_action_add_proposed (&relax_info->action_list, &ebb_table, sec);

      /* Update the index so we do not go looking at the relocations
	 we have already processed.  */
      i = ebb_table.ebb.end_reloc_idx;
      free_ebb_constraint (&ebb_table);
    }

#if DEBUG
  if (relax_info->action_list.head)
    print_action_list (stderr, &relax_info->action_list);
#endif

error_return:
  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  if (prop_table)
    free (prop_table);
  if (reloc_opcodes)
    free (reloc_opcodes);

  return ok;
}


/* Find all of the possible actions for an extended basic block.  */

bfd_boolean
compute_ebb_proposed_actions (ebb_constraint *ebb_table)
{
  const ebb_t *ebb = &ebb_table->ebb;
  unsigned rel_idx = ebb->start_reloc_idx;
  property_table_entry *entry, *start_entry, *end_entry;

  start_entry = &ebb->ptbl[ebb->start_ptbl_idx];
  end_entry = &ebb->ptbl[ebb->end_ptbl_idx];

  for (entry = start_entry; entry <= end_entry; entry++)
    {
      bfd_vma offset, start_offset, end_offset;
      bfd_size_type insn_len;

      start_offset = entry->address - ebb->sec->vma;
      end_offset = entry->address + entry->size - ebb->sec->vma;

      if (entry == start_entry)
	start_offset = ebb->start_offset;
      if (entry == end_entry)
	end_offset = ebb->end_offset;
      offset = start_offset;

      if (offset == entry->address - ebb->sec->vma
	  && (entry->flags & XTENSA_PROP_INSN_BRANCH_TARGET) != 0)
	{
	  enum ebb_target_enum align_type = EBB_DESIRE_TGT_ALIGN;
	  BFD_ASSERT (offset != end_offset);
	  if (offset == end_offset)
	    return FALSE;

	  insn_len = insn_decode_len (ebb->contents, ebb->content_length,
				      offset);

	  /* Propose no actions for a section with an undecodable offset.  */
	  if (insn_len == 0) 
	    {
	      (*_bfd_error_handler)
		(_("%B(%A+0x%lx): could not decode instruction; possible configuration mismatch"),
		 ebb->sec->owner, ebb->sec, offset);
	      return FALSE;
	    }
	  if (check_branch_target_aligned_address (offset, insn_len))
	    align_type = EBB_REQUIRE_TGT_ALIGN;

	  ebb_propose_action (ebb_table, align_type, 0,
			      ta_none, offset, 0, TRUE);
	}

      while (offset != end_offset)
	{
	  Elf_Internal_Rela *irel;
	  xtensa_opcode opcode;

	  while (rel_idx < ebb->end_reloc_idx
		 && (ebb->relocs[rel_idx].r_offset < offset
		     || (ebb->relocs[rel_idx].r_offset == offset
			 && (ELF32_R_TYPE (ebb->relocs[rel_idx].r_info)
			     != R_XTENSA_ASM_SIMPLIFY))))
	    rel_idx++;

	  /* Check for longcall.  */
	  irel = &ebb->relocs[rel_idx];
	  if (irel->r_offset == offset
	      && ELF32_R_TYPE (irel->r_info) == R_XTENSA_ASM_SIMPLIFY)
	    {
	      bfd_size_type simplify_size;

	      simplify_size = get_asm_simplify_size (ebb->contents, 
						     ebb->content_length,
						     irel->r_offset);
	      if (simplify_size == 0)
		{
		  (*_bfd_error_handler)
		    (_("%B(%A+0x%lx): could not decode instruction for XTENSA_ASM_SIMPLIFY relocation; possible configuration mismatch"),
		     ebb->sec->owner, ebb->sec, offset);
		  return FALSE;
		}

	      ebb_propose_action (ebb_table, EBB_NO_ALIGN, 0,
				  ta_convert_longcall, offset, 0, TRUE);
	      
	      offset += simplify_size;
	      continue;
	    }

	  insn_len = insn_decode_len (ebb->contents, ebb->content_length,
				      offset);
	  /* If the instruction is undecodable, then report an error.  */
	  if (insn_len == 0)
	    {
	      (*_bfd_error_handler)
		(_("%B(%A+0x%lx): could not decode instruction; possible configuration mismatch"),
		 ebb->sec->owner, ebb->sec, offset);
	      return FALSE;
	    }
	    
	  if ((entry->flags & XTENSA_PROP_INSN_NO_DENSITY) == 0
	      && (entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM) == 0
	      && narrow_instruction (ebb->contents, ebb->content_length,
				     offset, FALSE))
	    {
	      /* Add an instruction narrow action.  */
	      ebb_propose_action (ebb_table, EBB_NO_ALIGN, 0,
				  ta_narrow_insn, offset, 0, FALSE);
	      offset += insn_len;
	      continue;
	    }
	  if ((entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM) == 0
	      && widen_instruction (ebb->contents, ebb->content_length,
				    offset, FALSE))
	    {
	      /* Add an instruction widen action.  */
	      ebb_propose_action (ebb_table, EBB_NO_ALIGN, 0,
				  ta_widen_insn, offset, 0, FALSE);
	      offset += insn_len;
	      continue;
	    }
	  opcode = insn_decode_opcode (ebb->contents, ebb->content_length,
				       offset, 0);
	  if (xtensa_opcode_is_loop (xtensa_default_isa, opcode))
	    {
	      /* Check for branch targets.  */
	      ebb_propose_action (ebb_table, EBB_REQUIRE_LOOP_ALIGN, 0,
				  ta_none, offset, 0, TRUE);
	      offset += insn_len;
	      continue;
	    }

	  offset += insn_len;
	}
    }

  if (ebb->ends_unreachable)
    {
      ebb_propose_action (ebb_table, EBB_NO_ALIGN, 0,
			  ta_fill, ebb->end_offset, 0, TRUE);
    }

  return TRUE;
}


/* After all of the information has collected about the
   transformations possible in an EBB, compute the appropriate actions
   here in compute_ebb_actions.  We still must check later to make
   sure that the actions do not break any relocations.  The algorithm
   used here is pretty greedy.  Basically, it removes as many no-ops
   as possible so that the end of the EBB has the same alignment
   characteristics as the original.  First, it uses narrowing, then
   fill space at the end of the EBB, and finally widenings.  If that
   does not work, it tries again with one fewer no-op removed.  The
   optimization will only be performed if all of the branch targets
   that were aligned before transformation are also aligned after the
   transformation.

   When the size_opt flag is set, ignore the branch target alignments,
   narrow all wide instructions, and remove all no-ops unless the end
   of the EBB prevents it.  */

bfd_boolean
compute_ebb_actions (ebb_constraint *ebb_table)
{
  unsigned i = 0;
  unsigned j;
  int removed_bytes = 0;
  ebb_t *ebb = &ebb_table->ebb;
  unsigned seg_idx_start = 0;
  unsigned seg_idx_end = 0;

  /* We perform this like the assembler relaxation algorithm: Start by
     assuming all instructions are narrow and all no-ops removed; then
     walk through....  */

  /* For each segment of this that has a solid constraint, check to
     see if there are any combinations that will keep the constraint.
     If so, use it.  */
  for (seg_idx_end = 0; seg_idx_end < ebb_table->action_count; seg_idx_end++)
    {
      bfd_boolean requires_text_end_align = FALSE;
      unsigned longcall_count = 0;
      unsigned longcall_convert_count = 0;
      unsigned narrowable_count = 0;
      unsigned narrowable_convert_count = 0;
      unsigned widenable_count = 0;
      unsigned widenable_convert_count = 0;

      proposed_action *action = NULL;
      int align = (1 << ebb_table->ebb.sec->alignment_power);

      seg_idx_start = seg_idx_end;

      for (i = seg_idx_start; i < ebb_table->action_count; i++)
	{
	  action = &ebb_table->actions[i];
	  if (action->action == ta_convert_longcall)
	    longcall_count++;
	  if (action->action == ta_narrow_insn)
	    narrowable_count++;
	  if (action->action == ta_widen_insn)
	    widenable_count++;
	  if (action->action == ta_fill)
	    break;
	  if (action->align_type == EBB_REQUIRE_LOOP_ALIGN)
	    break;
	  if (action->align_type == EBB_REQUIRE_TGT_ALIGN
	      && !elf32xtensa_size_opt)
	    break;
	}
      seg_idx_end = i;

      if (seg_idx_end == ebb_table->action_count && !ebb->ends_unreachable)
	requires_text_end_align = TRUE;

      if (elf32xtensa_size_opt && !requires_text_end_align
	  && action->align_type != EBB_REQUIRE_LOOP_ALIGN
	  && action->align_type != EBB_REQUIRE_TGT_ALIGN)
	{
	  longcall_convert_count = longcall_count;
	  narrowable_convert_count = narrowable_count;
	  widenable_convert_count = 0;
	}
      else
	{
	  /* There is a constraint.  Convert the max number of longcalls.  */
	  narrowable_convert_count = 0;
	  longcall_convert_count = 0;
	  widenable_convert_count = 0;

	  for (j = 0; j < longcall_count; j++)
	    {
	      int removed = (longcall_count - j) * 3 & (align - 1);
	      unsigned desire_narrow = (align - removed) & (align - 1);
	      unsigned desire_widen = removed;
	      if (desire_narrow <= narrowable_count)
		{
		  narrowable_convert_count = desire_narrow;
		  narrowable_convert_count +=
		    (align * ((narrowable_count - narrowable_convert_count)
			      / align));
		  longcall_convert_count = (longcall_count - j);
		  widenable_convert_count = 0;
		  break;
		}
	      if (desire_widen <= widenable_count && !elf32xtensa_size_opt)
		{
		  narrowable_convert_count = 0;
		  longcall_convert_count = longcall_count - j;
		  widenable_convert_count = desire_widen;
		  break;
		}
	    }
	}

      /* Now the number of conversions are saved.  Do them.  */
      for (i = seg_idx_start; i < seg_idx_end; i++)
	{
	  action = &ebb_table->actions[i];
	  switch (action->action)
	    {
	    case ta_convert_longcall:
	      if (longcall_convert_count != 0)
		{
		  action->action = ta_remove_longcall;
		  action->do_action = TRUE;
		  action->removed_bytes += 3;
		  longcall_convert_count--;
		}
	      break;
	    case ta_narrow_insn:
	      if (narrowable_convert_count != 0)
		{
		  action->do_action = TRUE;
		  action->removed_bytes += 1;
		  narrowable_convert_count--;
		}
	      break;
	    case ta_widen_insn:
	      if (widenable_convert_count != 0)
		{
		  action->do_action = TRUE;
		  action->removed_bytes -= 1;
		  widenable_convert_count--;
		}
	      break;
	    default:
	      break;
	    }
	}
    }

  /* Now we move on to some local opts.  Try to remove each of the
     remaining longcalls.  */

  if (ebb_table->ebb.ends_section || ebb_table->ebb.ends_unreachable)
    {
      removed_bytes = 0;
      for (i = 0; i < ebb_table->action_count; i++)
	{
	  int old_removed_bytes = removed_bytes;
	  proposed_action *action = &ebb_table->actions[i];

	  if (action->do_action && action->action == ta_convert_longcall)
	    {
	      bfd_boolean bad_alignment = FALSE;
	      removed_bytes += 3;
	      for (j = i + 1; j < ebb_table->action_count; j++)
		{
		  proposed_action *new_action = &ebb_table->actions[j];
		  bfd_vma offset = new_action->offset;
		  if (new_action->align_type == EBB_REQUIRE_TGT_ALIGN)
		    {
		      if (!check_branch_target_aligned
			  (ebb_table->ebb.contents,
			   ebb_table->ebb.content_length,
			   offset, offset - removed_bytes))
			{
			  bad_alignment = TRUE;
			  break;
			}
		    }
		  if (new_action->align_type == EBB_REQUIRE_LOOP_ALIGN)
		    {
		      if (!check_loop_aligned (ebb_table->ebb.contents,
					       ebb_table->ebb.content_length,
					       offset,
					       offset - removed_bytes))
			{
			  bad_alignment = TRUE;
			  break;
			}
		    }
		  if (new_action->action == ta_narrow_insn
		      && !new_action->do_action
		      && ebb_table->ebb.sec->alignment_power == 2)
		    {
		      /* Narrow an instruction and we are done.  */
		      new_action->do_action = TRUE;
		      new_action->removed_bytes += 1;
		      bad_alignment = FALSE;
		      break;
		    }
		  if (new_action->action == ta_widen_insn
		      && new_action->do_action
		      && ebb_table->ebb.sec->alignment_power == 2)
		    {
		      /* Narrow an instruction and we are done.  */
		      new_action->do_action = FALSE;
		      new_action->removed_bytes += 1;
		      bad_alignment = FALSE;
		      break;
		    }
		}
	      if (!bad_alignment)
		{
		  action->removed_bytes += 3;
		  action->action = ta_remove_longcall;
		  action->do_action = TRUE;
		}
	    }
	  removed_bytes = old_removed_bytes;
	  if (action->do_action)
	    removed_bytes += action->removed_bytes;
	}
    }

  removed_bytes = 0;
  for (i = 0; i < ebb_table->action_count; ++i)
    {
      proposed_action *action = &ebb_table->actions[i];
      if (action->do_action)
	removed_bytes += action->removed_bytes;
    }

  if ((removed_bytes % (1 << ebb_table->ebb.sec->alignment_power)) != 0
      && ebb->ends_unreachable)
    {
      proposed_action *action;
      int br;
      int extra_space;

      BFD_ASSERT (ebb_table->action_count != 0);
      action = &ebb_table->actions[ebb_table->action_count - 1];
      BFD_ASSERT (action->action == ta_fill);
      BFD_ASSERT (ebb->ends_unreachable->flags & XTENSA_PROP_UNREACHABLE);

      extra_space = compute_fill_extra_space (ebb->ends_unreachable);
      br = action->removed_bytes + removed_bytes + extra_space;
      br = br & ((1 << ebb->sec->alignment_power ) - 1);

      action->removed_bytes = extra_space - br;
    }
  return TRUE;
}


/* The xlate_map is a sorted array of address mappings designed to
   answer the offset_with_removed_text() query with a binary search instead
   of a linear search through the section's action_list.  */

typedef struct xlate_map_entry xlate_map_entry_t;
typedef struct xlate_map xlate_map_t;

struct xlate_map_entry
{
  unsigned orig_address;
  unsigned new_address;
  unsigned size;
};

struct xlate_map
{
  unsigned entry_count;
  xlate_map_entry_t *entry;
};


static int 
xlate_compare (const void *a_v, const void *b_v)
{
  const xlate_map_entry_t *a = (const xlate_map_entry_t *) a_v;
  const xlate_map_entry_t *b = (const xlate_map_entry_t *) b_v;
  if (a->orig_address < b->orig_address)
    return -1;
  if (a->orig_address > (b->orig_address + b->size - 1))
    return 1;
  return 0;
}


static bfd_vma
xlate_offset_with_removed_text (const xlate_map_t *map,
				text_action_list *action_list,
				bfd_vma offset)
{
  xlate_map_entry_t tmp;
  void *r;
  xlate_map_entry_t *e;

  if (map == NULL)
    return offset_with_removed_text (action_list, offset);

  if (map->entry_count == 0)
    return offset;

  tmp.orig_address = offset;
  tmp.new_address = offset;
  tmp.size = 1;

  r = bsearch (&offset, map->entry, map->entry_count,
	       sizeof (xlate_map_entry_t), &xlate_compare);
  e = (xlate_map_entry_t *) r;
  
  BFD_ASSERT (e != NULL);
  if (e == NULL)
    return offset;
  return e->new_address - e->orig_address + offset;
}


/* Build a binary searchable offset translation map from a section's
   action list.  */

static xlate_map_t *
build_xlate_map (asection *sec, xtensa_relax_info *relax_info)
{
  xlate_map_t *map = (xlate_map_t *) bfd_malloc (sizeof (xlate_map_t));
  text_action_list *action_list = &relax_info->action_list;
  unsigned num_actions = 0;
  text_action *r;
  int removed;
  xlate_map_entry_t *current_entry;

  if (map == NULL)
    return NULL;

  num_actions = action_list_count (action_list);
  map->entry = (xlate_map_entry_t *) 
    bfd_malloc (sizeof (xlate_map_entry_t) * (num_actions + 1));
  if (map->entry == NULL)
    {
      free (map);
      return NULL;
    }
  map->entry_count = 0;
  
  removed = 0;
  current_entry = &map->entry[0];

  current_entry->orig_address = 0;
  current_entry->new_address = 0;
  current_entry->size = 0;

  for (r = action_list->head; r != NULL; r = r->next)
    {
      unsigned orig_size = 0;
      switch (r->action)
	{
	case ta_none:
	case ta_remove_insn:
	case ta_convert_longcall:
	case ta_remove_literal:
	case ta_add_literal:
	  break;
	case ta_remove_longcall:
	  orig_size = 6;
	  break;
	case ta_narrow_insn:
	  orig_size = 3;
	  break;
	case ta_widen_insn:
	  orig_size = 2;
	  break;
	case ta_fill:
	  break;
	}
      current_entry->size =
	r->offset + orig_size - current_entry->orig_address;
      if (current_entry->size != 0)
	{
	  current_entry++;
	  map->entry_count++;
	}
      current_entry->orig_address = r->offset + orig_size;
      removed += r->removed_bytes;
      current_entry->new_address = r->offset + orig_size - removed;
      current_entry->size = 0;
    }

  current_entry->size = (bfd_get_section_limit (sec->owner, sec)
			 - current_entry->orig_address);
  if (current_entry->size != 0)
    map->entry_count++;

  return map;
}


/* Free an offset translation map.  */

static void 
free_xlate_map (xlate_map_t *map)
{
  if (map && map->entry)
    free (map->entry);
  if (map)
    free (map);
}


/* Use check_section_ebb_pcrels_fit to make sure that all of the
   relocations in a section will fit if a proposed set of actions
   are performed.  */

static bfd_boolean
check_section_ebb_pcrels_fit (bfd *abfd,
			      asection *sec,
			      bfd_byte *contents,
			      Elf_Internal_Rela *internal_relocs,
			      const ebb_constraint *constraint,
			      const xtensa_opcode *reloc_opcodes)
{
  unsigned i, j;
  Elf_Internal_Rela *irel;
  xlate_map_t *xmap = NULL;
  bfd_boolean ok = TRUE;
  xtensa_relax_info *relax_info;

  relax_info = get_xtensa_relax_info (sec);

  if (relax_info && sec->reloc_count > 100)
    {
      xmap = build_xlate_map (sec, relax_info);
      /* NULL indicates out of memory, but the slow version
	 can still be used.  */
    }

  for (i = 0; i < sec->reloc_count; i++)
    {
      r_reloc r_rel;
      bfd_vma orig_self_offset, orig_target_offset;
      bfd_vma self_offset, target_offset;
      int r_type;
      reloc_howto_type *howto;
      int self_removed_bytes, target_removed_bytes;

      irel = &internal_relocs[i];
      r_type = ELF32_R_TYPE (irel->r_info);

      howto = &elf_howto_table[r_type];
      /* We maintain the required invariant: PC-relative relocations
	 that fit before linking must fit after linking.  Thus we only
	 need to deal with relocations to the same section that are
	 PC-relative.  */
      if (ELF32_R_TYPE (irel->r_info) == R_XTENSA_ASM_SIMPLIFY
	  || !howto->pc_relative)
	continue;

      r_reloc_init (&r_rel, abfd, irel, contents,
		    bfd_get_section_limit (abfd, sec));

      if (r_reloc_get_section (&r_rel) != sec)
	continue;

      orig_self_offset = irel->r_offset;
      orig_target_offset = r_rel.target_offset;

      self_offset = orig_self_offset;
      target_offset = orig_target_offset;

      if (relax_info)
	{
	  self_offset =
	    xlate_offset_with_removed_text (xmap, &relax_info->action_list,
					    orig_self_offset);
	  target_offset =
	    xlate_offset_with_removed_text (xmap, &relax_info->action_list,
					    orig_target_offset);
	}

      self_removed_bytes = 0;
      target_removed_bytes = 0;

      for (j = 0; j < constraint->action_count; ++j)
	{
	  proposed_action *action = &constraint->actions[j];
	  bfd_vma offset = action->offset;
	  int removed_bytes = action->removed_bytes;
	  if (offset < orig_self_offset
	      || (offset == orig_self_offset && action->action == ta_fill
		  && action->removed_bytes < 0))
	    self_removed_bytes += removed_bytes;
	  if (offset < orig_target_offset
	      || (offset == orig_target_offset && action->action == ta_fill
		  && action->removed_bytes < 0))
	    target_removed_bytes += removed_bytes;
	}
      self_offset -= self_removed_bytes;
      target_offset -= target_removed_bytes;

      /* Try to encode it.  Get the operand and check.  */
      if (is_alt_relocation (ELF32_R_TYPE (irel->r_info)))
	{
	  /* None of the current alternate relocs are PC-relative,
	     and only PC-relative relocs matter here.  */
	}
      else
	{
	  xtensa_opcode opcode;
	  int opnum;

	  if (reloc_opcodes)
	    opcode = reloc_opcodes[i];
	  else
	    opcode = get_relocation_opcode (abfd, sec, contents, irel);
	  if (opcode == XTENSA_UNDEFINED)
	    {
	      ok = FALSE;
	      break;
	    }

	  opnum = get_relocation_opnd (opcode, ELF32_R_TYPE (irel->r_info));
	  if (opnum == XTENSA_UNDEFINED)
	    {
	      ok = FALSE;
	      break;
	    }

	  if (!pcrel_reloc_fits (opcode, opnum, self_offset, target_offset))
	    {
	      ok = FALSE;
	      break;
	    }
	}
    }

  if (xmap)
    free_xlate_map (xmap);

  return ok;
}


static bfd_boolean
check_section_ebb_reduces (const ebb_constraint *constraint)
{
  int removed = 0;
  unsigned i;

  for (i = 0; i < constraint->action_count; i++)
    {
      const proposed_action *action = &constraint->actions[i];
      if (action->do_action)
	removed += action->removed_bytes;
    }
  if (removed < 0)
    return FALSE;

  return TRUE;
}


void
text_action_add_proposed (text_action_list *l,
			  const ebb_constraint *ebb_table,
			  asection *sec)
{
  unsigned i;

  for (i = 0; i < ebb_table->action_count; i++)
    {
      proposed_action *action = &ebb_table->actions[i];

      if (!action->do_action)
	continue;
      switch (action->action)
	{
	case ta_remove_insn:
	case ta_remove_longcall:
	case ta_convert_longcall:
	case ta_narrow_insn:
	case ta_widen_insn:
	case ta_fill:
	case ta_remove_literal:
	  text_action_add (l, action->action, sec, action->offset,
			   action->removed_bytes);
	  break;
	case ta_none:
	  break;
	default:
	  BFD_ASSERT (0);
	  break;
	}
    }
}


int
compute_fill_extra_space (property_table_entry *entry)
{
  int fill_extra_space;

  if (!entry)
    return 0;

  if ((entry->flags & XTENSA_PROP_UNREACHABLE) == 0)
    return 0;

  fill_extra_space = entry->size;
  if ((entry->flags & XTENSA_PROP_ALIGN) != 0)
    {
      /* Fill bytes for alignment:
	 (2**n)-1 - (addr + (2**n)-1) & (2**n -1) */
      int pow = GET_XTENSA_PROP_ALIGNMENT (entry->flags);
      int nsm = (1 << pow) - 1;
      bfd_vma addr = entry->address + entry->size;
      bfd_vma align_fill = nsm - ((addr + nsm) & nsm);
      fill_extra_space += align_fill;
    }
  return fill_extra_space;
}


/* First relaxation pass.  */

/* If the section contains relaxable literals, check each literal to
   see if it has the same value as another literal that has already
   been seen, either in the current section or a previous one.  If so,
   add an entry to the per-section list of removed literals.  The
   actual changes are deferred until the next pass.  */

static bfd_boolean 
compute_removed_literals (bfd *abfd,
			  asection *sec,
			  struct bfd_link_info *link_info,
			  value_map_hash_table *values)
{
  xtensa_relax_info *relax_info;
  bfd_byte *contents;
  Elf_Internal_Rela *internal_relocs;
  source_reloc *src_relocs, *rel;
  bfd_boolean ok = TRUE;
  property_table_entry *prop_table = NULL;
  int ptblsize;
  int i, prev_i;
  bfd_boolean last_loc_is_prev = FALSE;
  bfd_vma last_target_offset = 0;
  section_cache_t target_sec_cache;
  bfd_size_type sec_size;

  init_section_cache (&target_sec_cache);

  /* Do nothing if it is not a relaxable literal section.  */
  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);
  if (!relax_info->is_relaxable_literal_section)
    return ok;

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);

  sec_size = bfd_get_section_limit (abfd, sec);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  /* Sort the source_relocs by target offset.  */
  src_relocs = relax_info->src_relocs;
  qsort (src_relocs, relax_info->src_count,
	 sizeof (source_reloc), source_reloc_compare);
  qsort (internal_relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
	 internal_reloc_compare);

  ptblsize = xtensa_read_table_entries (abfd, sec, &prop_table,
					XTENSA_PROP_SEC_NAME, FALSE);
  if (ptblsize < 0)
    {
      ok = FALSE;
      goto error_return;
    }

  prev_i = -1;
  for (i = 0; i < relax_info->src_count; i++)
    {
      Elf_Internal_Rela *irel = NULL;

      rel = &src_relocs[i];
      if (get_l32r_opcode () != rel->opcode)
	continue;
      irel = get_irel_at_offset (sec, internal_relocs,
				 rel->r_rel.target_offset);

      /* If the relocation on this is not a simple R_XTENSA_32 or
	 R_XTENSA_PLT then do not consider it.  This may happen when
	 the difference of two symbols is used in a literal.  */
      if (irel && (ELF32_R_TYPE (irel->r_info) != R_XTENSA_32
		   && ELF32_R_TYPE (irel->r_info) != R_XTENSA_PLT))
	continue;

      /* If the target_offset for this relocation is the same as the
	 previous relocation, then we've already considered whether the
	 literal can be coalesced.  Skip to the next one....  */
      if (i != 0 && prev_i != -1
	  && src_relocs[i-1].r_rel.target_offset == rel->r_rel.target_offset)
	continue;
      prev_i = i;

      if (last_loc_is_prev && 
	  last_target_offset + 4 != rel->r_rel.target_offset)
	last_loc_is_prev = FALSE;

      /* Check if the relocation was from an L32R that is being removed
	 because a CALLX was converted to a direct CALL, and check if
	 there are no other relocations to the literal.  */
      if (is_removable_literal (rel, i, src_relocs, relax_info->src_count))
	{
	  if (!remove_dead_literal (abfd, sec, link_info, internal_relocs,
				    irel, rel, prop_table, ptblsize))
	    {
	      ok = FALSE;
	      goto error_return;
	    }
	  last_target_offset = rel->r_rel.target_offset;
	  continue;
	}

      if (!identify_literal_placement (abfd, sec, contents, link_info,
				       values, 
				       &last_loc_is_prev, irel, 
				       relax_info->src_count - i, rel,
				       prop_table, ptblsize,
				       &target_sec_cache, rel->is_abs_literal))
	{
	  ok = FALSE;
	  goto error_return;
	}
      last_target_offset = rel->r_rel.target_offset;
    }

#if DEBUG
  print_removed_literals (stderr, &relax_info->removed_list);
  print_action_list (stderr, &relax_info->action_list);
#endif /* DEBUG */

error_return:
  if (prop_table) free (prop_table);
  clear_section_cache (&target_sec_cache);

  release_contents (sec, contents);
  release_internal_relocs (sec, internal_relocs);
  return ok;
}


static Elf_Internal_Rela *
get_irel_at_offset (asection *sec,
		    Elf_Internal_Rela *internal_relocs,
		    bfd_vma offset)
{
  unsigned i;
  Elf_Internal_Rela *irel;
  unsigned r_type;
  Elf_Internal_Rela key;

  if (!internal_relocs) 
    return NULL;

  key.r_offset = offset;
  irel = bsearch (&key, internal_relocs, sec->reloc_count,
		  sizeof (Elf_Internal_Rela), internal_reloc_matches);
  if (!irel)
    return NULL;

  /* bsearch does not guarantee which will be returned if there are
     multiple matches.  We need the first that is not an alignment.  */
  i = irel - internal_relocs;
  while (i > 0)
    {
      if (internal_relocs[i-1].r_offset != offset)
	break;
      i--;
    }
  for ( ; i < sec->reloc_count; i++)
    {
      irel = &internal_relocs[i];
      r_type = ELF32_R_TYPE (irel->r_info);
      if (irel->r_offset == offset && r_type != R_XTENSA_NONE)
	return irel;
    }

  return NULL;
}


bfd_boolean
is_removable_literal (const source_reloc *rel,
		      int i,
		      const source_reloc *src_relocs,
		      int src_count)
{
  const source_reloc *curr_rel;
  if (!rel->is_null)
    return FALSE;
  
  for (++i; i < src_count; ++i)
    {
      curr_rel = &src_relocs[i];
      /* If all others have the same target offset....  */
      if (curr_rel->r_rel.target_offset != rel->r_rel.target_offset)
	return TRUE;

      if (!curr_rel->is_null
	  && !xtensa_is_property_section (curr_rel->source_sec)
	  && !(curr_rel->source_sec->flags & SEC_DEBUGGING))
	return FALSE;
    }
  return TRUE;
}


bfd_boolean 
remove_dead_literal (bfd *abfd,
		     asection *sec,
		     struct bfd_link_info *link_info,
		     Elf_Internal_Rela *internal_relocs,
		     Elf_Internal_Rela *irel,
		     source_reloc *rel,
		     property_table_entry *prop_table,
		     int ptblsize)
{
  property_table_entry *entry;
  xtensa_relax_info *relax_info;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    return FALSE;

  entry = elf_xtensa_find_property_entry (prop_table, ptblsize,
					  sec->vma + rel->r_rel.target_offset);

  /* Mark the unused literal so that it will be removed.  */
  add_removed_literal (&relax_info->removed_list, &rel->r_rel, NULL);

  text_action_add (&relax_info->action_list,
		   ta_remove_literal, sec, rel->r_rel.target_offset, 4);

  /* If the section is 4-byte aligned, do not add fill.  */
  if (sec->alignment_power > 2) 
    {
      int fill_extra_space;
      bfd_vma entry_sec_offset;
      text_action *fa;
      property_table_entry *the_add_entry;
      int removed_diff;

      if (entry)
	entry_sec_offset = entry->address - sec->vma + entry->size;
      else
	entry_sec_offset = rel->r_rel.target_offset + 4;

      /* If the literal range is at the end of the section,
	 do not add fill.  */
      the_add_entry = elf_xtensa_find_property_entry (prop_table, ptblsize,
						      entry_sec_offset);
      fill_extra_space = compute_fill_extra_space (the_add_entry);

      fa = find_fill_action (&relax_info->action_list, sec, entry_sec_offset);
      removed_diff = compute_removed_action_diff (fa, sec, entry_sec_offset,
						  -4, fill_extra_space);
      if (fa)
	adjust_fill_action (fa, removed_diff);
      else
	text_action_add (&relax_info->action_list,
			 ta_fill, sec, entry_sec_offset, removed_diff);
    }

  /* Zero out the relocation on this literal location.  */
  if (irel)
    {
      if (elf_hash_table (link_info)->dynamic_sections_created)
	shrink_dynamic_reloc_sections (link_info, abfd, sec, irel);

      irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
      pin_internal_relocs (sec, internal_relocs);
    }

  /* Do not modify "last_loc_is_prev".  */
  return TRUE;
}


bfd_boolean 
identify_literal_placement (bfd *abfd,
			    asection *sec,
			    bfd_byte *contents,
			    struct bfd_link_info *link_info,
			    value_map_hash_table *values,
			    bfd_boolean *last_loc_is_prev_p,
			    Elf_Internal_Rela *irel,
			    int remaining_src_rels,
			    source_reloc *rel,
			    property_table_entry *prop_table,
			    int ptblsize,
			    section_cache_t *target_sec_cache,
			    bfd_boolean is_abs_literal)
{
  literal_value val;
  value_map *val_map;
  xtensa_relax_info *relax_info;
  bfd_boolean literal_placed = FALSE;
  r_reloc r_rel;
  unsigned long value;
  bfd_boolean final_static_link;
  bfd_size_type sec_size;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    return FALSE;

  sec_size = bfd_get_section_limit (abfd, sec);

  final_static_link =
    (!link_info->relocatable
     && !elf_hash_table (link_info)->dynamic_sections_created);

  /* The placement algorithm first checks to see if the literal is
     already in the value map.  If so and the value map is reachable
     from all uses, then the literal is moved to that location.  If
     not, then we identify the last location where a fresh literal was
     placed.  If the literal can be safely moved there, then we do so.
     If not, then we assume that the literal is not to move and leave
     the literal where it is, marking it as the last literal
     location.  */

  /* Find the literal value.  */
  value = 0;
  r_reloc_init (&r_rel, abfd, irel, contents, sec_size);
  if (!irel)
    {
      BFD_ASSERT (rel->r_rel.target_offset < sec_size);
      value = bfd_get_32 (abfd, contents + rel->r_rel.target_offset);
    }
  init_literal_value (&val, &r_rel, value, is_abs_literal);

  /* Check if we've seen another literal with the same value that
     is in the same output section.  */
  val_map = value_map_get_cached_value (values, &val, final_static_link);

  if (val_map
      && (r_reloc_get_section (&val_map->loc)->output_section
	  == sec->output_section)
      && relocations_reach (rel, remaining_src_rels, &val_map->loc)
      && coalesce_shared_literal (sec, rel, prop_table, ptblsize, val_map))
    {
      /* No change to last_loc_is_prev.  */
      literal_placed = TRUE;
    }

  /* For relocatable links, do not try to move literals.  To do it
     correctly might increase the number of relocations in an input
     section making the default relocatable linking fail.  */
  if (!link_info->relocatable && !literal_placed 
      && values->has_last_loc && !(*last_loc_is_prev_p))
    {
      asection *target_sec = r_reloc_get_section (&values->last_loc);
      if (target_sec && target_sec->output_section == sec->output_section)
	{
	  /* Increment the virtual offset.  */
	  r_reloc try_loc = values->last_loc;
	  try_loc.virtual_offset += 4;

	  /* There is a last loc that was in the same output section.  */
	  if (relocations_reach (rel, remaining_src_rels, &try_loc)
	      && move_shared_literal (sec, link_info, rel,
				      prop_table, ptblsize, 
				      &try_loc, &val, target_sec_cache))
	    {
	      values->last_loc.virtual_offset += 4;
	      literal_placed = TRUE;
	      if (!val_map)
		val_map = add_value_map (values, &val, &try_loc,
					 final_static_link);
	      else
		val_map->loc = try_loc;
	    }
	}
    }

  if (!literal_placed)
    {
      /* Nothing worked, leave the literal alone but update the last loc.  */
      values->has_last_loc = TRUE;
      values->last_loc = rel->r_rel;
      if (!val_map)
	val_map = add_value_map (values, &val, &rel->r_rel, final_static_link);
      else
	val_map->loc = rel->r_rel;
      *last_loc_is_prev_p = TRUE;
    }

  return TRUE;
}


/* Check if the original relocations (presumably on L32R instructions)
   identified by reloc[0..N] can be changed to reference the literal
   identified by r_rel.  If r_rel is out of range for any of the
   original relocations, then we don't want to coalesce the original
   literal with the one at r_rel.  We only check reloc[0..N], where the
   offsets are all the same as for reloc[0] (i.e., they're all
   referencing the same literal) and where N is also bounded by the
   number of remaining entries in the "reloc" array.  The "reloc" array
   is sorted by target offset so we know all the entries for the same
   literal will be contiguous.  */

static bfd_boolean
relocations_reach (source_reloc *reloc,
		   int remaining_relocs,
		   const r_reloc *r_rel)
{
  bfd_vma from_offset, source_address, dest_address;
  asection *sec;
  int i;

  if (!r_reloc_is_defined (r_rel))
    return FALSE;

  sec = r_reloc_get_section (r_rel);
  from_offset = reloc[0].r_rel.target_offset;

  for (i = 0; i < remaining_relocs; i++)
    {
      if (reloc[i].r_rel.target_offset != from_offset)
	break;

      /* Ignore relocations that have been removed.  */
      if (reloc[i].is_null)
	continue;

      /* The original and new output section for these must be the same
         in order to coalesce.  */
      if (r_reloc_get_section (&reloc[i].r_rel)->output_section
	  != sec->output_section)
	return FALSE;

      /* Absolute literals in the same output section can always be
	 combined.  */
      if (reloc[i].is_abs_literal)
	continue;

      /* A literal with no PC-relative relocations can be moved anywhere.  */
      if (reloc[i].opnd != -1)
	{
	  /* Otherwise, check to see that it fits.  */
	  source_address = (reloc[i].source_sec->output_section->vma
			    + reloc[i].source_sec->output_offset
			    + reloc[i].r_rel.rela.r_offset);
	  dest_address = (sec->output_section->vma
			  + sec->output_offset
			  + r_rel->target_offset);

	  if (!pcrel_reloc_fits (reloc[i].opcode, reloc[i].opnd,
				 source_address, dest_address))
	    return FALSE;
	}
    }

  return TRUE;
}


/* Move a literal to another literal location because it is
   the same as the other literal value.  */

static bfd_boolean 
coalesce_shared_literal (asection *sec,
			 source_reloc *rel,
			 property_table_entry *prop_table,
			 int ptblsize,
			 value_map *val_map)
{
  property_table_entry *entry;
  text_action *fa;
  property_table_entry *the_add_entry;
  int removed_diff;
  xtensa_relax_info *relax_info;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    return FALSE;

  entry = elf_xtensa_find_property_entry
    (prop_table, ptblsize, sec->vma + rel->r_rel.target_offset);
  if (entry && (entry->flags & XTENSA_PROP_INSN_NO_TRANSFORM))
    return TRUE;

  /* Mark that the literal will be coalesced.  */
  add_removed_literal (&relax_info->removed_list, &rel->r_rel, &val_map->loc);

  text_action_add (&relax_info->action_list,
		   ta_remove_literal, sec, rel->r_rel.target_offset, 4);

  /* If the section is 4-byte aligned, do not add fill.  */
  if (sec->alignment_power > 2) 
    {
      int fill_extra_space;
      bfd_vma entry_sec_offset;

      if (entry)
	entry_sec_offset = entry->address - sec->vma + entry->size;
      else
	entry_sec_offset = rel->r_rel.target_offset + 4;

      /* If the literal range is at the end of the section,
	 do not add fill.  */
      fill_extra_space = 0;
      the_add_entry = elf_xtensa_find_property_entry (prop_table, ptblsize,
						      entry_sec_offset);
      if (the_add_entry && (the_add_entry->flags & XTENSA_PROP_UNREACHABLE))
	fill_extra_space = the_add_entry->size;

      fa = find_fill_action (&relax_info->action_list, sec, entry_sec_offset);
      removed_diff = compute_removed_action_diff (fa, sec, entry_sec_offset,
						  -4, fill_extra_space);
      if (fa)
	adjust_fill_action (fa, removed_diff);
      else
	text_action_add (&relax_info->action_list,
			 ta_fill, sec, entry_sec_offset, removed_diff);
    }

  return TRUE;
}


/* Move a literal to another location.  This may actually increase the
   total amount of space used because of alignments so we need to do
   this carefully.  Also, it may make a branch go out of range.  */

static bfd_boolean 
move_shared_literal (asection *sec,
		     struct bfd_link_info *link_info,
		     source_reloc *rel,
		     property_table_entry *prop_table,
		     int ptblsize,
		     const r_reloc *target_loc,
		     const literal_value *lit_value,
		     section_cache_t *target_sec_cache)
{
  property_table_entry *the_add_entry, *src_entry, *target_entry = NULL;
  text_action *fa, *target_fa;
  int removed_diff;
  xtensa_relax_info *relax_info, *target_relax_info;
  asection *target_sec;
  ebb_t *ebb;
  ebb_constraint ebb_table;
  bfd_boolean relocs_fit;

  /* If this routine always returns FALSE, the literals that cannot be
     coalesced will not be moved.  */
  if (elf32xtensa_no_literal_movement)
    return FALSE;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    return FALSE;

  target_sec = r_reloc_get_section (target_loc);
  target_relax_info = get_xtensa_relax_info (target_sec);

  /* Literals to undefined sections may not be moved because they
     must report an error.  */
  if (bfd_is_und_section (target_sec))
    return FALSE;

  src_entry = elf_xtensa_find_property_entry
    (prop_table, ptblsize, sec->vma + rel->r_rel.target_offset);

  if (!section_cache_section (target_sec_cache, target_sec, link_info))
    return FALSE;

  target_entry = elf_xtensa_find_property_entry
    (target_sec_cache->ptbl, target_sec_cache->pte_count, 
     target_sec->vma + target_loc->target_offset);

  if (!target_entry)
    return FALSE;

  /* Make sure that we have not broken any branches.  */
  relocs_fit = FALSE;

  init_ebb_constraint (&ebb_table);
  ebb = &ebb_table.ebb;
  init_ebb (ebb, target_sec_cache->sec, target_sec_cache->contents, 
	    target_sec_cache->content_length,
	    target_sec_cache->ptbl, target_sec_cache->pte_count,
	    target_sec_cache->relocs, target_sec_cache->reloc_count);

  /* Propose to add 4 bytes + worst-case alignment size increase to
     destination.  */
  ebb_propose_action (&ebb_table, EBB_NO_ALIGN, 0,
		      ta_fill, target_loc->target_offset,
		      -4 - (1 << target_sec->alignment_power), TRUE);

  /* Check all of the PC-relative relocations to make sure they still fit.  */
  relocs_fit = check_section_ebb_pcrels_fit (target_sec->owner, target_sec, 
					     target_sec_cache->contents,
					     target_sec_cache->relocs,
					     &ebb_table, NULL);

  if (!relocs_fit) 
    return FALSE;

  text_action_add_literal (&target_relax_info->action_list,
			   ta_add_literal, target_loc, lit_value, -4);

  if (target_sec->alignment_power > 2 && target_entry != src_entry) 
    {
      /* May need to add or remove some fill to maintain alignment.  */
      int fill_extra_space;
      bfd_vma entry_sec_offset;

      entry_sec_offset = 
	target_entry->address - target_sec->vma + target_entry->size;

      /* If the literal range is at the end of the section,
	 do not add fill.  */
      fill_extra_space = 0;
      the_add_entry =
	elf_xtensa_find_property_entry (target_sec_cache->ptbl,
					target_sec_cache->pte_count,
					entry_sec_offset);
      if (the_add_entry && (the_add_entry->flags & XTENSA_PROP_UNREACHABLE))
	fill_extra_space = the_add_entry->size;

      target_fa = find_fill_action (&target_relax_info->action_list,
				    target_sec, entry_sec_offset);
      removed_diff = compute_removed_action_diff (target_fa, target_sec,
						  entry_sec_offset, 4,
						  fill_extra_space);
      if (target_fa)
	adjust_fill_action (target_fa, removed_diff);
      else
	text_action_add (&target_relax_info->action_list,
			 ta_fill, target_sec, entry_sec_offset, removed_diff);
    }

  /* Mark that the literal will be moved to the new location.  */
  add_removed_literal (&relax_info->removed_list, &rel->r_rel, target_loc);

  /* Remove the literal.  */
  text_action_add (&relax_info->action_list,
		   ta_remove_literal, sec, rel->r_rel.target_offset, 4);

  /* If the section is 4-byte aligned, do not add fill.  */
  if (sec->alignment_power > 2 && target_entry != src_entry) 
    {
      int fill_extra_space;
      bfd_vma entry_sec_offset;

      if (src_entry)
	entry_sec_offset = src_entry->address - sec->vma + src_entry->size;
      else
	entry_sec_offset = rel->r_rel.target_offset+4;

      /* If the literal range is at the end of the section,
	 do not add fill.  */
      fill_extra_space = 0;
      the_add_entry = elf_xtensa_find_property_entry (prop_table, ptblsize,
						      entry_sec_offset);
      if (the_add_entry && (the_add_entry->flags & XTENSA_PROP_UNREACHABLE))
	fill_extra_space = the_add_entry->size;

      fa = find_fill_action (&relax_info->action_list, sec, entry_sec_offset);
      removed_diff = compute_removed_action_diff (fa, sec, entry_sec_offset,
						  -4, fill_extra_space);
      if (fa)
	adjust_fill_action (fa, removed_diff);
      else
	text_action_add (&relax_info->action_list,
			 ta_fill, sec, entry_sec_offset, removed_diff);
    }

  return TRUE;
}


/* Second relaxation pass.  */

/* Modify all of the relocations to point to the right spot, and if this
   is a relaxable section, delete the unwanted literals and fix the
   section size.  */

bfd_boolean
relax_section (bfd *abfd, asection *sec, struct bfd_link_info *link_info)
{
  Elf_Internal_Rela *internal_relocs;
  xtensa_relax_info *relax_info;
  bfd_byte *contents;
  bfd_boolean ok = TRUE;
  unsigned i;
  bfd_boolean rv = FALSE;
  bfd_boolean virtual_action;
  bfd_size_type sec_size;

  sec_size = bfd_get_section_limit (abfd, sec);
  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  /* First translate any of the fixes that have been added already.  */
  translate_section_fixes (sec);

  /* Handle property sections (e.g., literal tables) specially.  */
  if (xtensa_is_property_section (sec))
    {
      BFD_ASSERT (!relax_info->is_relaxable_literal_section);
      return relax_property_section (abfd, sec, link_info);
    }

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  if (internal_relocs)
    {
      for (i = 0; i < sec->reloc_count; i++)
	{
	  Elf_Internal_Rela *irel;
	  xtensa_relax_info *target_relax_info;
	  bfd_vma source_offset, old_source_offset;
	  r_reloc r_rel;
	  unsigned r_type;
	  asection *target_sec;

	  /* Locally change the source address.
	     Translate the target to the new target address.
	     If it points to this section and has been removed,
	     NULLify it.
	     Write it back.  */

	  irel = &internal_relocs[i];
	  source_offset = irel->r_offset;
	  old_source_offset = source_offset;

	  r_type = ELF32_R_TYPE (irel->r_info);
	  r_reloc_init (&r_rel, abfd, irel, contents,
			bfd_get_section_limit (abfd, sec));

	  /* If this section could have changed then we may need to
	     change the relocation's offset.  */

	  if (relax_info->is_relaxable_literal_section
	      || relax_info->is_relaxable_asm_section)
	    {
	      if (r_type != R_XTENSA_NONE
		  && find_removed_literal (&relax_info->removed_list,
					   irel->r_offset))
		{
		  /* Remove this relocation.  */
		  if (elf_hash_table (link_info)->dynamic_sections_created)
		    shrink_dynamic_reloc_sections (link_info, abfd, sec, irel);
		  irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
		  irel->r_offset = offset_with_removed_text
		    (&relax_info->action_list, irel->r_offset);
		  pin_internal_relocs (sec, internal_relocs);
		  continue;
		}

	      if (r_type == R_XTENSA_ASM_SIMPLIFY)
		{
		  text_action *action =
		    find_insn_action (&relax_info->action_list,
				      irel->r_offset);
		  if (action && (action->action == ta_convert_longcall
				 || action->action == ta_remove_longcall))
		    {
		      bfd_reloc_status_type retval;
		      char *error_message = NULL;

		      retval = contract_asm_expansion (contents, sec_size,
						       irel, &error_message);
		      if (retval != bfd_reloc_ok)
			{
			  (*link_info->callbacks->reloc_dangerous)
			    (link_info, error_message, abfd, sec,
			     irel->r_offset);
			  goto error_return;
			}
		      /* Update the action so that the code that moves
			 the contents will do the right thing.  */
		      if (action->action == ta_remove_longcall)
			action->action = ta_remove_insn;
		      else
			action->action = ta_none;
		      /* Refresh the info in the r_rel.  */
		      r_reloc_init (&r_rel, abfd, irel, contents, sec_size);
		      r_type = ELF32_R_TYPE (irel->r_info);
		    }
		}

	      source_offset = offset_with_removed_text
		(&relax_info->action_list, irel->r_offset);
	      irel->r_offset = source_offset;
	    }

	  /* If the target section could have changed then
	     we may need to change the relocation's target offset.  */

	  target_sec = r_reloc_get_section (&r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && (target_relax_info->is_relaxable_literal_section
		  || target_relax_info->is_relaxable_asm_section))
	    {
	      r_reloc new_reloc;
	      reloc_bfd_fix *fix;
	      bfd_vma addend_displacement;

	      translate_reloc (&r_rel, &new_reloc);

	      if (r_type == R_XTENSA_DIFF8
		  || r_type == R_XTENSA_DIFF16
		  || r_type == R_XTENSA_DIFF32)
		{
		  bfd_vma diff_value = 0, new_end_offset, diff_mask = 0;

		  if (bfd_get_section_limit (abfd, sec) < old_source_offset)
		    {
		      (*link_info->callbacks->reloc_dangerous)
			(link_info, _("invalid relocation address"),
			 abfd, sec, old_source_offset);
		      goto error_return;
		    }

		  switch (r_type)
		    {
		    case R_XTENSA_DIFF8:
		      diff_value =
			bfd_get_8 (abfd, &contents[old_source_offset]);
		      break;
		    case R_XTENSA_DIFF16:
		      diff_value =
			bfd_get_16 (abfd, &contents[old_source_offset]);
		      break;
		    case R_XTENSA_DIFF32:
		      diff_value =
			bfd_get_32 (abfd, &contents[old_source_offset]);
		      break;
		    }

		  new_end_offset = offset_with_removed_text
		    (&target_relax_info->action_list,
		     r_rel.target_offset + diff_value);
		  diff_value = new_end_offset - new_reloc.target_offset;

		  switch (r_type)
		    {
		    case R_XTENSA_DIFF8:
		      diff_mask = 0xff;
		      bfd_put_8 (abfd, diff_value,
				 &contents[old_source_offset]);
		      break;
		    case R_XTENSA_DIFF16:
		      diff_mask = 0xffff;
		      bfd_put_16 (abfd, diff_value,
				  &contents[old_source_offset]);
		      break;
		    case R_XTENSA_DIFF32:
		      diff_mask = 0xffffffff;
		      bfd_put_32 (abfd, diff_value,
				  &contents[old_source_offset]);
		      break;
		    }

		  /* Check for overflow.  */
		  if ((diff_value & ~diff_mask) != 0)
		    {
		      (*link_info->callbacks->reloc_dangerous)
			(link_info, _("overflow after relaxation"),
			 abfd, sec, old_source_offset);
		      goto error_return;
		    }

		  pin_contents (sec, contents);
		}

	      /* FIXME: If the relocation still references a section in
		 the same input file, the relocation should be modified
		 directly instead of adding a "fix" record.  */

	      addend_displacement =
		new_reloc.target_offset + new_reloc.virtual_offset;

	      fix = reloc_bfd_fix_init (sec, source_offset, r_type, 0,
					r_reloc_get_section (&new_reloc),
					addend_displacement, TRUE);
	      add_fix (sec, fix);
	    }

	  pin_internal_relocs (sec, internal_relocs);
	}
    }

  if ((relax_info->is_relaxable_literal_section
       || relax_info->is_relaxable_asm_section)
      && relax_info->action_list.head)
    {
      /* Walk through the planned actions and build up a table
	 of move, copy and fill records.  Use the move, copy and
	 fill records to perform the actions once.  */

      bfd_size_type size = sec->size;
      int removed = 0;
      bfd_size_type final_size, copy_size, orig_insn_size;
      bfd_byte *scratch = NULL;
      bfd_byte *dup_contents = NULL;
      bfd_size_type orig_size = size;
      bfd_vma orig_dot = 0;
      bfd_vma orig_dot_copied = 0; /* Byte copied already from
					    orig dot in physical memory.  */
      bfd_vma orig_dot_vo = 0; /* Virtual offset from orig_dot.  */
      bfd_vma dup_dot = 0;

      text_action *action = relax_info->action_list.head;

      final_size = sec->size;
      for (action = relax_info->action_list.head; action;
	   action = action->next)
	{
	  final_size -= action->removed_bytes;
	}

      scratch = (bfd_byte *) bfd_zmalloc (final_size);
      dup_contents = (bfd_byte *) bfd_zmalloc (final_size);

      /* The dot is the current fill location.  */
#if DEBUG
      print_action_list (stderr, &relax_info->action_list);
#endif

      for (action = relax_info->action_list.head; action;
	   action = action->next)
	{
	  virtual_action = FALSE;
	  if (action->offset > orig_dot)
	    {
	      orig_dot += orig_dot_copied;
	      orig_dot_copied = 0;
	      orig_dot_vo = 0;
	      /* Out of the virtual world.  */
	    }

	  if (action->offset > orig_dot)
	    {
	      copy_size = action->offset - orig_dot;
	      memmove (&dup_contents[dup_dot], &contents[orig_dot], copy_size);
	      orig_dot += copy_size;
	      dup_dot += copy_size;
	      BFD_ASSERT (action->offset == orig_dot);
	    }
	  else if (action->offset < orig_dot)
	    {
	      if (action->action == ta_fill
		  && action->offset - action->removed_bytes == orig_dot)
		{
		  /* This is OK because the fill only effects the dup_dot.  */
		}
	      else if (action->action == ta_add_literal)
		{
		  /* TBD.  Might need to handle this.  */
		}
	    }
	  if (action->offset == orig_dot)
	    {
	      if (action->virtual_offset > orig_dot_vo)
		{
		  if (orig_dot_vo == 0)
		    {
		      /* Need to copy virtual_offset bytes.  Probably four.  */
		      copy_size = action->virtual_offset - orig_dot_vo;
		      memmove (&dup_contents[dup_dot],
			       &contents[orig_dot], copy_size);
		      orig_dot_copied = copy_size;
		      dup_dot += copy_size;
		    }
		  virtual_action = TRUE;
		} 
	      else
		BFD_ASSERT (action->virtual_offset <= orig_dot_vo);
	    }
	  switch (action->action)
	    {
	    case ta_remove_literal:
	    case ta_remove_insn:
	      BFD_ASSERT (action->removed_bytes >= 0);
	      orig_dot += action->removed_bytes;
	      break;

	    case ta_narrow_insn:
	      orig_insn_size = 3;
	      copy_size = 2;
	      memmove (scratch, &contents[orig_dot], orig_insn_size);
	      BFD_ASSERT (action->removed_bytes == 1);
	      rv = narrow_instruction (scratch, final_size, 0, TRUE);
	      BFD_ASSERT (rv);
	      memmove (&dup_contents[dup_dot], scratch, copy_size);
	      orig_dot += orig_insn_size;
	      dup_dot += copy_size;
	      break;

	    case ta_fill:
	      if (action->removed_bytes >= 0)
		orig_dot += action->removed_bytes;
	      else
		{
		  /* Already zeroed in dup_contents.  Just bump the
		     counters.  */
		  dup_dot += (-action->removed_bytes);
		}
	      break;

	    case ta_none:
	      BFD_ASSERT (action->removed_bytes == 0);
	      break;

	    case ta_convert_longcall:
	    case ta_remove_longcall:
	      /* These will be removed or converted before we get here.  */
	      BFD_ASSERT (0);
	      break;

	    case ta_widen_insn:
	      orig_insn_size = 2;
	      copy_size = 3;
	      memmove (scratch, &contents[orig_dot], orig_insn_size);
	      BFD_ASSERT (action->removed_bytes == -1);
	      rv = widen_instruction (scratch, final_size, 0, TRUE);
	      BFD_ASSERT (rv);
	      memmove (&dup_contents[dup_dot], scratch, copy_size);
	      orig_dot += orig_insn_size;
	      dup_dot += copy_size;
	      break;

	    case ta_add_literal:
	      orig_insn_size = 0;
	      copy_size = 4;
	      BFD_ASSERT (action->removed_bytes == -4);
	      /* TBD -- place the literal value here and insert
		 into the table.  */
	      memset (&dup_contents[dup_dot], 0, 4);
	      pin_internal_relocs (sec, internal_relocs);
	      pin_contents (sec, contents);

	      if (!move_literal (abfd, link_info, sec, dup_dot, dup_contents,
				 relax_info, &internal_relocs, &action->value))
		goto error_return;

	      if (virtual_action) 
		orig_dot_vo += copy_size;

	      orig_dot += orig_insn_size;
	      dup_dot += copy_size;
	      break;

	    default:
	      /* Not implemented yet.  */
	      BFD_ASSERT (0);
	      break;
	    }

	  size -= action->removed_bytes;
	  removed += action->removed_bytes;
	  BFD_ASSERT (dup_dot <= final_size);
	  BFD_ASSERT (orig_dot <= orig_size);
	}

      orig_dot += orig_dot_copied;
      orig_dot_copied = 0;

      if (orig_dot != orig_size)
	{
	  copy_size = orig_size - orig_dot;
	  BFD_ASSERT (orig_size > orig_dot);
	  BFD_ASSERT (dup_dot + copy_size == final_size);
	  memmove (&dup_contents[dup_dot], &contents[orig_dot], copy_size);
	  orig_dot += copy_size;
	  dup_dot += copy_size;
	}
      BFD_ASSERT (orig_size == orig_dot);
      BFD_ASSERT (final_size == dup_dot);

      /* Move the dup_contents back.  */
      if (final_size > orig_size)
	{
	  /* Contents need to be reallocated.  Swap the dup_contents into
	     contents.  */
	  sec->contents = dup_contents;
	  free (contents);
	  contents = dup_contents;
	  pin_contents (sec, contents);
	}
      else
	{
	  BFD_ASSERT (final_size <= orig_size);
	  memset (contents, 0, orig_size);
	  memcpy (contents, dup_contents, final_size);
	  free (dup_contents);
	}
      free (scratch);
      pin_contents (sec, contents);

      sec->size = final_size;
    }

 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}


static bfd_boolean 
translate_section_fixes (asection *sec)
{
  xtensa_relax_info *relax_info;
  reloc_bfd_fix *r;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    return TRUE;

  for (r = relax_info->fix_list; r != NULL; r = r->next)
    if (!translate_reloc_bfd_fix (r))
      return FALSE;

  return TRUE;
}


/* Translate a fix given the mapping in the relax info for the target
   section.  If it has already been translated, no work is required.  */

static bfd_boolean 
translate_reloc_bfd_fix (reloc_bfd_fix *fix)
{
  reloc_bfd_fix new_fix;
  asection *sec;
  xtensa_relax_info *relax_info;
  removed_literal *removed;
  bfd_vma new_offset, target_offset;

  if (fix->translated)
    return TRUE;

  sec = fix->target_sec;
  target_offset = fix->target_offset;

  relax_info = get_xtensa_relax_info (sec);
  if (!relax_info)
    {
      fix->translated = TRUE;
      return TRUE;
    }

  new_fix = *fix;

  /* The fix does not need to be translated if the section cannot change.  */
  if (!relax_info->is_relaxable_literal_section
      && !relax_info->is_relaxable_asm_section)
    {
      fix->translated = TRUE;
      return TRUE;
    }

  /* If the literal has been moved and this relocation was on an
     opcode, then the relocation should move to the new literal
     location.  Otherwise, the relocation should move within the
     section.  */

  removed = FALSE;
  if (is_operand_relocation (fix->src_type))
    {
      /* Check if the original relocation is against a literal being
	 removed.  */
      removed = find_removed_literal (&relax_info->removed_list,
				      target_offset);
    }

  if (removed) 
    {
      asection *new_sec;

      /* The fact that there is still a relocation to this literal indicates
	 that the literal is being coalesced, not simply removed.  */
      BFD_ASSERT (removed->to.abfd != NULL);

      /* This was moved to some other address (possibly another section).  */
      new_sec = r_reloc_get_section (&removed->to);
      if (new_sec != sec) 
	{
	  sec = new_sec;
	  relax_info = get_xtensa_relax_info (sec);
	  if (!relax_info || 
	      (!relax_info->is_relaxable_literal_section
	       && !relax_info->is_relaxable_asm_section))
	    {
	      target_offset = removed->to.target_offset;
	      new_fix.target_sec = new_sec;
	      new_fix.target_offset = target_offset;
	      new_fix.translated = TRUE;
	      *fix = new_fix;
	      return TRUE;
	    }
	}
      target_offset = removed->to.target_offset;
      new_fix.target_sec = new_sec;
    }

  /* The target address may have been moved within its section.  */
  new_offset = offset_with_removed_text (&relax_info->action_list,
					 target_offset);

  new_fix.target_offset = new_offset;
  new_fix.target_offset = new_offset;
  new_fix.translated = TRUE;
  *fix = new_fix;
  return TRUE;
}


/* Fix up a relocation to take account of removed literals.  */

static void
translate_reloc (const r_reloc *orig_rel, r_reloc *new_rel)
{
  asection *sec;
  xtensa_relax_info *relax_info;
  removed_literal *removed;
  bfd_vma new_offset, target_offset, removed_bytes;

  *new_rel = *orig_rel;

  if (!r_reloc_is_defined (orig_rel))
    return;
  sec = r_reloc_get_section (orig_rel);

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  if (!relax_info->is_relaxable_literal_section
      && !relax_info->is_relaxable_asm_section)
    return;

  target_offset = orig_rel->target_offset;

  removed = FALSE;
  if (is_operand_relocation (ELF32_R_TYPE (orig_rel->rela.r_info)))
    {
      /* Check if the original relocation is against a literal being
	 removed.  */
      removed = find_removed_literal (&relax_info->removed_list,
				      target_offset);
    }
  if (removed && removed->to.abfd)
    {
      asection *new_sec;

      /* The fact that there is still a relocation to this literal indicates
	 that the literal is being coalesced, not simply removed.  */
      BFD_ASSERT (removed->to.abfd != NULL);

      /* This was moved to some other address
	 (possibly in another section).  */
      *new_rel = removed->to;
      new_sec = r_reloc_get_section (new_rel);
      if (new_sec != sec)
	{
	  sec = new_sec;
	  relax_info = get_xtensa_relax_info (sec);
	  if (!relax_info
	      || (!relax_info->is_relaxable_literal_section
		  && !relax_info->is_relaxable_asm_section))
	    return;
	}
      target_offset = new_rel->target_offset;
    }

  /* ...and the target address may have been moved within its section.  */
  new_offset = offset_with_removed_text (&relax_info->action_list,
					 target_offset);

  /* Modify the offset and addend.  */
  removed_bytes = target_offset - new_offset;
  new_rel->target_offset = new_offset;
  new_rel->rela.r_addend -= removed_bytes;
}


/* For dynamic links, there may be a dynamic relocation for each
   literal.  The number of dynamic relocations must be computed in
   size_dynamic_sections, which occurs before relaxation.  When a
   literal is removed, this function checks if there is a corresponding
   dynamic relocation and shrinks the size of the appropriate dynamic
   relocation section accordingly.  At this point, the contents of the
   dynamic relocation sections have not yet been filled in, so there's
   nothing else that needs to be done.  */

static void
shrink_dynamic_reloc_sections (struct bfd_link_info *info,
			       bfd *abfd,
			       asection *input_section,
			       Elf_Internal_Rela *rel)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  unsigned long r_symndx;
  int r_type;
  struct elf_link_hash_entry *h;
  bfd_boolean dynamic_symbol;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  r_type = ELF32_R_TYPE (rel->r_info);
  r_symndx = ELF32_R_SYM (rel->r_info);

  if (r_symndx < symtab_hdr->sh_info)
    h = NULL;
  else
    h = sym_hashes[r_symndx - symtab_hdr->sh_info];

  dynamic_symbol = xtensa_elf_dynamic_symbol_p (h, info);

  if ((r_type == R_XTENSA_32 || r_type == R_XTENSA_PLT)
      && (input_section->flags & SEC_ALLOC) != 0
      && (dynamic_symbol || info->shared))
    {
      bfd *dynobj;
      const char *srel_name;
      asection *srel;
      bfd_boolean is_plt = FALSE;

      dynobj = elf_hash_table (info)->dynobj;
      BFD_ASSERT (dynobj != NULL);

      if (dynamic_symbol && r_type == R_XTENSA_PLT)
	{
	  srel_name = ".rela.plt";
	  is_plt = TRUE;
	}
      else
	srel_name = ".rela.got";

      /* Reduce size of the .rela.* section by one reloc.  */
      srel = bfd_get_section_by_name (dynobj, srel_name);
      BFD_ASSERT (srel != NULL);
      BFD_ASSERT (srel->size >= sizeof (Elf32_External_Rela));
      srel->size -= sizeof (Elf32_External_Rela);

      if (is_plt)
	{
	  asection *splt, *sgotplt, *srelgot;
	  int reloc_index, chunk;

	  /* Find the PLT reloc index of the entry being removed.  This
	     is computed from the size of ".rela.plt".  It is needed to
	     figure out which PLT chunk to resize.  Usually "last index
	     = size - 1" since the index starts at zero, but in this
	     context, the size has just been decremented so there's no
	     need to subtract one.  */
	  reloc_index = srel->size / sizeof (Elf32_External_Rela);

	  chunk = reloc_index / PLT_ENTRIES_PER_CHUNK;
	  splt = elf_xtensa_get_plt_section (dynobj, chunk);
	  sgotplt = elf_xtensa_get_gotplt_section (dynobj, chunk);
	  BFD_ASSERT (splt != NULL && sgotplt != NULL);

	  /* Check if an entire PLT chunk has just been eliminated.  */
	  if (reloc_index % PLT_ENTRIES_PER_CHUNK == 0)
	    {
	      /* The two magic GOT entries for that chunk can go away.  */
	      srelgot = bfd_get_section_by_name (dynobj, ".rela.got");
	      BFD_ASSERT (srelgot != NULL);
	      srelgot->reloc_count -= 2;
	      srelgot->size -= 2 * sizeof (Elf32_External_Rela);
	      sgotplt->size -= 8;

	      /* There should be only one entry left (and it will be
		 removed below).  */
	      BFD_ASSERT (sgotplt->size == 4);
	      BFD_ASSERT (splt->size == PLT_ENTRY_SIZE);
	    }

	  BFD_ASSERT (sgotplt->size >= 4);
	  BFD_ASSERT (splt->size >= PLT_ENTRY_SIZE);

	  sgotplt->size -= 4;
	  splt->size -= PLT_ENTRY_SIZE;
	}
    }
}


/* Take an r_rel and move it to another section.  This usually
   requires extending the interal_relocation array and pinning it.  If
   the original r_rel is from the same BFD, we can complete this here.
   Otherwise, we add a fix record to let the final link fix the
   appropriate address.  Contents and internal relocations for the
   section must be pinned after calling this routine.  */

static bfd_boolean
move_literal (bfd *abfd,
	      struct bfd_link_info *link_info,
	      asection *sec,
	      bfd_vma offset,
	      bfd_byte *contents,
	      xtensa_relax_info *relax_info,
	      Elf_Internal_Rela **internal_relocs_p,
	      const literal_value *lit)
{
  Elf_Internal_Rela *new_relocs = NULL;
  size_t new_relocs_count = 0;
  Elf_Internal_Rela this_rela;
  const r_reloc *r_rel;

  r_rel = &lit->r_rel;
  BFD_ASSERT (elf_section_data (sec)->relocs == *internal_relocs_p);

  if (r_reloc_is_const (r_rel))
    bfd_put_32 (abfd, lit->value, contents + offset);
  else
    {
      int r_type;
      unsigned i;
      asection *target_sec;
      reloc_bfd_fix *fix;
      unsigned insert_at;

      r_type = ELF32_R_TYPE (r_rel->rela.r_info);
      target_sec = r_reloc_get_section (r_rel);

      /* This is the difficult case.  We have to create a fix up.  */
      this_rela.r_offset = offset;
      this_rela.r_info = ELF32_R_INFO (0, r_type);
      this_rela.r_addend =
	r_rel->target_offset - r_reloc_get_target_offset (r_rel);
      bfd_put_32 (abfd, lit->value, contents + offset);

      /* Currently, we cannot move relocations during a relocatable link.  */
      BFD_ASSERT (!link_info->relocatable);
      fix = reloc_bfd_fix_init (sec, offset, r_type, r_rel->abfd,
				r_reloc_get_section (r_rel),
				r_rel->target_offset + r_rel->virtual_offset,
				FALSE);
      /* We also need to mark that relocations are needed here.  */
      sec->flags |= SEC_RELOC;

      translate_reloc_bfd_fix (fix);
      /* This fix has not yet been translated.  */
      add_fix (sec, fix);

      /* Add the relocation.  If we have already allocated our own
	 space for the relocations and we have room for more, then use
	 it.  Otherwise, allocate new space and move the literals.  */
      insert_at = sec->reloc_count;
      for (i = 0; i < sec->reloc_count; ++i)
	{
	  if (this_rela.r_offset < (*internal_relocs_p)[i].r_offset)
	    {
	      insert_at = i;
	      break;
	    }
	}

      if (*internal_relocs_p != relax_info->allocated_relocs
	  || sec->reloc_count + 1 > relax_info->allocated_relocs_count)
	{
	  BFD_ASSERT (relax_info->allocated_relocs == NULL
		      || sec->reloc_count == relax_info->relocs_count);

	  if (relax_info->allocated_relocs_count == 0) 
	    new_relocs_count = (sec->reloc_count + 2) * 2;
	  else
	    new_relocs_count = (relax_info->allocated_relocs_count + 2) * 2;

	  new_relocs = (Elf_Internal_Rela *)
	    bfd_zmalloc (sizeof (Elf_Internal_Rela) * (new_relocs_count));
	  if (!new_relocs)
	    return FALSE;

	  /* We could handle this more quickly by finding the split point.  */
	  if (insert_at != 0)
	    memcpy (new_relocs, *internal_relocs_p,
		    insert_at * sizeof (Elf_Internal_Rela));

	  new_relocs[insert_at] = this_rela;

	  if (insert_at != sec->reloc_count)
	    memcpy (new_relocs + insert_at + 1,
		    (*internal_relocs_p) + insert_at,
		    (sec->reloc_count - insert_at) 
		    * sizeof (Elf_Internal_Rela));

	  if (*internal_relocs_p != relax_info->allocated_relocs)
	    {
	      /* The first time we re-allocate, we can only free the
		 old relocs if they were allocated with bfd_malloc.
		 This is not true when keep_memory is in effect.  */
	      if (!link_info->keep_memory)
		free (*internal_relocs_p);
	    }
	  else
	    free (*internal_relocs_p);
	  relax_info->allocated_relocs = new_relocs;
	  relax_info->allocated_relocs_count = new_relocs_count;
	  elf_section_data (sec)->relocs = new_relocs;
	  sec->reloc_count++;
	  relax_info->relocs_count = sec->reloc_count;
	  *internal_relocs_p = new_relocs;
	}
      else
	{
	  if (insert_at != sec->reloc_count)
	    {
	      unsigned idx;
	      for (idx = sec->reloc_count; idx > insert_at; idx--)
		(*internal_relocs_p)[idx] = (*internal_relocs_p)[idx-1];
	    }
	  (*internal_relocs_p)[insert_at] = this_rela;
	  sec->reloc_count++;
	  if (relax_info->allocated_relocs)
	    relax_info->relocs_count = sec->reloc_count;
	}
    }
  return TRUE;
}


/* This is similar to relax_section except that when a target is moved,
   we shift addresses up.  We also need to modify the size.  This
   algorithm does NOT allow for relocations into the middle of the
   property sections.  */

static bfd_boolean
relax_property_section (bfd *abfd,
			asection *sec,
			struct bfd_link_info *link_info)
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  unsigned i, nexti;
  bfd_boolean ok = TRUE;
  bfd_boolean is_full_prop_section;
  size_t last_zfill_target_offset = 0;
  asection *last_zfill_target_sec = NULL;
  bfd_size_type sec_size;

  sec_size = bfd_get_section_limit (abfd, sec);
  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  is_full_prop_section =
    ((strcmp (sec->name, XTENSA_PROP_SEC_NAME) == 0)
     || (strncmp (sec->name, ".gnu.linkonce.prop.",
		  sizeof ".gnu.linkonce.prop." - 1) == 0));

  if (internal_relocs)
    {
      for (i = 0; i < sec->reloc_count; i++)
	{
	  Elf_Internal_Rela *irel;
	  xtensa_relax_info *target_relax_info;
	  unsigned r_type;
	  asection *target_sec;
	  literal_value val;
	  bfd_byte *size_p, *flags_p;

	  /* Locally change the source address.
	     Translate the target to the new target address.
	     If it points to this section and has been removed, MOVE IT.
	     Also, don't forget to modify the associated SIZE at
	     (offset + 4).  */

	  irel = &internal_relocs[i];
	  r_type = ELF32_R_TYPE (irel->r_info);
	  if (r_type == R_XTENSA_NONE)
	    continue;

	  /* Find the literal value.  */
	  r_reloc_init (&val.r_rel, abfd, irel, contents, sec_size);
	  size_p = &contents[irel->r_offset + 4];
	  flags_p = NULL;
	  if (is_full_prop_section)
	    {
	      flags_p = &contents[irel->r_offset + 8];
	      BFD_ASSERT (irel->r_offset + 12 <= sec_size);
	    }
	  else
	    BFD_ASSERT (irel->r_offset + 8 <= sec_size);

	  target_sec = r_reloc_get_section (&val.r_rel);
	  target_relax_info = get_xtensa_relax_info (target_sec);

	  if (target_relax_info
	      && (target_relax_info->is_relaxable_literal_section
		  || target_relax_info->is_relaxable_asm_section ))
	    {
	      /* Translate the relocation's destination.  */
	      bfd_vma new_offset, new_end_offset;
	      long old_size, new_size;

	      new_offset = offset_with_removed_text
		(&target_relax_info->action_list, val.r_rel.target_offset);

	      /* Assert that we are not out of bounds.  */
	      old_size = bfd_get_32 (abfd, size_p);

	      if (old_size == 0)
		{
		  /* Only the first zero-sized unreachable entry is
		     allowed to expand.  In this case the new offset
		     should be the offset before the fill and the new
		     size is the expansion size.  For other zero-sized
		     entries the resulting size should be zero with an
		     offset before or after the fill address depending
		     on whether the expanding unreachable entry
		     preceeds it.  */
		  if (last_zfill_target_sec
		      && last_zfill_target_sec == target_sec
		      && last_zfill_target_offset == val.r_rel.target_offset)
		    new_end_offset = new_offset;
		  else
		    {
		      new_end_offset = new_offset;
		      new_offset = offset_with_removed_text_before_fill
			(&target_relax_info->action_list,
			 val.r_rel.target_offset);

		      /* If it is not unreachable and we have not yet
			 seen an unreachable at this address, place it
			 before the fill address.  */
		      if (!flags_p
			  || (bfd_get_32 (abfd, flags_p)
			      & XTENSA_PROP_UNREACHABLE) == 0)
			new_end_offset = new_offset;
		      else
			{
			  last_zfill_target_sec = target_sec;
			  last_zfill_target_offset = val.r_rel.target_offset;
			}
		    }
		}
	      else
		{
		  new_end_offset = offset_with_removed_text_before_fill
		    (&target_relax_info->action_list,
		     val.r_rel.target_offset + old_size);
		}

	      new_size = new_end_offset - new_offset;

	      if (new_size != old_size)
		{
		  bfd_put_32 (abfd, new_size, size_p);
		  pin_contents (sec, contents);
		}

	      if (new_offset != val.r_rel.target_offset)
		{
		  bfd_vma diff = new_offset - val.r_rel.target_offset;
		  irel->r_addend += diff;
		  pin_internal_relocs (sec, internal_relocs);
		}
	    }
	}
    }

  /* Combine adjacent property table entries.  This is also done in
     finish_dynamic_sections() but at that point it's too late to
     reclaim the space in the output section, so we do this twice.  */

  if (internal_relocs && (!link_info->relocatable
			  || strcmp (sec->name, XTENSA_LIT_SEC_NAME) == 0))
    {
      Elf_Internal_Rela *last_irel = NULL;
      int removed_bytes = 0;
      bfd_vma offset, last_irel_offset;
      bfd_vma section_size;
      bfd_size_type entry_size;
      flagword predef_flags;

      if (is_full_prop_section)
	entry_size = 12;
      else
	entry_size = 8;

      predef_flags = xtensa_get_property_predef_flags (sec);

      /* Walk over memory and irels at the same time.
         This REQUIRES that the internal_relocs be sorted by offset.  */
      qsort (internal_relocs, sec->reloc_count, sizeof (Elf_Internal_Rela),
	     internal_reloc_compare);
      nexti = 0; /* Index into internal_relocs.  */

      pin_internal_relocs (sec, internal_relocs);
      pin_contents (sec, contents);

      last_irel_offset = (bfd_vma) -1;
      section_size = sec->size;
      BFD_ASSERT (section_size % entry_size == 0);

      for (offset = 0; offset < section_size; offset += entry_size)
	{
	  Elf_Internal_Rela *irel, *next_irel;
	  bfd_vma bytes_to_remove, size, actual_offset;
	  bfd_boolean remove_this_irel;
	  flagword flags;

	  irel = NULL;
	  next_irel = NULL;

	  /* Find the next two relocations (if there are that many left),
	     skipping over any R_XTENSA_NONE relocs.  On entry, "nexti" is
	     the starting reloc index.  After these two loops, "i"
	     is the index of the first non-NONE reloc past that starting
	     index, and "nexti" is the index for the next non-NONE reloc
	     after "i".  */

	  for (i = nexti; i < sec->reloc_count; i++)
	    {
	      if (ELF32_R_TYPE (internal_relocs[i].r_info) != R_XTENSA_NONE)
		{
		  irel = &internal_relocs[i];
		  break;
		}
	      internal_relocs[i].r_offset -= removed_bytes;
	    }

	  for (nexti = i + 1; nexti < sec->reloc_count; nexti++)
	    {
	      if (ELF32_R_TYPE (internal_relocs[nexti].r_info)
		  != R_XTENSA_NONE)
		{
		  next_irel = &internal_relocs[nexti];
		  break;
		}
	      internal_relocs[nexti].r_offset -= removed_bytes;
	    }

	  remove_this_irel = FALSE;
	  bytes_to_remove = 0;
	  actual_offset = offset - removed_bytes;
	  size = bfd_get_32 (abfd, &contents[actual_offset + 4]);

	  if (is_full_prop_section) 
	    flags = bfd_get_32 (abfd, &contents[actual_offset + 8]);
	  else
	    flags = predef_flags;

	  /* Check that the irels are sorted by offset,
	     with only one per address.  */
	  BFD_ASSERT (!irel || (int) irel->r_offset > (int) last_irel_offset); 
	  BFD_ASSERT (!next_irel || next_irel->r_offset > irel->r_offset);

	  /* Make sure there aren't relocs on the size or flag fields.  */
	  if ((irel && irel->r_offset == offset + 4)
	      || (is_full_prop_section 
		  && irel && irel->r_offset == offset + 8))
	    {
	      irel->r_offset -= removed_bytes;
	      last_irel_offset = irel->r_offset;
	    }
	  else if (next_irel && (next_irel->r_offset == offset + 4
				 || (is_full_prop_section 
				     && next_irel->r_offset == offset + 8)))
	    {
	      nexti += 1;
	      irel->r_offset -= removed_bytes;
	      next_irel->r_offset -= removed_bytes;
	      last_irel_offset = next_irel->r_offset;
	    }
	  else if (size == 0 && (flags & XTENSA_PROP_ALIGN) == 0
		   && (flags & XTENSA_PROP_UNREACHABLE) == 0)
	    {
	      /* Always remove entries with zero size and no alignment.  */
	      bytes_to_remove = entry_size;
	      if (irel && irel->r_offset == offset)
		{
		  remove_this_irel = TRUE;

		  irel->r_offset -= removed_bytes;
		  last_irel_offset = irel->r_offset;
		}
	    }
	  else if (irel && irel->r_offset == offset)
	    {
	      if (ELF32_R_TYPE (irel->r_info) == R_XTENSA_32)
		{
		  if (last_irel)
		    {
		      flagword old_flags;
		      bfd_vma old_size =
			bfd_get_32 (abfd, &contents[last_irel->r_offset + 4]);
		      bfd_vma old_address =
			(last_irel->r_addend
			 + bfd_get_32 (abfd, &contents[last_irel->r_offset]));
		      bfd_vma new_address =
			(irel->r_addend
			 + bfd_get_32 (abfd, &contents[actual_offset]));
		      if (is_full_prop_section) 
			old_flags = bfd_get_32
			  (abfd, &contents[last_irel->r_offset + 8]);
		      else
			old_flags = predef_flags;

		      if ((ELF32_R_SYM (irel->r_info)
			   == ELF32_R_SYM (last_irel->r_info))
			  && old_address + old_size == new_address
			  && old_flags == flags
			  && (old_flags & XTENSA_PROP_INSN_BRANCH_TARGET) == 0
			  && (old_flags & XTENSA_PROP_INSN_LOOP_TARGET) == 0)
			{
			  /* Fix the old size.  */
			  bfd_put_32 (abfd, old_size + size,
				      &contents[last_irel->r_offset + 4]);
			  bytes_to_remove = entry_size;
			  remove_this_irel = TRUE;
			}
		      else
			last_irel = irel;
		    }
		  else
		    last_irel = irel;
		}

	      irel->r_offset -= removed_bytes;
	      last_irel_offset = irel->r_offset;
	    }

	  if (remove_this_irel)
	    {
	      irel->r_info = ELF32_R_INFO (0, R_XTENSA_NONE);
	      irel->r_offset -= bytes_to_remove;
	    }

	  if (bytes_to_remove != 0)
	    {
	      removed_bytes += bytes_to_remove;
	      if (offset + bytes_to_remove < section_size)
		memmove (&contents[actual_offset],
			 &contents[actual_offset + bytes_to_remove],
			 section_size - offset - bytes_to_remove);
	    }
	}

      if (removed_bytes)
	{
	  /* Clear the removed bytes.  */
	  memset (&contents[section_size - removed_bytes], 0, removed_bytes);

	  sec->size = section_size - removed_bytes;

	  if (xtensa_is_littable_section (sec))
	    {
	      bfd *dynobj = elf_hash_table (link_info)->dynobj;
	      if (dynobj)
		{
		  asection *sgotloc =
		    bfd_get_section_by_name (dynobj, ".got.loc");
		  if (sgotloc)
		    sgotloc->size -= removed_bytes;
		}
	    }
	}
    }

 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}


/* Third relaxation pass.  */

/* Change symbol values to account for removed literals.  */

bfd_boolean
relax_section_symbols (bfd *abfd, asection *sec)
{
  xtensa_relax_info *relax_info;
  unsigned int sec_shndx;
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf;
  unsigned i, num_syms, num_locals;

  relax_info = get_xtensa_relax_info (sec);
  BFD_ASSERT (relax_info);

  if (!relax_info->is_relaxable_literal_section
      && !relax_info->is_relaxable_asm_section)
    return TRUE;

  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  isymbuf = retrieve_local_syms (abfd);

  num_syms = symtab_hdr->sh_size / sizeof (Elf32_External_Sym);
  num_locals = symtab_hdr->sh_info;

  /* Adjust the local symbols defined in this section.  */
  for (i = 0; i < num_locals; i++)
    {
      Elf_Internal_Sym *isym = &isymbuf[i];

      if (isym->st_shndx == sec_shndx)
	{
	  bfd_vma new_address = offset_with_removed_text
	    (&relax_info->action_list, isym->st_value);
	  bfd_vma new_size = isym->st_size;

	  if (ELF32_ST_TYPE (isym->st_info) == STT_FUNC)
	    {
	      bfd_vma new_end = offset_with_removed_text
		(&relax_info->action_list, isym->st_value + isym->st_size);
	      new_size = new_end - new_address;
	    }

	  isym->st_value = new_address;
	  isym->st_size = new_size;
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  for (i = 0; i < (num_syms - num_locals); i++)
    {
      struct elf_link_hash_entry *sym_hash;

      sym_hash = elf_sym_hashes (abfd)[i];

      if (sym_hash->root.type == bfd_link_hash_warning)
	sym_hash = (struct elf_link_hash_entry *) sym_hash->root.u.i.link;

      if ((sym_hash->root.type == bfd_link_hash_defined
	   || sym_hash->root.type == bfd_link_hash_defweak)
	  && sym_hash->root.u.def.section == sec)
	{
	  bfd_vma new_address = offset_with_removed_text
	    (&relax_info->action_list, sym_hash->root.u.def.value);
	  bfd_vma new_size = sym_hash->size;

	  if (sym_hash->type == STT_FUNC)
	    {
	      bfd_vma new_end = offset_with_removed_text
		(&relax_info->action_list,
		 sym_hash->root.u.def.value + sym_hash->size);
	      new_size = new_end - new_address;
	    }

	  sym_hash->root.u.def.value = new_address;
	  sym_hash->size = new_size;
	}
    }

  return TRUE;
}


/* "Fix" handling functions, called while performing relocations.  */

static bfd_boolean
do_fix_for_relocatable_link (Elf_Internal_Rela *rel,
			     bfd *input_bfd,
			     asection *input_section,
			     bfd_byte *contents)
{
  r_reloc r_rel;
  asection *sec, *old_sec;
  bfd_vma old_offset;
  int r_type = ELF32_R_TYPE (rel->r_info);
  reloc_bfd_fix *fix;

  if (r_type == R_XTENSA_NONE)
    return TRUE;

  fix = get_bfd_fix (input_section, rel->r_offset, r_type);
  if (!fix)
    return TRUE;

  r_reloc_init (&r_rel, input_bfd, rel, contents,
		bfd_get_section_limit (input_bfd, input_section));
  old_sec = r_reloc_get_section (&r_rel);
  old_offset = r_rel.target_offset;

  if (!old_sec || !r_reloc_is_defined (&r_rel))
    {
      if (r_type != R_XTENSA_ASM_EXPAND)
	{
	  (*_bfd_error_handler)
	    (_("%B(%A+0x%lx): unexpected fix for %s relocation"),
	     input_bfd, input_section, rel->r_offset,
	     elf_howto_table[r_type].name);
	  return FALSE;
	}
      /* Leave it be.  Resolution will happen in a later stage.  */
    }
  else
    {
      sec = fix->target_sec;
      rel->r_addend += ((sec->output_offset + fix->target_offset)
			- (old_sec->output_offset + old_offset));
    }
  return TRUE;
}


static void
do_fix_for_final_link (Elf_Internal_Rela *rel,
		       bfd *input_bfd,
		       asection *input_section,
		       bfd_byte *contents,
		       bfd_vma *relocationp)
{
  asection *sec;
  int r_type = ELF32_R_TYPE (rel->r_info);
  reloc_bfd_fix *fix;
  bfd_vma fixup_diff;

  if (r_type == R_XTENSA_NONE)
    return;

  fix = get_bfd_fix (input_section, rel->r_offset, r_type);
  if (!fix)
    return;

  sec = fix->target_sec;

  fixup_diff = rel->r_addend;
  if (elf_howto_table[fix->src_type].partial_inplace)
    {
      bfd_vma inplace_val;
      BFD_ASSERT (fix->src_offset
		  < bfd_get_section_limit (input_bfd, input_section));
      inplace_val = bfd_get_32 (input_bfd, &contents[fix->src_offset]);
      fixup_diff += inplace_val;
    }

  *relocationp = (sec->output_section->vma
		  + sec->output_offset
		  + fix->target_offset - fixup_diff);
}


/* Miscellaneous utility functions....  */

static asection *
elf_xtensa_get_plt_section (bfd *dynobj, int chunk)
{
  char plt_name[10];

  if (chunk == 0)
    return bfd_get_section_by_name (dynobj, ".plt");

  sprintf (plt_name, ".plt.%u", chunk);
  return bfd_get_section_by_name (dynobj, plt_name);
}


static asection *
elf_xtensa_get_gotplt_section (bfd *dynobj, int chunk)
{
  char got_name[14];

  if (chunk == 0)
    return bfd_get_section_by_name (dynobj, ".got.plt");

  sprintf (got_name, ".got.plt.%u", chunk);
  return bfd_get_section_by_name (dynobj, got_name);
}


/* Get the input section for a given symbol index.
   If the symbol is:
   . a section symbol, return the section;
   . a common symbol, return the common section;
   . an undefined symbol, return the undefined section;
   . an indirect symbol, follow the links;
   . an absolute value, return the absolute section.  */

static asection *
get_elf_r_symndx_section (bfd *abfd, unsigned long r_symndx)
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  asection *target_sec = NULL;
  if (r_symndx < symtab_hdr->sh_info)
    {
      Elf_Internal_Sym *isymbuf;
      unsigned int section_index;

      isymbuf = retrieve_local_syms (abfd);
      section_index = isymbuf[r_symndx].st_shndx;

      if (section_index == SHN_UNDEF)
	target_sec = bfd_und_section_ptr;
      else if (section_index > 0 && section_index < SHN_LORESERVE)
	target_sec = bfd_section_from_elf_index (abfd, section_index);
      else if (section_index == SHN_ABS)
	target_sec = bfd_abs_section_ptr;
      else if (section_index == SHN_COMMON)
	target_sec = bfd_com_section_ptr;
      else
	/* Who knows?  */
	target_sec = NULL;
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h = elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
        h = (struct elf_link_hash_entry *) h->root.u.i.link;

      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case  bfd_link_hash_defweak:
	  target_sec = h->root.u.def.section;
	  break;
	case bfd_link_hash_common:
	  target_sec = bfd_com_section_ptr;
	  break;
	case bfd_link_hash_undefined:
	case bfd_link_hash_undefweak:
	  target_sec = bfd_und_section_ptr;
	  break;
	default: /* New indirect warning.  */
	  target_sec = bfd_und_section_ptr;
	  break;
	}
    }
  return target_sec;
}


static struct elf_link_hash_entry *
get_elf_r_symndx_hash_entry (bfd *abfd, unsigned long r_symndx)
{
  unsigned long indx;
  struct elf_link_hash_entry *h;
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  if (r_symndx < symtab_hdr->sh_info)
    return NULL;

  indx = r_symndx - symtab_hdr->sh_info;
  h = elf_sym_hashes (abfd)[indx];
  while (h->root.type == bfd_link_hash_indirect
	 || h->root.type == bfd_link_hash_warning)
    h = (struct elf_link_hash_entry *) h->root.u.i.link;
  return h;
}


/* Get the section-relative offset for a symbol number.  */

static bfd_vma
get_elf_r_symndx_offset (bfd *abfd, unsigned long r_symndx)
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  bfd_vma offset = 0;

  if (r_symndx < symtab_hdr->sh_info)
    {
      Elf_Internal_Sym *isymbuf;
      isymbuf = retrieve_local_syms (abfd);
      offset = isymbuf[r_symndx].st_value;
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h =
	elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_defined
          || h->root.type == bfd_link_hash_defweak)
	offset = h->root.u.def.value;
    }
  return offset;
}


static bfd_boolean
is_reloc_sym_weak (bfd *abfd, Elf_Internal_Rela *rel)
{
  unsigned long r_symndx = ELF32_R_SYM (rel->r_info);
  struct elf_link_hash_entry *h;

  h = get_elf_r_symndx_hash_entry (abfd, r_symndx);
  if (h && h->root.type == bfd_link_hash_defweak)
    return TRUE;
  return FALSE;
}


static bfd_boolean
pcrel_reloc_fits (xtensa_opcode opc,
		  int opnd,
		  bfd_vma self_address,
		  bfd_vma dest_address)
{
  xtensa_isa isa = xtensa_default_isa;
  uint32 valp = dest_address;
  if (xtensa_operand_do_reloc (isa, opc, opnd, &valp, self_address)
      || xtensa_operand_encode (isa, opc, opnd, &valp))
    return FALSE;
  return TRUE;
}


static int linkonce_len = sizeof (".gnu.linkonce.") - 1;
static int insn_sec_len = sizeof (XTENSA_INSN_SEC_NAME) - 1;
static int lit_sec_len = sizeof (XTENSA_LIT_SEC_NAME) - 1;
static int prop_sec_len = sizeof (XTENSA_PROP_SEC_NAME) - 1;


static bfd_boolean 
xtensa_is_property_section (asection *sec)
{
  if (strncmp (XTENSA_INSN_SEC_NAME, sec->name, insn_sec_len) == 0
      || strncmp (XTENSA_LIT_SEC_NAME, sec->name, lit_sec_len) == 0
      || strncmp (XTENSA_PROP_SEC_NAME, sec->name, prop_sec_len) == 0)
    return TRUE;

  if (strncmp (".gnu.linkonce.", sec->name, linkonce_len) == 0
      && (strncmp (&sec->name[linkonce_len], "x.", 2) == 0
	  || strncmp (&sec->name[linkonce_len], "p.", 2) == 0
	  || strncmp (&sec->name[linkonce_len], "prop.", 5) == 0))
    return TRUE;

  return FALSE;
}


static bfd_boolean 
xtensa_is_littable_section (asection *sec)
{
  if (strncmp (XTENSA_LIT_SEC_NAME, sec->name, lit_sec_len) == 0)
    return TRUE;

  if (strncmp (".gnu.linkonce.", sec->name, linkonce_len) == 0
      && sec->name[linkonce_len] == 'p'
      && sec->name[linkonce_len + 1] == '.')
    return TRUE;

  return FALSE;
}


static int
internal_reloc_compare (const void *ap, const void *bp)
{
  const Elf_Internal_Rela *a = (const Elf_Internal_Rela *) ap;
  const Elf_Internal_Rela *b = (const Elf_Internal_Rela *) bp;

  if (a->r_offset != b->r_offset)
    return (a->r_offset - b->r_offset);

  /* We don't need to sort on these criteria for correctness,
     but enforcing a more strict ordering prevents unstable qsort
     from behaving differently with different implementations.
     Without the code below we get correct but different results
     on Solaris 2.7 and 2.8.  We would like to always produce the
     same results no matter the host.  */

  if (a->r_info != b->r_info)
    return (a->r_info - b->r_info);

  return (a->r_addend - b->r_addend);
}


static int
internal_reloc_matches (const void *ap, const void *bp)
{
  const Elf_Internal_Rela *a = (const Elf_Internal_Rela *) ap;
  const Elf_Internal_Rela *b = (const Elf_Internal_Rela *) bp;

  /* Check if one entry overlaps with the other; this shouldn't happen
     except when searching for a match.  */
  return (a->r_offset - b->r_offset);
}


char *
xtensa_get_property_section_name (asection *sec, const char *base_name)
{
  if (strncmp (sec->name, ".gnu.linkonce.", linkonce_len) == 0)
    {
      char *prop_sec_name;
      const char *suffix;
      char *linkonce_kind = 0;

      if (strcmp (base_name, XTENSA_INSN_SEC_NAME) == 0) 
	linkonce_kind = "x.";
      else if (strcmp (base_name, XTENSA_LIT_SEC_NAME) == 0) 
	linkonce_kind = "p.";
      else if (strcmp (base_name, XTENSA_PROP_SEC_NAME) == 0)
	linkonce_kind = "prop.";
      else
	abort ();

      prop_sec_name = (char *) bfd_malloc (strlen (sec->name)
					   + strlen (linkonce_kind) + 1);
      memcpy (prop_sec_name, ".gnu.linkonce.", linkonce_len);
      strcpy (prop_sec_name + linkonce_len, linkonce_kind);

      suffix = sec->name + linkonce_len;
      /* For backward compatibility, replace "t." instead of inserting
         the new linkonce_kind (but not for "prop" sections).  */
      if (strncmp (suffix, "t.", 2) == 0 && linkonce_kind[1] == '.')
        suffix += 2;
      strcat (prop_sec_name + linkonce_len, suffix);

      return prop_sec_name;
    }

  return strdup (base_name);
}


flagword
xtensa_get_property_predef_flags (asection *sec)
{
  if (strcmp (sec->name, XTENSA_INSN_SEC_NAME) == 0
      || strncmp (sec->name, ".gnu.linkonce.x.",
		  sizeof ".gnu.linkonce.x." - 1) == 0)
    return (XTENSA_PROP_INSN
	    | XTENSA_PROP_INSN_NO_TRANSFORM
	    | XTENSA_PROP_INSN_NO_REORDER);

  if (xtensa_is_littable_section (sec))
    return (XTENSA_PROP_LITERAL
	    | XTENSA_PROP_INSN_NO_TRANSFORM
	    | XTENSA_PROP_INSN_NO_REORDER);

  return 0;
}


/* Other functions called directly by the linker.  */

bfd_boolean
xtensa_callback_required_dependence (bfd *abfd,
				     asection *sec,
				     struct bfd_link_info *link_info,
				     deps_callback_t callback,
				     void *closure)
{
  Elf_Internal_Rela *internal_relocs;
  bfd_byte *contents;
  unsigned i;
  bfd_boolean ok = TRUE;
  bfd_size_type sec_size;

  sec_size = bfd_get_section_limit (abfd, sec);

  /* ".plt*" sections have no explicit relocations but they contain L32R
     instructions that reference the corresponding ".got.plt*" sections.  */
  if ((sec->flags & SEC_LINKER_CREATED) != 0
      && strncmp (sec->name, ".plt", 4) == 0)
    {
      asection *sgotplt;

      /* Find the corresponding ".got.plt*" section.  */
      if (sec->name[4] == '\0')
	sgotplt = bfd_get_section_by_name (sec->owner, ".got.plt");
      else
	{
	  char got_name[14];
	  int chunk = 0;

	  BFD_ASSERT (sec->name[4] == '.');
	  chunk = strtol (&sec->name[5], NULL, 10);

	  sprintf (got_name, ".got.plt.%u", chunk);
	  sgotplt = bfd_get_section_by_name (sec->owner, got_name);
	}
      BFD_ASSERT (sgotplt);

      /* Assume worst-case offsets: L32R at the very end of the ".plt"
	 section referencing a literal at the very beginning of
	 ".got.plt".  This is very close to the real dependence, anyway.  */
      (*callback) (sec, sec_size, sgotplt, 0, closure);
    }

  internal_relocs = retrieve_internal_relocs (abfd, sec, 
					      link_info->keep_memory);
  if (internal_relocs == NULL
      || sec->reloc_count == 0)
    return ok;

  /* Cache the contents for the duration of this scan.  */
  contents = retrieve_contents (abfd, sec, link_info->keep_memory);
  if (contents == NULL && sec_size != 0)
    {
      ok = FALSE;
      goto error_return;
    }

  if (!xtensa_default_isa)
    xtensa_default_isa = xtensa_isa_init (0, 0);

  for (i = 0; i < sec->reloc_count; i++)
    {
      Elf_Internal_Rela *irel = &internal_relocs[i];
      if (is_l32r_relocation (abfd, sec, contents, irel))
	{
	  r_reloc l32r_rel;
	  asection *target_sec;
	  bfd_vma target_offset;

	  r_reloc_init (&l32r_rel, abfd, irel, contents, sec_size);
	  target_sec = NULL;
	  target_offset = 0;
	  /* L32Rs must be local to the input file.  */
	  if (r_reloc_is_defined (&l32r_rel))
	    {
	      target_sec = r_reloc_get_section (&l32r_rel);
	      target_offset = l32r_rel.target_offset;
	    }
	  (*callback) (sec, irel->r_offset, target_sec, target_offset,
		       closure);
	}
    }

 error_return:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return ok;
}

/* The default literal sections should always be marked as "code" (i.e.,
   SHF_EXECINSTR).  This is particularly important for the Linux kernel
   module loader so that the literals are not placed after the text.  */
static const struct bfd_elf_special_section elf_xtensa_special_sections[] =
{
  { ".fini.literal", 13, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { ".init.literal", 13, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { ".literal",       8, 0, SHT_PROGBITS, SHF_ALLOC + SHF_EXECINSTR },
  { NULL,             0, 0, 0,            0 }
};

#ifndef ELF_ARCH
#define TARGET_LITTLE_SYM		bfd_elf32_xtensa_le_vec
#define TARGET_LITTLE_NAME		"elf32-xtensa-le"
#define TARGET_BIG_SYM			bfd_elf32_xtensa_be_vec
#define TARGET_BIG_NAME			"elf32-xtensa-be"
#define ELF_ARCH			bfd_arch_xtensa

#define ELF_MACHINE_CODE		EM_XTENSA
#define ELF_MACHINE_ALT1		EM_XTENSA_OLD

#if XCHAL_HAVE_MMU
#define ELF_MAXPAGESIZE			(1 << XCHAL_MMU_MIN_PTE_PAGE_SIZE)
#else /* !XCHAL_HAVE_MMU */
#define ELF_MAXPAGESIZE			1
#endif /* !XCHAL_HAVE_MMU */
#endif /* ELF_ARCH */

#define elf_backend_can_gc_sections	1
#define elf_backend_can_refcount	1
#define elf_backend_plt_readonly	1
#define elf_backend_got_header_size	4
#define elf_backend_want_dynbss		0
#define elf_backend_want_got_plt	1

#define elf_info_to_howto		     elf_xtensa_info_to_howto_rela

#define bfd_elf32_bfd_merge_private_bfd_data elf_xtensa_merge_private_bfd_data
#define bfd_elf32_new_section_hook	     elf_xtensa_new_section_hook
#define bfd_elf32_bfd_print_private_bfd_data elf_xtensa_print_private_bfd_data
#define bfd_elf32_bfd_relax_section	     elf_xtensa_relax_section
#define bfd_elf32_bfd_reloc_type_lookup	     elf_xtensa_reloc_type_lookup
#define bfd_elf32_bfd_set_private_flags	     elf_xtensa_set_private_flags

#define elf_backend_adjust_dynamic_symbol    elf_xtensa_adjust_dynamic_symbol
#define elf_backend_check_relocs	     elf_xtensa_check_relocs
#define elf_backend_create_dynamic_sections  elf_xtensa_create_dynamic_sections
#define elf_backend_discard_info	     elf_xtensa_discard_info
#define elf_backend_ignore_discarded_relocs  elf_xtensa_ignore_discarded_relocs
#define elf_backend_final_write_processing   elf_xtensa_final_write_processing
#define elf_backend_finish_dynamic_sections  elf_xtensa_finish_dynamic_sections
#define elf_backend_finish_dynamic_symbol    elf_xtensa_finish_dynamic_symbol
#define elf_backend_gc_mark_hook	     elf_xtensa_gc_mark_hook
#define elf_backend_gc_sweep_hook	     elf_xtensa_gc_sweep_hook
#define elf_backend_grok_prstatus	     elf_xtensa_grok_prstatus
#define elf_backend_grok_psinfo		     elf_xtensa_grok_psinfo
#define elf_backend_hide_symbol		     elf_xtensa_hide_symbol
#define elf_backend_modify_segment_map	     elf_xtensa_modify_segment_map
#define elf_backend_object_p		     elf_xtensa_object_p
#define elf_backend_reloc_type_class	     elf_xtensa_reloc_type_class
#define elf_backend_relocate_section	     elf_xtensa_relocate_section
#define elf_backend_size_dynamic_sections    elf_xtensa_size_dynamic_sections
#define elf_backend_special_sections	     elf_xtensa_special_sections

#include "elf32-target.h"
