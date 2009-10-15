/* Software floating-point emulation.
   Return 0 iff a == b, 1 iff a > b, 2 iff a ? b, -1 iff a < b
   Copyright (C) 1997,1999,2006 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Richard Henderson (rth@cygnus.com) and
		  Jakub Jelinek (jj@ultra.linux.cz).

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   In addition to the permissions in the GNU Lesser General Public
   License, the Free Software Foundation gives you unlimited
   permission to link the compiled version of this file into
   combinations with other programs, and to distribute those
   combinations without any restriction coming from the use of this
   file.  (The Lesser General Public License restrictions do apply in
   other respects; for example, they cover modification of the file,
   and distribution when not linked into a combine executable.)

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "soft-fp.h"
#include "quad.h"

int __letf2(TFtype a, TFtype b)
{
  FP_DECL_EX;
  FP_DECL_Q(A); FP_DECL_Q(B);
  int r;

  FP_UNPACK_RAW_Q(A, a);
  FP_UNPACK_RAW_Q(B, b);
  FP_CMP_Q(r, A, B, 2);
  if (r == 2 && (FP_ISSIGNAN_Q(A) || FP_ISSIGNAN_Q(B)))
    FP_SET_EXCEPTION(FP_EX_INVALID);
  FP_HANDLE_EXCEPTIONS;

  return r;
}

strong_alias(__letf2, __lttf2);
