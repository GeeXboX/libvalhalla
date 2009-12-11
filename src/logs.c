/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
 *
 *  Freely inspired from libplayer logging system.
 *   - libplayer.geexbox.org
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

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "valhalla.h"
#include "valhalla_internals.h"

#ifdef USE_LOGCOLOR
#define NORMAL   "\033[0m"
#define COLOR(x) "\033[" #x ";1m"
#define BOLD     COLOR(1)
#define F_RED    COLOR(31)
#define F_GREEN  COLOR(32)
#define F_YELLOW COLOR(33)
#define F_BLUE   COLOR(34)
#define B_RED    COLOR(41)
#endif /* USE_LOGCOLOR */

static pthread_mutex_t g_mutex_verb = PTHREAD_MUTEX_INITIALIZER;
static valhalla_verb_t g_verbosity  = VALHALLA_MSG_INFO;


void
vh_log_verb (valhalla_verb_t level)
{
  pthread_mutex_lock (&g_mutex_verb);
  g_verbosity = level;
  pthread_mutex_unlock (&g_mutex_verb);
}

int
vh_log_test (valhalla_verb_t level)
{
  valhalla_verb_t verbosity;

  pthread_mutex_lock (&g_mutex_verb);
  verbosity = g_verbosity;
  pthread_mutex_unlock (&g_mutex_verb);

  /* do we really want logging ? */
  if (verbosity == VALHALLA_MSG_NONE)
    return 0;

  if (level < verbosity)
    return 0;

  return 1;
}

void
vh_log_orig (valhalla_verb_t level,
      const char *file, int line, const char *format, ...)
{
#ifdef USE_LOGCOLOR
  static const char *const c[] = {
    [VALHALLA_MSG_VERBOSE]  = F_BLUE,
    [VALHALLA_MSG_INFO]     = F_GREEN,
    [VALHALLA_MSG_WARNING]  = F_YELLOW,
    [VALHALLA_MSG_ERROR]    = F_RED,
    [VALHALLA_MSG_CRITICAL] = B_RED,
  };
#endif /* USE_LOGCOLOR */
  static const char *const l[] = {
    [VALHALLA_MSG_VERBOSE]  = "Verb",
    [VALHALLA_MSG_INFO]     = "Info",
    [VALHALLA_MSG_WARNING]  = "Warn",
    [VALHALLA_MSG_ERROR]    = "Err",
    [VALHALLA_MSG_CRITICAL] = "Crit",
  };
  va_list va;

  if (!format || !vh_log_test (level))
    return;

  va_start (va, format);

#ifdef USE_LOGCOLOR
  fprintf (stderr, "[" BOLD "libvalhalla" NORMAL "] [%s:%i] %s%s" NORMAL ": ",
           file, line, c[level], l[level]);
#else
  fprintf (stderr, "[libvalhalla] [%s:%i] %s: ", file, line, l[level]);
#endif /* USE_LOGCOLOR */

  vfprintf (stderr, format, va);
  fprintf (stderr, "\n");
  va_end (va);
}
