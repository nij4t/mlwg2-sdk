/*
 * ProFTPD - FTP server daemon
 * Copyright (c) 2007 The ProFTPD Project team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 *
 * As a special exemption, The ProFTPD Project team and other respective
 * copyright holders give permission to link this program with OpenSSL, and
 * distribute the resulting executable, without including the source code for
 * OpenSSL in the source distribution.
 */

/*
 * Environment management
 * $Id: //WIFI_SOC/release/SDK_4_1_0_0/source/user/proftpd/src/env.c#1 $
 */

#include "conf.h"

char *pr_env_get(pool *p, const char *key) {
  if (!p || !key) {
    errno = EINVAL;
    return NULL;
  }

#if defined(HAVE_GETENV)
  return getenv(key);
#else
  errno = ENOSYS;
  return NULL;
#endif /* !HAVE_GETENV */
}

int pr_env_set(pool *p, const char *key, const char *value) {
#if defined(HAVE_SETENV)
  const char *k, *v;
#elif defined(HAVE_PUTENV)
  const char *str;
#endif /* !HAVE_SETENV and !HAVE_PUTENV */

  if (!p || !key || !value) {
    errno = EINVAL;
    return -1;
  }

  /* In the PR_USE_DEVEL cases below, we use strdup(2) rather than ProFTPD's
   * pstrdup() in order to soothe memory trackers (e.g. Valgrind) who may
   * complain.
   */

#if defined(HAVE_SETENV)
# ifdef PR_USE_DEVEL
  k = strdup(key);
  if (!k) {
    pr_log_pri(PR_LOG_ERR, "fatal: Memory exhausted");
    exit(1);
  }

  v = strdup(value);
  if (!v) {
    pr_log_pri(PR_LOG_ERR, "fatal: Memory exhausted");
    exit(1);
  }

# else
  k = key;
  v = value;
# endif /* PR_USE_DEVEL */
  return setenv(k, v, 1);

#elif defined(HAVE_PUTENV)
  str = pstrcat(p, key, "=", value, NULL);

# ifdef PR_USE_DEVEL
  str = strdup(str);
  if (!str) {
    pr_log_pri(PR_LOG_ERR, "fatal: Memory exhausted");
    exit(1);
  }
# endif /* PR_USE_DEVEL */
  return putenv((char *) str);

#else
  errno = ENOSYS;
  return -1;
#endif /* !HAVE_SETENV and !HAVE_PUTENV */
}

int pr_env_unset(pool *p, const char *key) {
#if defined(HAVE_UNSETENV)
  char *res;
#endif /* !HAVE_UNSETENV */

  if (!p || !key) {
    errno = EINVAL;
    return -1;
  }

#if defined(HAVE_UNSETENV)
  /* The same key may appear multiple times in the environ, so make certain
   * that all such occurrences are removed.
   */
  res = pr_env_get(p, key);
  while (res) {
    pr_signals_handle();

    unsetenv(key);
    res = pr_env_get(p, key);
  }

  return 0;
#else
  errno = ENOSYS;
  return -1;
#endif /* !HAVE_UNSETENV */
}

