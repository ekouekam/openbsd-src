#	$Sendmail: NEXTSTEP.4.x,v 8.5 1999/12/08 06:08:07 gshapiro Exp $
PUSHDIVERT(1)
# NEXTSTEP 3.1 and 3.2 only support m68k and i386
#ARCH=  -arch m68k -arch i386 -arch hppa -arch sparc
#ARCH=  -arch m68k -arch i386
#ARCH=   ${RC_CFLAGS}
# For new sendmail Makefile structure, this must go in the ENVDEF and LDOPTS
POPDIVERT
define(`confBEFORE', `unistd.h dirent.h')
define(`confMAPDEF', `-DNDBM -DNIS -DNETINFO')
define(`confENVDEF', `-DNeXT -Wno-precomp -pipe ${RC_CFLAGS}')
define(`confLDOPTS', `${RC_CFLAGS}')
define(`confLIBS', `-ldbm')
define(`confMBINDIR', `/usr/lib')
define(`confSBINDIR', `/usr/etc')
define(`confUBINDIR', `/usr/ucb')
define(`confEBINDIR', `/usr/lib')
define(`confINSTALL', `${BUILDBIN}/install.sh')
define(`confRANLIBOPTS', `-c')
PUSHDIVERT(3)
unistd.h:
	cp /dev/null unistd.h

dirent.h:
	echo "#include <sys/dir.h>" > dirent.h
	echo "#define dirent	direct" >> dirent.h
POPDIVERT
