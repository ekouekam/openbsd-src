divert(-1)
#
# Copyright (c) 2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#	Compile/run a test program for libsm.
#
#	$Sendmail: sm-test.m4,v 1.4 2001/01/18 04:15:04 ca Exp $
#
define(`smtest',
`bldPUSH_TARGET($1)dnl
bldLIST_PUSH_ITEM(`bldC_PRODUCTS', $1)dnl
bldPUSH_CLEAN_TARGET($1`-clean')dnl
divert(bldTARGETS_SECTION)
$1: ${BEFORE} $1.o libsm.a
	${CC} -o $1 ${LDOPTS} ${LIBDIRS} $1.o libsm.a ${LIBS}
ifelse(len(X`'$2), `1', `', `
	@echo ============================================================
	./$1
	@echo ============================================================')
$1-clean:
	rm -f $1 $1.o
divert(0)')
