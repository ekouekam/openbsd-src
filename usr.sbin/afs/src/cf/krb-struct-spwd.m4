dnl $KTH: krb-struct-spwd.m4,v 1.2 2000/03/14 00:04:10 assar Exp $
dnl
dnl Test for `struct spwd'

AC_DEFUN(AC_KRB_STRUCT_SPWD, [
AC_MSG_CHECKING(for struct spwd)
AC_CACHE_VAL(ac_cv_type_struct_spwd, [
AC_TRY_COMPILE(
[#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif],
[struct spwd foo;],
ac_cv_struct_spwd=yes,
ac_cv_struct_spwd=no)
])
AC_MSG_RESULT($ac_cv_struct_spwd)

if test "$ac_cv_struct_spwd" = "yes"; then
  AC_DEFINE(HAVE_STRUCT_SPWD, 1, [define if you have struct spwd])
fi
])
