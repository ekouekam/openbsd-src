/* crypto/conf/conf.h */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#ifndef  HEADER_CONF_H
#define HEADER_CONF_H

#ifdef  __cplusplus
extern "C" {
#endif

#include "stack.h"
#include "lhash.h"

typedef struct
	{
	char *section;
	char *name;
	char *value;
	} CONF_VALUE;

#ifndef NOPROTO

LHASH *CONF_load(LHASH *conf,char *file,long *eline);
STACK *CONF_get_section(LHASH *conf,char *section);
char *CONF_get_string(LHASH *conf,char *group,char *name);
long CONF_get_number(LHASH *conf,char *group,char *name);
void CONF_free(LHASH *conf);
void ERR_load_CONF_strings(void );

#else

LHASH *CONF_load();
STACK *CONF_get_section();
char *CONF_get_string();
long CONF_get_number();
void CONF_free();
void ERR_load_CONF_strings();

#endif

/* BEGIN ERROR CODES */
/* Error codes for the CONF functions. */

/* Function codes. */
#define CONF_F_CONF_LOAD				 100
#define CONF_F_STR_COPY					 101

/* Reason codes. */
#define CONF_R_MISSING_CLOSE_SQUARE_BRACKET		 100
#define CONF_R_MISSING_EQUAL_SIGN			 101
#define CONF_R_NO_CLOSE_BRACE				 102
#define CONF_R_UNABLE_TO_CREATE_NEW_SECTION		 103
#define CONF_R_VARIABLE_HAS_NO_VALUE			 104
 
#ifdef  __cplusplus
}
#endif
#endif

