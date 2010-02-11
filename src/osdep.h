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

#ifdef OSDEP_CLOCK_GETTIME_DARWIN
typedef enum {
  CLOCK_REALTIME = 0,
  CLOCK_MONOTONIC,          /* unsupported */
  CLOCK_PROCESS_CPUTIME_ID, /* unsupported */
  CLOCK_THREAD_CPUTIME_ID   /* unsupported */
} clockid_t;


int clock_gettime (clockid_t clk_id, struct timespec *tp);
#endif /* OSDEP_CLOCK_GETTIME_DARWIN */

#ifdef OSDEP_STRNDUP
char *strndup (const char *s, size_t n);
#endif /* OSDEP_STRNDUP */
#ifdef OSDEP_STRCASESTR
char *strcasestr (const char *haystack, const char *needle);
#endif /* OSDEP_STRCASESTR */
#ifdef OSDEP_STRTOK_R
char *strtok_r (char *str, const char *delim, char **saveptr);
#endif /* OSDEP_STRTOK_R */

#endif /* VALHALLA_OSDEP_H */
