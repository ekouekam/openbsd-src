/* BFD COFF object file private structure.
   Copyright (C) 1990, 1991, 1992, 1993 Free Software Foundation, Inc.
   Written by Cygnus Support.

This file is part of BFD, the Binary File Descriptor library.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	$Id: libcoff.h,v 1.1 1995/10/18 08:39:53 deraadt Exp $
*/


/* Object file tdata; access macros */

#define coff_data(bfd)		((bfd)->tdata.coff_obj_data)
#define exec_hdr(bfd)		(coff_data(bfd)->hdr)
#define obj_symbols(bfd)	(coff_data(bfd)->symbols)
#define	obj_sym_filepos(bfd)	(coff_data(bfd)->sym_filepos)

#define obj_relocbase(bfd)	(coff_data(bfd)->relocbase)
#define obj_raw_syments(bfd)	(coff_data(bfd)->raw_syments)
#define obj_raw_syment_count(bfd)	(coff_data(bfd)->raw_syment_count)
#define obj_convert(bfd)	(coff_data(bfd)->conversion_table)
#define obj_conv_table_size(bfd) (coff_data(bfd)->conv_table_size)
#if CFILE_STUFF
#define obj_symbol_slew(bfd)	(coff_data(bfd)->symbol_index_slew)
#else
#define obj_symbol_slew(bfd) 0
#endif


/* `Tdata' information kept for COFF files.  */

typedef struct coff_tdata
{
  struct   coff_symbol_struct *symbols;	/* symtab for input bfd */
  unsigned int *conversion_table;
  int conv_table_size;
  file_ptr sym_filepos;

  long symbol_index_slew;	/* used during read to mark whether a
				   C_FILE symbol as been added. */

  struct coff_ptr_struct *raw_syments;
  struct lineno *raw_linenos;
  unsigned int raw_syment_count;
  unsigned short flags;

  /* These are only valid once writing has begun */
  long int relocbase;

  /* These members communicate important constants about the symbol table
     to GDB's symbol-reading code.  These `constants' unfortunately vary
     from coff implementation to implementation...  */
  unsigned local_n_btmask;
  unsigned local_n_btshft;
  unsigned local_n_tmask;
  unsigned local_n_tshift;
  unsigned local_symesz;
  unsigned local_auxesz;
  unsigned local_linesz;
} coff_data_type;

/* We take the address of the first element of a asymbol to ensure that the
 * macro is only ever applied to an asymbol.  */
#define coffsymbol(asymbol) ((coff_symbol_type *)(&((asymbol)->the_bfd)))

/* Functions in coffgen.c.  */
extern bfd_target *coff_object_p PARAMS ((bfd *));
extern struct sec *coff_section_from_bfd_index PARAMS ((bfd *, int));
extern unsigned int coff_get_symtab_upper_bound PARAMS ((bfd *));
extern unsigned int coff_get_symtab PARAMS ((bfd *, asymbol **));
extern int coff_count_linenumbers PARAMS ((bfd *));
extern struct coff_symbol_struct *coff_symbol_from PARAMS ((bfd *, asymbol *));
extern void coff_renumber_symbols PARAMS ((bfd *));
extern void coff_mangle_symbols PARAMS ((bfd *));
extern void coff_write_symbols PARAMS ((bfd *));
extern void coff_write_linenumbers PARAMS ((bfd *));
extern alent *coff_get_lineno PARAMS ((bfd *, asymbol *));
extern asymbol *coff_section_symbol PARAMS ((bfd *, char *));
extern struct coff_ptr_struct *coff_get_normalized_symtab PARAMS ((bfd *));
extern unsigned int coff_get_reloc_upper_bound PARAMS ((bfd *, sec_ptr));
extern asymbol *coff_make_empty_symbol PARAMS ((bfd *));
extern void coff_print_symbol PARAMS ((bfd *, PTR filep, asymbol *,
				       bfd_print_symbol_type how));
extern void coff_get_symbol_info PARAMS ((bfd *, asymbol *,
					  symbol_info *ret));
extern asymbol *coff_bfd_make_debug_symbol PARAMS ((bfd *, PTR,
						    unsigned long));
extern boolean coff_find_nearest_line PARAMS ((bfd *,
					       asection *,
					       asymbol **,
					       bfd_vma offset,
					       CONST char **filename_ptr,
					       CONST char **functionname_ptr,
					       unsigned int *line_ptr));
extern int coff_sizeof_headers PARAMS ((bfd *, boolean reloc));
extern boolean bfd_coff_reloc16_relax_section PARAMS ((bfd *,
						       asection *,
						       asymbol **));
extern bfd_byte *bfd_coff_reloc16_get_relocated_section_contents
  PARAMS ((bfd *, struct bfd_seclet *, bfd_byte *, boolean relocateable));
extern bfd_vma bfd_coff_reloc16_get_value PARAMS ((arelent *,
						   struct bfd_seclet *));

/* And more taken from the source .. */

typedef struct coff_ptr_struct 
{

        /* Remembers the offset from the first symbol in the file for
          this symbol. Generated by coff_renumber_symbols. */
unsigned int offset;

        /* Should the tag field of this symbol be renumbered.
          Created by coff_pointerize_aux. */
char fix_tag;

        /* Should the endidx field of this symbol be renumbered.
          Created by coff_pointerize_aux. */
char fix_end;

        /* The container for the symbol structure as read and translated
           from the file. */

union {
   union internal_auxent auxent;
   struct internal_syment syment;
 } u;
} combined_entry_type;


 /* Each canonical asymbol really looks like this: */

typedef struct coff_symbol_struct
{
    /* The actual symbol which the rest of BFD works with */
asymbol symbol;

    /* A pointer to the hidden information for this symbol */
combined_entry_type *native;

    /* A pointer to the linenumber information for this symbol */
struct lineno_cache_entry *lineno;

    /* Have the line numbers been relocated yet ? */
boolean done_lineno;
} coff_symbol_type;
typedef struct 
{
  void (*_bfd_coff_swap_aux_in) PARAMS ((
       bfd            *abfd ,
       PTR             ext,
       int             type,
       int             class ,
       PTR             in));

  void (*_bfd_coff_swap_sym_in) PARAMS ((
       bfd            *abfd ,
       PTR             ext,
       PTR             in));

  void (*_bfd_coff_swap_lineno_in) PARAMS ((
       bfd            *abfd,
       PTR            ext,
       PTR             in));

 unsigned int (*_bfd_coff_swap_aux_out) PARAMS ((
       bfd   	*abfd,
       PTR	in,
       int    	type,
       int    	class,
       PTR    	ext));

 unsigned int (*_bfd_coff_swap_sym_out) PARAMS ((
      bfd      *abfd,
      PTR	in,
      PTR	ext));

 unsigned int (*_bfd_coff_swap_lineno_out) PARAMS ((
      	bfd   	*abfd,
      	PTR	in,
	PTR	ext));

 unsigned int (*_bfd_coff_swap_reloc_out) PARAMS ((
      	bfd     *abfd,
     	PTR	src,
	PTR	dst));

 unsigned int (*_bfd_coff_swap_filehdr_out) PARAMS ((
      	bfd  	*abfd,
	PTR 	in,
	PTR 	out));

 unsigned int (*_bfd_coff_swap_aouthdr_out) PARAMS ((
      	bfd 	*abfd,
	PTR 	in,
	PTR	out));

 unsigned int (*_bfd_coff_swap_scnhdr_out) PARAMS ((
      	bfd  	*abfd,
      	PTR	in,
	PTR	out));

 unsigned int _bfd_filhsz;
 unsigned int _bfd_aoutsz;
 unsigned int _bfd_scnhsz;
 unsigned int _bfd_symesz;
 unsigned int _bfd_auxesz;
 unsigned int _bfd_linesz;
 boolean _bfd_coff_long_filenames;
 void (*_bfd_coff_swap_filehdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 void (*_bfd_coff_swap_aouthdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 void (*_bfd_coff_swap_scnhdr_in) PARAMS ((
       bfd     *abfd,
       PTR     ext,
       PTR     in));
 boolean (*_bfd_coff_bad_format_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr));
 boolean (*_bfd_coff_set_arch_mach_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr));
 PTR (*_bfd_coff_mkobject_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_filehdr,
       PTR     internal_aouthdr));
 flagword (*_bfd_styp_to_sec_flags_hook) PARAMS ((
       bfd     *abfd,
       PTR     internal_scnhdr));
 asection *(*_bfd_make_section_hook) PARAMS ((
       bfd     *abfd,
       char    *name));
 void (*_bfd_set_alignment_hook) PARAMS ((
       bfd     *abfd,
       asection *sec,
       PTR     internal_scnhdr));
 boolean (*_bfd_coff_slurp_symbol_table) PARAMS ((
       bfd     *abfd));
 boolean (*_bfd_coff_symname_in_debug) PARAMS ((
       bfd     *abfd,
       struct internal_syment *sym));
 void (*_bfd_coff_reloc16_extra_cases) PARAMS ((
       bfd     *abfd,
       struct bfd_seclet *seclet,
       arelent *reloc,
       bfd_byte *data,
       unsigned int *src_ptr,
       unsigned int *dst_ptr));
 int (*_bfd_coff_reloc16_estimate) PARAMS ((
       asection *input_section,
       asymbol **symbols,
       arelent *r,
       unsigned int shrink));	

} bfd_coff_backend_data;

#define coff_backend_info(abfd) ((bfd_coff_backend_data *) (abfd)->xvec->backend_data)

#define bfd_coff_swap_aux_in(a,e,t,c,i) \
        ((coff_backend_info (a)->_bfd_coff_swap_aux_in) (a,e,t,c,i))

#define bfd_coff_swap_sym_in(a,e,i) \
        ((coff_backend_info (a)->_bfd_coff_swap_sym_in) (a,e,i))

#define bfd_coff_swap_lineno_in(a,e,i) \
        ((coff_backend_info ( a)->_bfd_coff_swap_lineno_in) (a,e,i))

#define bfd_coff_swap_reloc_out(abfd, i, o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_reloc_out) (abfd, i, o))

#define bfd_coff_swap_lineno_out(abfd, i, o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_lineno_out) (abfd, i, o))

#define bfd_coff_swap_aux_out(abfd, i, t,c,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_aux_out) (abfd, i,t,c, o))

#define bfd_coff_swap_sym_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_sym_out) (abfd, i, o))

#define bfd_coff_swap_scnhdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_out) (abfd, i, o))

#define bfd_coff_swap_filehdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_out) (abfd, i, o))

#define bfd_coff_swap_aouthdr_out(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_out) (abfd, i, o))

#define bfd_coff_filhsz(abfd) (coff_backend_info (abfd)->_bfd_filhsz)
#define bfd_coff_aoutsz(abfd) (coff_backend_info (abfd)->_bfd_aoutsz)
#define bfd_coff_scnhsz(abfd) (coff_backend_info (abfd)->_bfd_scnhsz)
#define bfd_coff_symesz(abfd) (coff_backend_info (abfd)->_bfd_symesz)
#define bfd_coff_auxesz(abfd) (coff_backend_info (abfd)->_bfd_auxesz)
#define bfd_coff_linesz(abfd) (coff_backend_info (abfd)->_bfd_linesz)
#define bfd_coff_long_filenames(abfd) (coff_backend_info (abfd)->_bfd_coff_long_filenames)
#define bfd_coff_swap_filehdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_filehdr_in) (abfd, i, o))

#define bfd_coff_swap_aouthdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_aouthdr_in) (abfd, i, o))

#define bfd_coff_swap_scnhdr_in(abfd, i,o) \
        ((coff_backend_info (abfd)->_bfd_coff_swap_scnhdr_in) (abfd, i, o))

#define bfd_coff_bad_format_hook(abfd, filehdr) \
        ((coff_backend_info (abfd)->_bfd_coff_bad_format_hook) (abfd, filehdr))

#define bfd_coff_set_arch_mach_hook(abfd, filehdr)\
        ((coff_backend_info (abfd)->_bfd_coff_set_arch_mach_hook) (abfd, filehdr))
#define bfd_coff_mkobject_hook(abfd, filehdr, aouthdr)\
        ((coff_backend_info (abfd)->_bfd_coff_mkobject_hook) (abfd, filehdr, aouthdr))

#define bfd_coff_styp_to_sec_flags_hook(abfd, scnhdr)\
        ((coff_backend_info (abfd)->_bfd_styp_to_sec_flags_hook) (abfd, scnhdr))

#define bfd_coff_make_section_hook(abfd, name)\
        ((coff_backend_info (abfd)->_bfd_make_section_hook) (abfd, name))

#define bfd_coff_set_alignment_hook(abfd, sec, scnhdr)\
        ((coff_backend_info (abfd)->_bfd_set_alignment_hook) (abfd, sec, scnhdr))

#define bfd_coff_slurp_symbol_table(abfd)\
        ((coff_backend_info (abfd)->_bfd_coff_slurp_symbol_table) (abfd))

#define bfd_coff_symname_in_debug(abfd, sym)\
        ((coff_backend_info (abfd)->_bfd_coff_symname_in_debug) (abfd, sym))

#define bfd_coff_reloc16_extra_cases(abfd, seclet, reloc, data, src_ptr, dst_ptr)\
        ((coff_backend_info (abfd)->_bfd_coff_reloc16_extra_cases)\
         (abfd, seclet, reloc, data, src_ptr, dst_ptr))

#define bfd_coff_reloc16_estimate(abfd, section, symbols, reloc, shrink)\
        ((coff_backend_info (abfd)->_bfd_coff_reloc16_estimate)\
         (section, symbols, reloc, shrink))
 
 
