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
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "utils.h"
#include "fifo_queue.h"
#include "logs.h"
#include "thread_utils.h"
#include "grabber.h"
#include "grabber_common.h"
#include "dbmanager.h"
#include "dispatcher.h"

#ifdef HAVE_GRABBER_DUMMY
#include "grabber_dummy.h"
#endif /* HAVE_GRABBER_DUMMY */
#ifdef HAVE_GRABBER_AMAZON
#include "grabber_amazon.h"
#endif /* HAVE_GRABBER_AMAZON */
#ifdef HAVE_GRABBER_TMDB
#include "grabber_tmdb.h"
#endif /* HAVE_GRABBER_TMDB */

struct grabber_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             run;
  pthread_mutex_t mutex_run;

  grabber_list_t *list;
  sem_t          *sem_grabber;
  pthread_mutex_t mutex_grabber;
};

/*
 * The first grabber is the prioritate.
 */
static grabber_list_t *(*g_grabber_register[]) (void) = {
#ifdef HAVE_GRABBER_DUMMY
  grabber_dummy_register,
#endif /* HAVE_GRABBER_DUMMY */
#ifdef HAVE_GRABBER_AMAZON
  grabber_amazon_register,
#endif /* HAVE_GRABBER_AMAZON */
#ifdef HAVE_GRABBER_TMDB
  grabber_tmdb_register,
#endif /* HAVE_GRABBER_TMDB */
  NULL
};


static inline int
grabber_is_stopped (grabber_t *grabber)
{
  int run;
  pthread_mutex_lock (&grabber->mutex_run);
  run = grabber->run;
  pthread_mutex_unlock (&grabber->mutex_run);
  return !run;
}

static int
file_type_supported (int caps_flag, valhalla_file_type_t type)
{
  switch (type)
  {
  case VALHALLA_FILE_TYPE_NULL:
    return 1;

  case VALHALLA_FILE_TYPE_AUDIO:
    if (caps_flag & GRABBER_CAP_AUDIO)
      return 1;

  case VALHALLA_FILE_TYPE_VIDEO:
    if (caps_flag & GRABBER_CAP_VIDEO)
      return 1;

  case VALHALLA_FILE_TYPE_IMAGE:
    if (caps_flag & GRABBER_CAP_IMAGE)
      return 1;

  default:
    return 0;
  }
}

#define GRABBER_IS_AVAILABLE                                  \
  grab = 0;                                                   \
  for (it = grabber->list, cnt = 0; it; it = it->next, cnt++) \
    if (cnt >= pdata->grabber_cnt                             \
        && file_type_supported (it->caps_flag, pdata->type))  \
    {                                                         \
      grab = 1;                                               \
      break;                                                  \
    }

static void *
grabber_thread (void *arg)
{
  int res;
  int e;
  int cnt, grab;
  void *data = NULL;
  file_data_t *pdata;
  grabber_t *grabber = arg;
  grabber_list_t *it;

  if (!grabber)
    pthread_exit (NULL);

  my_setpriority (grabber->priority);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = fifo_queue_pop (grabber->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    if (e == ACTION_DB_NEXT_LOOP)
    {
      for (it = grabber->list; it; it = it->next)
        if (it->loop)
          it->loop (it->priv);
      continue;
    }

    pdata = data;

    /*
     * Wait here until the grabbed data are inserted/updated in the
     * database by the dbmanager. It prevent a race condition on
     * pdata->meta_grabber.
     */
    if (pdata->wait)
    {
      /* sem_grabber provides a way to wake up the thread for "force_stop" */
      grabber->sem_grabber = &pdata->sem_grabber;
      pthread_mutex_unlock (&grabber->mutex_grabber);

      sem_wait (&pdata->sem_grabber);
      pdata->wait = 0;

      pthread_mutex_lock (&grabber->mutex_grabber);
      grabber->sem_grabber = NULL;

      if (grabber_is_stopped (grabber))
        break;
    }

    GRABBER_IS_AVAILABLE
    if (grab) /* next child available */
    {
      int res;

      pdata->grabber_cnt = cnt + 1;
      res = it->grab (it->priv, pdata);
      if (res)
        valhalla_log (VALHALLA_MSG_WARNING,
                      "[%s] grabbing failed (%i)", it->name, res);

      /* at least still one grabber for this file ? */
      GRABBER_IS_AVAILABLE
      if (!grab) /* no?, then next step */
        file_data_step_increase (pdata, &e);
      else
        file_data_step_continue (pdata, &e);
    }
    else /* next step, no grabber */
      file_data_step_increase (pdata, &e);

    dispatcher_action_send (grabber->valhalla->dispatcher, e, pdata);
  }
  while (!grabber_is_stopped (grabber));

  /* the thread is locked by grabber_run() */
  pthread_mutex_unlock (&grabber->mutex_grabber);

  pthread_exit (NULL);
}

int
grabber_run (grabber_t *grabber, int priority)
{
  int res = GRABBER_SUCCESS;
  pthread_attr_t attr;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return GRABBER_ERROR_HANDLER;

  grabber->priority = priority;
  grabber->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  pthread_mutex_lock (&grabber->mutex_grabber);
  res = pthread_create (&grabber->thread, &attr, grabber_thread, grabber);
  if (res)
  {
    res = GRABBER_ERROR_THREAD;
    grabber->run = 0;
    pthread_mutex_unlock (&grabber->mutex_grabber);
  }

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
grabber_fifo_get (grabber_t *grabber)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return NULL;

  return grabber->fifo;
}

void
grabber_stop (grabber_t *grabber)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  if (grabber_is_stopped (grabber))
    return;

  pthread_mutex_lock (&grabber->mutex_run);
  grabber->run = 0;
  pthread_mutex_unlock (&grabber->mutex_run);

  fifo_queue_push (grabber->fifo, ACTION_KILL_THREAD, NULL);

  /* wake up the thread if this is asleep by dbmanager */
  pthread_mutex_lock (&grabber->mutex_grabber);
  if (grabber->sem_grabber)
    sem_post (grabber->sem_grabber);
  pthread_mutex_unlock (&grabber->mutex_grabber);

  pthread_join (grabber->thread, NULL);
}

void
grabber_uninit (grabber_t *grabber)
{
  grabber_list_t *it;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  fifo_queue_free (grabber->fifo);
  pthread_mutex_destroy (&grabber->mutex_run);
  pthread_mutex_destroy (&grabber->mutex_grabber);

  /* uninit all childs */
  for (it = grabber->list; it; it = it->next)
    it->uninit (it->priv);

  for (it = grabber->list; it;)
  {
    grabber_list_t *tmp = it->next;
    free (it);
    it = tmp;
  }

  free (grabber);
}

static void
grabber_childs_add (grabber_list_t **list, grabber_list_t *child)
{
  if (!*list)
    *list = child;
  else
  {
    grabber_list_t *it;
    for (it = *list; it->next; it = it->next)
      ;
    it->next = child;
  }
}

/*
 * NOTE: the first grabber in the linked list is considered as the prioritate.
 *       grabber_childs_add() adds the grabber always at the end of the list.
 */
static grabber_list_t *
grabber_register_childs (void)
{
  grabber_list_t *list = NULL, *child;
  grabber_list_t *(**reg) (void);

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  for (reg = g_grabber_register; *reg; reg++)
  {
    child = (*reg) ();
    if (child)
      grabber_childs_add (&list, child);
  }

  return list;
}

grabber_t *
grabber_init (valhalla_t *handle)
{
  grabber_t *grabber;
  grabber_list_t *it;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  grabber = calloc (1, sizeof (grabber_t));
  if (!grabber)
    return NULL;

  pthread_mutex_init (&grabber->mutex_run, NULL);
  pthread_mutex_init (&grabber->mutex_grabber, NULL);

  grabber->fifo = fifo_queue_new ();
  if (!grabber->fifo)
    goto err;

  grabber->valhalla = handle;

  grabber->list = grabber_register_childs ();
  if (!grabber->list)
    goto err;

  /* init all childs */
  for (it = grabber->list; it; it = it->next)
  {
    int res = it->init (it->priv);
    if (res)
      goto err;
  }

  return grabber;

 err:
  grabber_uninit (grabber);
  return NULL;
}

void
grabber_action_send (grabber_t *grabber, int action, void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  fifo_queue_push (grabber->fifo, action, data);
}
