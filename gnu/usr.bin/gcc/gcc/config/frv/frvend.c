/* Frv initialization file linked after all user modules
   Copyright (C) 1999, 2000 Free Software Foundation, Inc.
    Contributed by Red Hat, Inc.
  
   This file is part of GNU CC.
  
   GNU CC is free software ; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation * either version 2, or (at your option)
   any later version.
  
   GNU CC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY ; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with GNU CC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA. */

#include "defaults.h"
#include <stddef.h>
#include "unwind-dw2-fde.h"

#ifdef __FRV_UNDERSCORE__
#define UNDERSCORE "_"
#else
#define UNDERSCORE ""
#endif

#define FINI_SECTION_ZERO(SECTION, FLAGS, NAME)				\
__asm__ (".section " SECTION "," FLAGS "\n\t"				\
	 ".globl   " UNDERSCORE NAME "\n\t"				\
	 ".type    " UNDERSCORE NAME ",@object\n\t"			\
	 ".p2align  2\n"						\
	 UNDERSCORE NAME ":\n\t"					\
	 ".word     0\n\t"						\
	 ".previous")

#define FINI_SECTION(SECTION, FLAGS, NAME)				\
__asm__ (".section " SECTION "," FLAGS "\n\t"				\
	 ".globl   " UNDERSCORE NAME "\n\t"				\
	 ".type    " UNDERSCORE NAME ",@object\n\t"			\
	 ".p2align  2\n"						\
	 UNDERSCORE NAME ":\n\t"					\
	 ".previous")

/* End of .ctor/.dtor sections that provides a list of constructors and
   destructors to run.  */

FINI_SECTION_ZERO (".ctors", "\"aw\"", "__CTOR_END__");
FINI_SECTION_ZERO (".dtors", "\"aw\"", "__DTOR_END__");

/* End of .eh_frame section that provides all of the exception handling
   tables.  */

FINI_SECTION_ZERO (".eh_frame", "\"aw\"", "__FRAME_END__");

/* End of .rofixup section that provides a list of pointers that we
   need to adjust.  */

FINI_SECTION (".rofixup", "\"a\"", "__ROFIXUP_END__");
