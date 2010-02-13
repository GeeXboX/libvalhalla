/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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
#include <time.h>
#include <stdlib.h>

#include "utils.h"
#include "osdep.h"
#include "timer_thread.h"

struct timer_thread_s {
  pthread_cond_t  cond;
  pthread_mutex_t mutex;
  int             run;
};


void
vh_timer_thread_sleep (timer_thread_t *timer, uint64_t timeout)
{
  struct timespec ts, ti;

  ti.tv_sec = 0;
  while (timeout >= 1000000000)
  {
    ti.tv_sec++;
    timeout -= 1000000000;
  }
  ti.tv_nsec = timeout;

  pthread_mutex_lock (&timer->mutex);
  if (timer->run)
  {
    clock_gettime (CLOCK_REALTIME, &ts);
    VH_TIMERADD (&ts, &ti, &ts);

    pthread_cond_timedwait (&timer->cond, &timer->mutex, &ts);
  }
  pthread_mutex_unlock (&timer->mutex);
}

void
vh_timer_thread_wakeup (timer_thread_t *timer)
{
  pthread_mutex_lock (&timer->mutex);
  if (timer->run)
    pthread_cond_signal (&timer->cond);
  pthread_mutex_unlock (&timer->mutex);
}

void
vh_timer_thread_stop (timer_thread_t *timer)
{
  if (!timer)
    return;

  pthread_mutex_lock (&timer->mutex);
  if (timer->run)
  {
    pthread_cond_signal (&timer->cond);
    timer->run = 0;
  }
  pthread_mutex_unlock (&timer->mutex);
}

void
vh_timer_thread_start (timer_thread_t *timer)
{
  if (!timer)
    return;

  pthread_mutex_lock (&timer->mutex);
  timer->run = 1;
  pthread_mutex_unlock (&timer->mutex);
}

void
vh_timer_thread_delete (timer_thread_t *timer)
{
  if (!timer)
    return;

  pthread_mutex_destroy (&timer->mutex);
  pthread_cond_destroy (&timer->cond);
  free (timer);
}

timer_thread_t *
vh_timer_thread_create (void)
{
  timer_thread_t *timer;

  timer = calloc (1, sizeof (timer_thread_t));
  if (!timer)
    return NULL;

  pthread_mutex_init (&timer->mutex, NULL);
  pthread_cond_init (&timer->cond, NULL);

  return timer;
}
