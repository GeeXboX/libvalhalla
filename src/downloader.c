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
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

  int             run;
  pthread_mutex_t mutex_run;

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
  int res;
  int e;
  void *data = NULL;
  file_data_t *pdata;
  downloader_t *downloader = arg;

  if (!downloader)
    pthread_exit (NULL);

  my_setpriority (downloader->priority);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = fifo_queue_pop (downloader->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    pdata = data;

    if (pdata->list_downloader)
    {
      file_dl_t *it = pdata->list_downloader;
      for (; it; it = it->next)
      {
        char *dest;
        size_t len;
        valhalla_dl_t dst = it->dst;

        if (downloader_is_stopped (downloader))
          break;

        if (!it->url || dst >= VALHALLA_DL_LAST || dst < 0)
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
        url_save_to_disk (downloader->url_handler, it->url, dest);
        valhalla_log (VALHALLA_MSG_VERBOSE, "[%s] %s saved to %s",
                      __FUNCTION__, it->url, downloader->dl_list[dst]);
        free (dest);
      }
    }

    file_data_step_increase (pdata, &e);
    dispatcher_action_send (downloader->valhalla->dispatcher, e, pdata);
  }
  while (!downloader_is_stopped (downloader));

  pthread_exit (NULL);
}

int
downloader_run (downloader_t *downloader, int priority)
{
  int res = DOWNLOADER_SUCCESS;
  pthread_attr_t attr;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
downloader_fifo_get (downloader_t *downloader)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return NULL;

  return downloader->fifo;
}

void
downloader_stop (downloader_t *downloader)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  if (downloader_is_stopped (downloader))
    return;

  pthread_mutex_lock (&downloader->mutex_run);
  downloader->run = 0;
  pthread_mutex_unlock (&downloader->mutex_run);

  fifo_queue_push (downloader->fifo, ACTION_KILL_THREAD, NULL);
  pthread_join (downloader->thread, NULL);
}

void
downloader_uninit (downloader_t *downloader)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  if (downloader->url_handler)
    url_free (downloader->url_handler);

  if (downloader->dl_list)
  {
    char **it = downloader->dl_list;
    for (; it && it - downloader->dl_list < VALHALLA_DL_LAST; it++)
      if (*it)
        free (*it);
    free (downloader->dl_list);
  }

  fifo_queue_free (downloader->fifo);
  pthread_mutex_destroy (&downloader->mutex_run);

  free (downloader);
}

downloader_t *
downloader_init (valhalla_t *handle)
{
  downloader_t *downloader;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  downloader = calloc (1, sizeof (downloader_t));
  if (!downloader)
    return NULL;

  downloader->fifo = fifo_queue_new ();
  if (!downloader->fifo)
    goto err;

  downloader->url_handler = url_new ();
  if (!downloader->url_handler)
    goto err;

  downloader->dl_list = calloc (VALHALLA_DL_LAST, sizeof (char *));
  if (!downloader->dl_list)
    goto err;

  downloader->valhalla = handle;

  pthread_mutex_init (&downloader->mutex_run, NULL);

  return downloader;

 err:
  downloader_uninit (downloader);
  return NULL;
}

void
downloader_destination_set (downloader_t *downloader,
                            valhalla_dl_t dl, const char *dst)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader || !downloader->dl_list || !dst
      || dl >= VALHALLA_DL_LAST || dl < 0)
    return;

  if (downloader->dl_list[dl])
    free (downloader->dl_list[dl]);

  downloader->dl_list[dl] = strdup (dst);
}

const char *
downloader_destination_get (downloader_t *downloader, valhalla_dl_t dl)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader || !downloader->dl_list || dl >= VALHALLA_DL_LAST || dl < 0)
    return NULL;

  return downloader->dl_list[dl];
}

void
downloader_action_send (downloader_t *downloader, int action, void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!downloader)
    return;

  fifo_queue_push (downloader->fifo, action, data);
}
