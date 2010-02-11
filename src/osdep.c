/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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

#ifdef OSDEP_CLOCK_GETTIME_DARWIN
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#endif /* OSDEP_CLOCK_GETTIME_DARWIN */

#include "utils.h"
#include "osdep.h"


#ifdef OSDEP_CLOCK_GETTIME_DARWIN
/*
 * Partial implementation of clock_gettime for Darwin. Only CLOCK_REALTIME
 * is supported and errno is not set appropriately.
 */
int
clock_gettime (clockid_t clk_id, struct timespec *tp)
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
strndup (const char *s, size_t n)
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
strcasestr (const char *haystack, const char *needle)
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
strtok_r (char *str, const char *delim, char **saveptr)
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
