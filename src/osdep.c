/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu@schroetersa.ch>
 *
 * The clock_gettime support for Windows comes from Evil, an EFL library
 * mainly written by Vincent Torri <vtorri at univ-evry dot fr> and
 * providing many UNIX functions for the Windows platform :
 *   http://docs.enlightenment.org/auto/evil/
 *
 * This file is part of libvalhalla.
 *
 * libvalhalla is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libvalhalla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libvalhalla; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#ifdef OSDEP_CLOCK_GETTIME_DARWIN
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#endif /* OSDEP_CLOCK_GETTIME_DARWIN */

#ifdef OSDEP_CLOCK_GETTIME_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif /* OSDEP_CLOCK_GETTIME_WINDOWS */

#include "utils.h"
#include "osdep.h"

#ifdef OSDEP_CLOCK_GETTIME_WINDOWS
static LONGLONG time_freq;
static LONGLONG time_count;
static int64_t  time_second;


static int64_t
systemtime_to_time (SYSTEMTIME st)
{
  const int days[] = {
    -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364
  };
  int day;
  time_t time;

  st.wYear -= 1900;
  if ((st.wYear < 70) || (st.wYear > 138))
    return -1;

  day = st.wDay + days[st.wMonth - 1];

  if (!(st.wYear & 3) && (st.wMonth > 2))
    day++;

  time =   ((st.wYear - 70) * 365
         + ((st.wYear - 1) >> 2) - 17 + day) * 24 + st.wHour;
  time = (time * 60 + st.wMinute) * 60 + st.wSecond;

  return (int64_t) time;
}

static int
time_init (void)
{
  SYSTEMTIME    st;
  LARGE_INTEGER freq;
  LARGE_INTEGER count;
  WORD          second;

  if (!QueryPerformanceFrequency (&freq))
    return -1;

  time_freq = freq.QuadPart;

  GetSystemTime (&st);
  second = st.wSecond + 1;

  /* retrieve the tick corresponding to the time we retrieve above */
  while (1)
  {
    GetSystemTime (&st);
    QueryPerformanceCounter (&count);
    if ((st.wSecond & 1) == (second & 1))
      break;
  }

  time_second = systemtime_to_time (st);
  if (time_second < 0)
    return -1;

  time_count = count.QuadPart;
  return 0;
}

/*
 * Partial implementation of clock_gettime for Windows. Only CLOCK_REALTIME
 * is supported and errno is not set appropriately.
 */
int
vh_clock_gettime (clockid_t clk_id, struct timespec *tp)
{
  LARGE_INTEGER count;
  LONGLONG      diff;
  BOOL          ret;

  switch (clk_id)
  {
  case CLOCK_REALTIME:
    ret = QueryPerformanceCounter (&count);
    if (!ret)
      return -1;

    diff = count.QuadPart - time_count;

    tp->tv_sec  = time_second + diff / time_freq;
    tp->tv_nsec = ((diff % time_freq) * 1000000000LL) / time_freq;
    return 0;

  default:
    return -1;
  }
}
#endif /* OSDEP_CLOCK_GETTIME_WINDOWS */

#ifdef OSDEP_CLOCK_GETTIME_DARWIN
/*
 * Partial implementation of clock_gettime for Darwin. Only CLOCK_REALTIME
 * is supported and errno is not set appropriately.
 */
int
vh_clock_gettime (clockid_t clk_id, struct timespec *tp)
{
  kern_return_t   ret;
  clock_serv_t    clk;
  mach_timespec_t tm;

  switch (clk_id)
  {
  case CLOCK_REALTIME:
    ret = host_get_clock_service (mach_host_self (), CALENDAR_CLOCK, &clk);
    if (ret != KERN_SUCCESS)
      return -1;

    ret = clock_get_time (clk, &tm);
    if (ret != KERN_SUCCESS)
      return -1;

    tp->tv_sec  = tm.tv_sec;
    tp->tv_nsec = tm.tv_nsec;
    return 0;

  default:
    return -1;
  }
}
#endif /* OSDEP_CLOCK_GETTIME_DARWIN */

#ifdef OSDEP_STRNDUP
char *
vh_strndup (const char *s, size_t n)
{
  char *res;
  size_t length;

  length = strlen (s);
  if (length > n)
    length = n;

  res = malloc (length + 1);
  if (!res)
    return NULL;

  memcpy (res, s, length);
  res[length] = '\0';

  return res;
}
#endif /* OSDEP_STRNDUP */

#ifdef OSDEP_STRCASESTR
char *
vh_strcasestr (const char *haystack, const char *needle)
{
  size_t length;

  length = strlen (needle);
  if (!length)
    return (char *) haystack;

  for (; *haystack; haystack++)
    if (VH_TOLOWER (*haystack) == VH_TOLOWER (*needle))
      if (!strncasecmp (haystack, needle, length))
        return (char *) haystack;

  return NULL;
}
#endif /* OSDEP_STRCASESTR */

#ifdef OSDEP_STRTOK_R
char *
vh_strtok_r (char *str, const char *delim, char **saveptr)
{
  char *token;

  if (str)
    *saveptr = str;
  token = *saveptr;

  if (!token)
    return NULL;

  token += strspn (token, delim);
  *saveptr = strpbrk (token, delim);
  if (*saveptr)
    *(*saveptr)++ = '\0';

  return *token ? token : NULL;
}
#endif /* OSDEP_STRTOK_R */

#ifdef OSDEP_LSTAT
int
vh_lstat (const char *path, struct stat *buf)
{
#ifdef _WIN32
  return stat (path, buf);
#else /* _WIN32 */
#error "lstat unsupported by your OS"
#endif /* !_WIN32 */
}
#endif /* OSDEP_LSTAT */

int
vh_osdep_init (void)
{
  int rc = 0;
#ifdef OSDEP_CLOCK_GETTIME_WINDOWS
  rc = time_init ();
#endif /* OSDEP_CLOCK_GETTIME_WINDOWS */
  return rc;
}
