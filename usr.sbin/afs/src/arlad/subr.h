/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Header with prototypes for OS-specific functions.
 */

/* $Id: subr.h,v 1.3 2000/09/11 14:40:43 art Exp $ */

#ifndef _SUBR_H_
#define _SUBR_H_

#include <fdir.h>

struct write_dirent_args {
    int fd;
#ifdef HAVE_OFF64_T
    off64_t off;
#else
    off_t off;
#endif
    char *buf;
    char *ptr;
    void *last;
    FCacheEntry *e; 
    CredCacheEntry *ce;
};

ino_t
dentry2ino (const char *name, const VenusFid *fid, const FCacheEntry *parent);

Result
conv_dir (FCacheEntry *e, CredCacheEntry *ce, u_int tokens,
	  xfs_cache_handle *, char *, size_t);

Result
conv_dir_sub (FCacheEntry *e, CredCacheEntry *ce, u_int tokens,
	      xfs_cache_handle *cache_handle,
	      char *cache_name, size_t cache_name_sz,
	      fdir_readdir_func func,
	      void (*flush_func)(void *),
	      size_t blocksize);

int
dir_remove_name (FCacheEntry *e, const char *filename,
		 xfs_cache_handle *cache_handle,
		 char *cache_name, size_t cache_name_sz);

#endif /* _SUBR_H_ */

