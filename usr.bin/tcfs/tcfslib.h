/*
 *	Transparent Cryptographic File System (TCFS) for NetBSD 
 *	Author and mantainer: 	Luigi Catuogno [luicat@tcfs.unisa.it]
 *	
 *	references:		http://tcfs.dia.unisa.it
 *				tcfs-bsd@tcfs.unisa.it
 */

/*
 *	Base utility set v0.1
 *
 *	  $Source: /var/cvsync/src/usr.bin/tcfs/Attic/tcfslib.h,v $
 *	   $State: Exp $
 *	$Revision: 1.1 $
 *	  $Author: provos $
 *	    $Date: 2000/06/18 22:07:24 $
 *
 */

/* RCS_HEADER_ENDS_HERE */



#include <unistd.h>
#include "tcfsdefines.h"
#include "tcfspwdb.h"

extern int tcfspwdbr_new (tcfspwdb **p);
extern int tcfspwdbr_edit (tcfspwdb **p, int i, ...);
extern int tcfspwdbr_read (tcfspwdb *p, int i, ...);
extern void tcfspwdbr_dispose (tcfspwdb *p);
extern int tcfsgpwdbr_new (tcfsgpwdb **p);
extern int tcfsgpwdbr_edit (tcfsgpwdb **p, int i, ...);
extern int tcfsgpwdbr_read (tcfsgpwdb *p, int i, ...);
extern void tcfsgpwdbr_dispose (tcfsgpwdb *p);
extern int tcfs_chgpwd (char *u, char *o, char *p);
extern int tcfs_group_chgpwd (char *u, gid_t gid, char *o, char *p);
extern int tcfs_chgpassword (char *u, char *o, char *p);
extern int tcfs_decrypt_key (char *u, char *pwd, unsigned char *t, unsigned char *tk, unsigned int flag);
extern int tcfs_encrypt_key (char *u, char *pw, unsigned char *key, unsigned char *ek, unsigned int flag);
extern char *tcfs_decode (char *t, int *l);
extern char *tcfs_encode (char *t, int l);
extern char *gentcfskey (void);


