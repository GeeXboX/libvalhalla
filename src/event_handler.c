/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu@schroetersa.ch>
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
#include "metadata.h"
#include "list.h"
#include "event_handler.h"

#define VH_HANDLE event_handler->valhalla

struct event_handler_od_s {
  char               *file;
  valhalla_event_od_t e;
  const char         *id;
  list_t             *keys;
};

struct event_handler_md_s {
  valhalla_file_t     file;
  const char         *id;
  metadata_t         *meta;
  valhalla_event_md_t e;
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

  /* special for ondemand callback */
  int                       od_meta;
  const event_handler_od_t *edata;
  pthread_mutex_t           mutex_meta;
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
  vh_list_free (data->keys);
  free (data);
}

void
vh_event_handler_md_free (event_handler_md_t *data)
{
  if (!data)
    return;

  if (data->file.path)
    free ((void *) data->file.path);

  vh_metadata_free (data->meta);
  free (data);
}

static void *
event_handler_thread (void *arg)
{
  int res, tid;
  int e;
  void *data = NULL;
  event_handler_t *event_handler = arg;

  if (!event_handler)
    pthread_exit (NULL);

  tid = vh_setpriority (event_handler->priority);

  vh_log (VALHALLA_MSG_VERBOSE,
          "[%s] tid: %i priority: %i",
          __FUNCTION__, tid, event_handler->priority);

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

      if (event_handler->od_meta)
      {
        event_handler->edata = edata;
        pthread_mutex_unlock (&event_handler->mutex_meta);
      }

      /* Send to the front-end callback for ondemand events. */
      event_handler->cb.od_cb (edata->file,
                               edata->e, edata->id, event_handler->cb.od_data);

      if (event_handler->od_meta)
      {
        pthread_mutex_lock (&event_handler->mutex_meta);
        event_handler->edata = NULL;
      }

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

    case ACTION_EH_EVENTMD:
    {
      const metadata_t *tag = NULL;
      event_handler_md_t *edata = data;

      while (!vh_metadata_get (edata->meta, "", METADATA_IGNORE_SUFFIX, &tag))
      {
        valhalla_metadata_t md;

        md.name  = tag->name;
        md.value = tag->value;
        md.group = tag->group;

        /* Send to the front-end callback for metadata events. */
        event_handler->cb.md_cb (edata->e, edata->id,
                                 &edata->file, &md, event_handler->cb.md_data);
      }

      vh_event_handler_md_free (edata);
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

  if (event_handler->od_meta)
  {
    pthread_mutex_unlock (&event_handler->mutex_meta);
    pthread_mutex_destroy (&event_handler->mutex_meta);
  }

  free (event_handler);
}

event_handler_t *
vh_event_handler_init (valhalla_t *handle, event_handler_cb_t *cb, int od_meta)
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

  event_handler->valhalla = handle; /* VH_HANDLE */
  event_handler->cb       = *cb;

  pthread_mutex_init (&event_handler->mutex_run, NULL);

  /*
   * The meta (without data) can be retrieved in the callback with the
   * function valhalla_ondemand_cb_meta().
   */
  if (od_meta)
  {
    event_handler->od_meta = 1;
    pthread_mutex_init (&event_handler->mutex_meta, NULL);
    pthread_mutex_lock (&event_handler->mutex_meta);
  }

  return event_handler;

 err:
  vh_event_handler_uninit (event_handler);
  return NULL;
}

const char *
vh_event_handler_od_cb_meta (event_handler_t *event_handler, const char *meta)
{
  int rc, i = 0, stop = 0;
  const char *res = NULL;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  /* We must check if this call comes from the callback. */
  rc = pthread_mutex_trylock (&event_handler->mutex_meta);
  if (rc)
  {
    vh_log (VALHALLA_MSG_WARNING,
            "%s must be called only from the ondemand callback", __FUNCTION__);
    return NULL;
  }

  do
  {
    if (res && !strcmp (res, meta))
      stop = 1;
    res = vh_list_pos (event_handler->edata->keys, i++);
    if (!res || !meta)
      stop = 1;
  }
  while (!stop);

  pthread_mutex_unlock (&event_handler->mutex_meta);
  return res;
}

void
vh_event_handler_od_send (event_handler_t *event_handler, const char *file,
                          valhalla_event_od_t e, const char *id,
                          metadata_t *meta)
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

  if (meta && event_handler->od_meta)
  {
    const metadata_t *tag = NULL;

    edata->keys = vh_list_new (0, NULL);

    while (!vh_metadata_get (meta, "", METADATA_IGNORE_SUFFIX, &tag))
      vh_list_append (edata->keys, tag->name, strlen (tag->name) + 1);
  }

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

int
vh_event_handler_md_send (event_handler_t *event_handler,
                          valhalla_event_md_t e, const char *id,
                          valhalla_file_t *file, metadata_t *meta)
{
  event_handler_md_t *data;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!event_handler || !event_handler->cb.md_cb || !meta)
    return -1;

  data = calloc (1, sizeof (event_handler_md_t));
  if (!data)
    return -1;

  if (e == VALHALLA_EVENTMD_PARSER)
  {
    vh_metadata_dup (&data->meta, meta);
    if (!data->meta)
    {
      free (data);
      return -1;
    }
  }
  else
    data->meta = meta;

  data->file.path  = strdup (file->path);
  data->file.mtime = file->mtime;
  data->file.size  = file->size;
  data->file.type  = file->type;
  data->e          = e;
  data->id         = id;

  vh_fifo_queue_push (event_handler->fifo,
                      FIFO_QUEUE_PRIORITY_NORMAL, ACTION_EH_EVENTMD, data);
  return 0;
}
