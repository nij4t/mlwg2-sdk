/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2009, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at http://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/curl/lib/curl_sspi.c#1 $
 ***************************************************************************/

#include "setup.h"

#ifdef USE_WINDOWS_SSPI

#include <curl/curl.h>

#include "curl_sspi.h"

#define _MPRINTF_REPLACE /* use our functions only */
#include <curl/mprintf.h>

#include "curl_memory.h"
/* The last #include file should be: */
#include "memdebug.h"


/* Handle of security.dll or secur32.dll, depending on Windows version */
HMODULE s_hSecDll = NULL;

/* Pointer to SSPI dispatch table */
PSecurityFunctionTableA s_pSecFn = NULL;


/*
 * Curl_sspi_global_init()
 *
 * This is used to load the Security Service Provider Interface (SSPI)
 * dynamic link library portably across all Windows versions, without
 * the need to directly link libcurl, nor the application using it, at
 * build time.
 *
 * Once this function has been executed, Windows SSPI functions can be
 * called through the Security Service Provider Interface dispatch table.
 */

CURLcode
Curl_sspi_global_init(void)
{
  OSVERSIONINFO osver;
  INIT_SECURITY_INTERFACE_A pInitSecurityInterface;

  /* If security interface is not yet initialized try to do this */
  if(s_hSecDll == NULL) {

    /* Find out Windows version */
    memset(&osver, 0, sizeof(osver));
    osver.dwOSVersionInfoSize = sizeof(osver);
    if(! GetVersionEx(&osver))
      return CURLE_FAILED_INIT;

    /* Security Service Provider Interface (SSPI) functions are located in
     * security.dll on WinNT 4.0 and in secur32.dll on Win9x. Win2K and XP
     * have both these DLLs (security.dll forwards calls to secur32.dll) */

    /* Load SSPI dll into the address space of the calling process */
    if(osver.dwPlatformId == VER_PLATFORM_WIN32_NT
      && osver.dwMajorVersion == 4)
      s_hSecDll = LoadLibrary("security.dll");
    else
      s_hSecDll = LoadLibrary("secur32.dll");
    if(! s_hSecDll)
      return CURLE_FAILED_INIT;

    /* Get address of the InitSecurityInterfaceA function from the SSPI dll */
    pInitSecurityInterface = (INIT_SECURITY_INTERFACE_A)
      GetProcAddress(s_hSecDll, "InitSecurityInterfaceA");
    if(! pInitSecurityInterface)
      return CURLE_FAILED_INIT;

    /* Get pointer to Security Service Provider Interface dispatch table */
    s_pSecFn = pInitSecurityInterface();
    if(! s_pSecFn)
      return CURLE_FAILED_INIT;

  }
  return CURLE_OK;
}


/*
 * Curl_sspi_global_cleanup()
 *
 * This deinitializes the Security Service Provider Interface from libcurl.
 */

void
Curl_sspi_global_cleanup(void)
{
  if(s_hSecDll) {
    FreeLibrary(s_hSecDll);
    s_hSecDll = NULL;
    s_pSecFn = NULL;
  }
}

#endif /* USE_WINDOWS_SSPI */
