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

#ifndef VALHALLA_OSDEP_H
#define VALHALLA_OSDEP_H

#include <time.h>

#ifdef OSDEP_CLOCK_GETTIME_WINDOWS
#ifndef HAVE_STRUCT_TIMESPEC
struct timespec {
  time_t   tv_sec;
  long int tv_nsec;
};
#endif /* HAVE_STRUCT_TIMESPEC */
#endif /* OSDEP_CLOCK_GETTIME_WINDOWS */

#if defined (OSDEP_CLOCK_GETTIME_DARWIN) || defined (OSDEP_CLOCK_GETTIME_WINDOWS)
typedef enum {
  CLOCK_REALTIME = 0,
  CLOCK_MONOTONIC,          /* unsupported */
  CLOCK_PROCESS_CPUTIME_ID, /* unsupported */
  CLOCK_THREAD_CPUTIME_ID   /* unsupported */
} clockid_t;


int vh_clock_gettime (clockid_t clk_id, struct timespec *tp);
#undef  clock_gettime
#define clock_gettime vh_clock_gettime
#endif /* OSDEP_CLOCK_GETTIME_DARWIN || OSDEP_CLOCK_GETTIME_WINDOWS */
#ifdef OSDEP_STRNDUP
char *ivh_strndup (const char *s, size_t n);
#undef  strndup
#define strndup vh_strndup
#endif /* OSDEP_STRNDUP */
#ifdef OSDEP_STRCASESTR
char *vh_strcasestr (const char *haystack, const char *needle);
#undef  strcasestr
#define strcasestr vh_strcasestr
#endif /* OSDEP_STRCASESTR */
#ifdef OSDEP_STRTOK_R
char *vh_strtok_r (char *str, const char *delim, char **saveptr);
#undef  strtok_r
#define strtok_r vh_strtok_r
#endif /* OSDEP_STRTOK_R */
#ifdef OSDEP_LSTAT
int vh_lstat (const char *path, struct stat *buf);
#undef  lstat
#define lstat vh_lstat
#endif /* OSDEP_LSTAT */

int vh_osdep_init (void);

#endif /* VALHALLA_OSDEP_H */
