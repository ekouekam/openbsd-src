/* ====================================================================
 * Copyright (c) 1998-1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
 
 
 
 #include "ErrorHandling.hpp"
#include "CPStringUtils.hpp"

#ifdef __EXCEPTIONS_ENABLED__
	#include "CMyException.hpp"
#endif


static char					gErrorMessageBuffer[512];

char 						*gErrorMessage = gErrorMessageBuffer;
int							gErrorMessageMaxLength = sizeof(gErrorMessageBuffer);



void SetErrorMessage(const char *theErrorMessage)
{
	if (theErrorMessage != nil)
	{
		CopyCStrToCStr(theErrorMessage,gErrorMessage,gErrorMessageMaxLength);
	}
}


void SetErrorMessageAndAppendLongInt(const char *theErrorMessage,const long theLongInt)
{
	if (theErrorMessage != nil)
	{
		CopyCStrAndConcatLongIntToCStr(theErrorMessage,theLongInt,gErrorMessage,gErrorMessageMaxLength);
	}
}

void SetErrorMessageAndCStrAndLongInt(const char *theErrorMessage,const char * theCStr,const long theLongInt)
{
	if (theErrorMessage != nil)
	{
		CopyCStrAndInsertCStrLongIntIntoCStr(theErrorMessage,theCStr,theLongInt,gErrorMessage,gErrorMessageMaxLength);
	}

}

void SetErrorMessageAndCStr(const char *theErrorMessage,const char * theCStr)
{
	if (theErrorMessage != nil)
	{
		CopyCStrAndInsertCStrLongIntIntoCStr(theErrorMessage,theCStr,-1,gErrorMessage,gErrorMessageMaxLength);
	}
}


void AppendCStrToErrorMessage(const char *theErrorMessage)
{
	if (theErrorMessage != nil)
	{
		ConcatCStrToCStr(theErrorMessage,gErrorMessage,gErrorMessageMaxLength);
	}
}


void AppendLongIntToErrorMessage(const long theLongInt)
{
	ConcatLongIntToCStr(theLongInt,gErrorMessage,gErrorMessageMaxLength);
}



char *GetErrorMessage(void)
{
	return gErrorMessage;
}


OSErr GetErrorMessageInNewHandle(Handle *inoutHandle)
{
OSErr		errCode;


	errCode = CopyCStrToNewHandle(gErrorMessage,inoutHandle);
	
	return(errCode);
}


OSErr GetErrorMessageInExistingHandle(Handle inoutHandle)
{
OSErr		errCode;


	errCode = CopyCStrToExistingHandle(gErrorMessage,inoutHandle);
	
	return(errCode);
}



OSErr AppendErrorMessageToHandle(Handle inoutHandle)
{
OSErr		errCode;


	errCode = AppendCStrToHandle(gErrorMessage,inoutHandle,nil);
	
	return(errCode);
}


#ifdef __EXCEPTIONS_ENABLED__

void ThrowErrorMessageException(void)
{
	ThrowDescriptiveException(gErrorMessage);
}

#endif
