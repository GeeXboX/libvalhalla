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
#include <stdlib.h>
#include <string.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "fifo_queue.h"
#include "logs.h"
#include "thread_utils.h"
#include "event_handler.h"

struct event_handler_od_s {
  char               *file;
  valhalla_event_od_t e;
  const char         *id;
};

struct event_handler_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  event_handler_cb_t cb;

  int             wait;
  int             run;
  pthread_mutex_t mutex_run;
};


static inline int
event_handler_is_stopped (event_handler_t *event_handler)
{
  int run;
  pthread_mutex_lock (&event_handler->mutex_run);
  run = event_handler->run;
  pthread_mutex_unlock (&event_handler->mutex_run);
  return !run;
}

void
vh_event_handler_od_free (event_handler_od_t *data)
{
  if (!data)
    return;

  if (data->file)
    free (data->file);
  free (data);
}

static void *
event_handler_thread (void *arg)
{
  int res;
  int e;
  void *data = NULL;
  event_handler_t *event_handler = arg;

  if (!event_handler)
    pthread_exit (NULL);

  vh_setpriority (event_handler->priority);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = vh_fifo_queue_pop (event_handler->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    if (!data)
      continue;

    switch (e)
    {
    default:
      break;

    case ACTION_EH_EVENTOD:
    {
      event_handler_od_t *edata = data;

      /* Send to the front-end callback for ondemand events. */
      event_handler->cb.od_cb (edata->file,
                               edata->e, edata->id, event_handler->cb.od_data);
      vh_event_handler_od_free (edata);
      break;
    }

    case ACTION_EH_EVENTGL:
    {
      valhalla_event_gl_t *edata = data;

      /* Send to the front-end callback for global events. */
      event_handler->cb.gl_cb (*edata, event_handler->cb.gl_data);
      free (edata);
      break;
    }
    }
  }
  while (!event_handler_is_stopped (event_handler));

  pthread_exit (NULL);
}

int
vh_event_handler_run (event_handler_t *event_handler, int priority)
{
  int res = EVENT_HANDLER_SUCCESS;
  pthread_attr_t attr;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler)
    return EVENT_HANDLER_ERROR_HANDLER;

  event_handler->priority = priority;
  event_handler->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = pthread_create (&event_handler->thread,
                        &attr, event_handler_thread, event_handler);
  if (res)
  {
    res = EVENT_HANDLER_ERROR_THREAD;
    event_handler->run = 0;
  }

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
vh_event_handler_fifo_get (event_handler_t *event_handler)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler)
    return NULL;

  return event_handler->fifo;
}

void
vh_event_handler_stop (event_handler_t *event_handler, int f)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler)
    return;

  if (f & STOP_FLAG_REQUEST && !event_handler_is_stopped (event_handler))
  {
    pthread_mutex_lock (&event_handler->mutex_run);
    event_handler->run = 0;
    pthread_mutex_unlock (&event_handler->mutex_run);

    vh_fifo_queue_push (event_handler->fifo,
                        FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
    event_handler->wait = 1;
  }

  if (f & STOP_FLAG_WAIT && event_handler->wait)
  {
    pthread_join (event_handler->thread, NULL);
    event_handler->wait = 0;
  }
}

void
vh_event_handler_uninit (event_handler_t *event_handler)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler)
    return;

  vh_fifo_queue_free (event_handler->fifo);
  pthread_mutex_destroy (&event_handler->mutex_run);

  free (event_handler);
}

event_handler_t *
vh_event_handler_init (valhalla_t *handle, event_handler_cb_t *cb)
{
  event_handler_t *event_handler;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !cb)
    return NULL;

  event_handler = calloc (1, sizeof (event_handler_t));
  if (!event_handler)
    return NULL;

  event_handler->fifo = vh_fifo_queue_new ();
  if (!event_handler->fifo)
    goto err;

  event_handler->valhalla = handle;
  memcpy (&event_handler->cb, cb, sizeof (event_handler_cb_t));

  pthread_mutex_init (&event_handler->mutex_run, NULL);

  return event_handler;

 err:
  vh_event_handler_uninit (event_handler);
  return NULL;
}

void
vh_event_handler_od_send (event_handler_t *event_handler, const char *file,
                          valhalla_event_od_t e, const char *id)
{
  event_handler_od_t *edata;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler || !event_handler->cb.od_cb)
    return;

  edata = calloc (1, sizeof (event_handler_od_t));
  if (!edata)
    return;

  edata->file = strdup (file);
  edata->e    = e;
  edata->id   = id;

  vh_fifo_queue_push (event_handler->fifo,
                      FIFO_QUEUE_PRIORITY_NORMAL, ACTION_EH_EVENTOD, edata);
}

void
vh_event_handler_gl_send (event_handler_t *event_handler, valhalla_event_gl_t e)
{
  valhalla_event_gl_t *data;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler || !event_handler->cb.gl_cb)
    return;

  data = malloc (sizeof (valhalla_event_gl_t));
  if (!data)
    return;

  *data = e;

  vh_fifo_queue_push (event_handler->fifo,
                      FIFO_QUEUE_PRIORITY_NORMAL, ACTION_EH_EVENTGL, data);
}
