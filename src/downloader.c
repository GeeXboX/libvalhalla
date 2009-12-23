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
#include <stdio.h>
#include <string.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "utils.h"
#include "url_utils.h"
#include "fifo_queue.h"
#include "logs.h"
#include "thread_utils.h"
#include "dispatcher.h"
#include "downloader.h"

struct downloader_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             wait;
  int             run;
  pthread_mutex_t mutex_run;

  VH_THREAD_PAUSE_ATTRS

  url_t *url_handler;
  char **dl_list;
};


static inline int
downloader_is_stopped (downloader_t *downloader)
{
  int run;
  pthread_mutex_lock (&downloader->mutex_run);
  run = downloader->run;
  pthread_mutex_unlock (&downloader->mutex_run);
  return !run;
}

static void *
downloader_thread (void *arg)
{
  int res, tid;
  int e;
  void *data = NULL;
  file_data_t *pdata;
  downloader_t *downloader = arg;

  if (!downloader)
    pthread_exit (NULL);

  tid = vh_setpriority (downloader->priority);

  vh_log (VALHALLA_MSG_VERBOSE,
          "[%s] tid: %i priority: %i", __FUNCTION__, tid, downloader->priority);

  do
  {
    int interrup = 0;

    e = ACTION_NO_OPERATION;
    data = NULL;

    res = vh_fifo_queue_pop (downloader->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    if (e == ACTION_PAUSE_THREAD)
    {
      VH_THREAD_PAUSE_ACTION (downloader)
      continue;
    }

    pdata = data;

    if (pdata->list_downloader)
    {
      file_dl_t *it = pdata->list_downloader;
      for (; it; it = it->next)
      {
        char *dest;
        size_t len;
        valhalla_dl_t dst = it->dst;
        int err;

        if (downloader_is_stopped (downloader))
        {
          interrup = 1;
          break;
        }

        if (!it->url || dst >= VALHALLA_DL_LAST)
          continue;

        if (!downloader->dl_list[dst] || !downloader->dl_list[dst][0])
          dst = VALHALLA_DL_DEFAULT;

        if (!downloader->dl_list[dst] || !downloader->dl_list[dst][0])
          continue;

        len = strlen (downloader->dl_list[dst]) + strlen (it->name) + 2;
        dest = malloc (len);
        if (!dest)
          continue;

        snprintf (dest, len, "%s%s%s",
                  downloader->dl_list[dst],
                  *(strrchr (downloader->dl_list[dst], '\0') - 1) == '/'
                  ? "" : "/",
                  it->name);

        err = vh_url_save_to_disk (downloader->url_handler, it->url, dest);
        if (!err)
          vh_log (VALHALLA_MSG_VERBOSE, "[%s] %s saved to %s",
                  __FUNCTION__, it->url, downloader->dl_list[dst]);
        free (dest);
      }
    }

    if (!interrup)
      vh_file_data_step_increase (pdata, &e);
    vh_dispatcher_action_send (downloader->valhalla->dispatcher,
                               pdata->priority, e, pdata);
  }
  while (!downloader_is_stopped (downloader));

  pthread_exit (NULL);
}

int
vh_downloader_run (downloader_t *downloader, int priority)
{
  int res = DOWNLOADER_SUCCESS;
  pthread_attr_t attr;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return DOWNLOADER_ERROR_HANDLER;

  downloader->priority = priority;
  downloader->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res =
    pthread_create (&downloader->thread, &attr, downloader_thread, downloader);
  if (res)
  {
    res = DOWNLOADER_ERROR_THREAD;
    downloader->run = 0;
  }

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
vh_downloader_fifo_get (downloader_t *downloader)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return NULL;

  return downloader->fifo;
}

void
vh_downloader_pause (downloader_t *downloader)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  VH_THREAD_PAUSE_FCT (downloader, 1)
}

void
vh_downloader_stop (downloader_t *downloader, int f)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  if (f & STOP_FLAG_REQUEST && !downloader_is_stopped (downloader))
  {
    pthread_mutex_lock (&downloader->mutex_run);
    downloader->run = 0;
    pthread_mutex_unlock (&downloader->mutex_run);

    vh_fifo_queue_push (downloader->fifo,
                        FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
    downloader->wait = 1;
  }

  if (f & STOP_FLAG_WAIT && downloader->wait)
  {
    pthread_join (downloader->thread, NULL);
    downloader->wait = 0;
  }
}

void
vh_downloader_uninit (downloader_t *downloader)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  if (downloader->url_handler)
    vh_url_free (downloader->url_handler);

  if (downloader->dl_list)
  {
    char **it = downloader->dl_list;
    for (; it && it - downloader->dl_list < VALHALLA_DL_LAST; it++)
      if (*it)
        free (*it);
    free (downloader->dl_list);
  }

  vh_fifo_queue_free (downloader->fifo);
  pthread_mutex_destroy (&downloader->mutex_run);
  VH_THREAD_PAUSE_UNINIT (downloader)

  free (downloader);
}

downloader_t *
vh_downloader_init (valhalla_t *handle)
{
  downloader_t *downloader;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  downloader = calloc (1, sizeof (downloader_t));
  if (!downloader)
    return NULL;

  downloader->fifo = vh_fifo_queue_new ();
  if (!downloader->fifo)
    goto err;

  downloader->url_handler = vh_url_new ();
  if (!downloader->url_handler)
    goto err;

  downloader->dl_list = calloc (VALHALLA_DL_LAST, sizeof (char *));
  if (!downloader->dl_list)
    goto err;

  downloader->valhalla = handle;

  pthread_mutex_init (&downloader->mutex_run, NULL);
  VH_THREAD_PAUSE_INIT (downloader)

  return downloader;

 err:
  vh_downloader_uninit (downloader);
  return NULL;
}

void
vh_downloader_destination_set (downloader_t *downloader,
                               valhalla_dl_t dl, const char *dst)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader || !downloader->dl_list || dl >= VALHALLA_DL_LAST)
    return;

  if (downloader->dl_list[dl])
    free (downloader->dl_list[dl]);

  downloader->dl_list[dl] = dst ? strdup (dst) : NULL;
}

const char *
vh_downloader_destination_get (downloader_t *downloader, valhalla_dl_t dl)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader || !downloader->dl_list || dl >= VALHALLA_DL_LAST)
    return NULL;

  return downloader->dl_list[dl];
}

void
vh_downloader_action_send (downloader_t *downloader,
                           fifo_queue_prio_t prio, int action, void *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  vh_fifo_queue_push (downloader->fifo, prio, action, data);
}
