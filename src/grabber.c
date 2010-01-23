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
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
#ifdef HAVE_GRABBER_ALLOCINE
#include "grabber_allocine.h"
#endif /* HAVE_GRABBER_ALLOCINE */
#ifdef HAVE_GRABBER_AMAZON
#include "grabber_amazon.h"
#endif /* HAVE_GRABBER_AMAZON */
#ifdef HAVE_GRABBER_EXIF
#include "grabber_exif.h"
#endif /* HAVE_GRABBER_EXIF */
#ifdef HAVE_GRABBER_FFMPEG
#include "grabber_ffmpeg.h"
#endif /* HAVE_GRABBER_FFMPEG */
#ifdef HAVE_GRABBER_IMDB
#include "grabber_imdb.h"
#endif /* HAVE_GRABBER_IMDB */
#ifdef HAVE_GRABBER_LASTFM
#include "grabber_lastfm.h"
#endif /* HAVE_GRABBER_LASTFM */
#ifdef HAVE_GRABBER_LOCAL
#include "grabber_local.h"
#endif /* HAVE_GRABBER_LOCAL */
#ifdef HAVE_GRABBER_LYRICSFLY
#include "grabber_lyricsfly.h"
#endif /* HAVE_GRABBER_LYRICSFLY */
#ifdef HAVE_GRABBER_LYRICWIKI
#include "grabber_lyricwiki.h"
#endif /* HAVE_GRABBER_LYRICWIKI */
#ifdef HAVE_GRABBER_NFO
#include "grabber_nfo.h"
#endif /* HAVE_GRABBER_NFO */
#ifdef HAVE_GRABBER_TMDB
#include "grabber_tmdb.h"
#endif /* HAVE_GRABBER_TMDB */
#ifdef HAVE_GRABBER_TVDB
#include "grabber_tvdb.h"
#endif /* HAVE_GRABBER_TVDB */
#ifdef HAVE_GRABBER_TVRAGE
#include "grabber_tvrage.h"
#endif /* HAVE_GRABBER_TVRAGE */

struct grabber_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             wait;
  int             run;
  pthread_mutex_t mutex_run;

  VH_THREAD_PAUSE_ATTRS

  grabber_list_t *list;
  sem_t          *sem_grabber;
  pthread_mutex_t mutex_grabber;
};

/*
 * The first grabber has the highest priority.
 */
static grabber_list_t *(*g_grabber_register[]) (void) = {
#ifdef HAVE_GRABBER_DUMMY
  vh_grabber_dummy_register,
#endif /* HAVE_GRABBER_DUMMY */
#ifdef HAVE_GRABBER_FFMPEG
  vh_grabber_ffmpeg_register,
#endif /* HAVE_GRABBER_FFMPEG */
#ifdef HAVE_GRABBER_LOCAL
  vh_grabber_local_register,
#endif /* HAVE_GRABBER_LOCAL */
#ifdef HAVE_GRABBER_NFO
  vh_grabber_nfo_register,
#endif /* HAVE_GRABBER_NFO */
#ifdef HAVE_GRABBER_TMDB
  vh_grabber_tmdb_register,
#endif /* HAVE_GRABBER_TMDB */
#ifdef HAVE_GRABBER_AMAZON
  vh_grabber_amazon_register,
#endif /* HAVE_GRABBER_AMAZON */
#ifdef HAVE_GRABBER_LASTFM
  vh_grabber_lastfm_register,
#endif /* HAVE_GRABBER_LASTFM */
#ifdef HAVE_GRABBER_EXIF
  vh_grabber_exif_register,
#endif /* HAVE_GRABBER_EXIF */
#ifdef HAVE_GRABBER_LYRICSFLY
  vh_grabber_lyricsfly_register,
#endif /* HAVE_GRABBER_LYRICSFLY */
#ifdef HAVE_GRABBER_LYRICWIKI
  vh_grabber_lyricwiki_register,
#endif /* HAVE_GRABBER_LYRICWIKI */
#ifdef HAVE_GRABBER_TVDB
  vh_grabber_tvdb_register,
#endif /* HAVE_GRABBER_TVDB */
#ifdef HAVE_GRABBER_TVRAGE
  vh_grabber_tvrage_register,
#endif /* HAVE_GRABBER_TVRAGE */
#ifdef HAVE_GRABBER_IMDB
  vh_grabber_imdb_register,
#endif /* HAVE_GRABBER_IMDB */
#ifdef HAVE_GRABBER_ALLOCINE
  vh_grabber_allocine_register,
#endif /* HAVE_GRABBER_ALLOCINE */
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

static inline int
file_type_supported (int caps_flag, valhalla_file_type_t type)
{
  switch (type)
  {
  case VALHALLA_FILE_TYPE_NULL:
    return 1;

  case VALHALLA_FILE_TYPE_AUDIO:
    if (caps_flag & GRABBER_CAP_AUDIO)
      return 1;
    return 0;

  case VALHALLA_FILE_TYPE_VIDEO:
    if (caps_flag & GRABBER_CAP_VIDEO)
      return 1;
    return 0;

  case VALHALLA_FILE_TYPE_IMAGE:
    if (caps_flag & GRABBER_CAP_IMAGE)
      return 1;
    return 0;

  default:
    return 0;
  }
}

static int
grabber_cmp_fct (const void *tocmp, const void *data)
{
  if (!tocmp || !data)
    return -1;

  return strcmp (tocmp, data);
}

#define GRABBER_IS_AVAILABLE                                                 \
  grab = 0;                                                                  \
  for (it = grabber->list, cnt = 0; it; it = it->next, cnt++)                \
    if (it->enable && cnt >= pdata->grabber_cnt                              \
        && file_type_supported (it->caps_flag, pdata->type)                  \
        && !vh_list_search (pdata->grabber_list, it->name, grabber_cmp_fct)) \
    {                                                                        \
      grab = 1;                                                              \
      break;                                                                 \
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

  vh_setpriority (grabber->priority);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = vh_fifo_queue_pop (grabber->fifo, &e, &data);
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

    if (e == ACTION_PAUSE_THREAD)
    {
      VH_THREAD_PAUSE_ACTION (grabber)
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
      struct timespec ts, te, td;

      pdata->grabber_cnt = cnt + 1;
      pdata->grabber_name = it->name;
      clock_gettime (CLOCK_REALTIME, &ts);
      res = it->grab (it->priv, pdata);
      clock_gettime (CLOCK_REALTIME, &te);
      if (res)
      {
        it->stat_failure++;
        valhalla_log (VALHALLA_MSG_VERBOSE,
                      "[%s] grabbing failed (%i): %s",
                      it->name, res, pdata->file);
      }
      else
        it->stat_success++;

      VH_TIMERSUB (&te, &ts, &td);
      VH_TIMERADD (&it->stat_difftime, &td, &it->stat_difftime);

      /* at least still one grabber for this file ? */
      GRABBER_IS_AVAILABLE
      if (!grab) /* no?, then next step */
        vh_file_data_step_increase (pdata, &e);
      else
        vh_file_data_step_continue (pdata, &e);
    }
    else /* next step, no grabber */
      vh_file_data_step_increase (pdata, &e);

    vh_dispatcher_action_send (grabber->valhalla->dispatcher,
                               pdata->priority, e, pdata);
  }
  while (!grabber_is_stopped (grabber));

  /* Statistics */
  if (vh_log_test (VALHALLA_MSG_INFO))
    for (it = grabber->list; it; it = it->next)
    {
      unsigned int total = it->stat_success + it->stat_failure;
      float diff_time =
        it->stat_difftime.tv_sec + it->stat_difftime.tv_nsec / 1000000000.0;

      valhalla_log (VALHALLA_MSG_INFO,
                    "[%s] Stats %-10s : %6i/%-6i (%6.2f%%) "
                    "(%7.2f sec, %7.2f sec/file)",
                    __FUNCTION__, it->name, it->stat_success, total,
                    total ? 100.0 * it->stat_success / total : 100.0,
                    diff_time, total ? diff_time / total : 0.0);
    }

  /* the thread is locked by vh_grabber_run() */
  pthread_mutex_unlock (&grabber->mutex_grabber);

  pthread_exit (NULL);
}

int
vh_grabber_run (grabber_t *grabber, int priority)
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
  pthread_attr_setscope (&attr, VH_THREAD_SCOPE);

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
vh_grabber_fifo_get (grabber_t *grabber)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return NULL;

  return grabber->fifo;
}

void
vh_grabber_state_set (grabber_t *grabber, const char *id, int enable)
{
  grabber_list_t *it;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  for (it = grabber->list; it; it = it->next)
    if (!strcmp (it->name, id))
    {
      it->enable = enable;
      break;
    }
}

const char *
vh_grabber_list_get (grabber_t *grabber, const char *id)
{
  grabber_list_t *it;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return NULL;

  if (!id) /* first grabber? */
    return grabber->list ? grabber->list->name : NULL;

  for (it = grabber->list; it; it = it->next)
    if (!strcmp (it->name, id))
      break;

  if (!it)
    return NULL;

  it = it->next;
  return it ? it->name : NULL;
}

void
vh_grabber_pause (grabber_t *grabber)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  VH_THREAD_PAUSE_FCT (grabber, 1)
}

void
vh_grabber_stop (grabber_t *grabber, int f)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  if (f & STOP_FLAG_REQUEST && !grabber_is_stopped (grabber))
  {
    pthread_mutex_lock (&grabber->mutex_run);
    grabber->run = 0;
    pthread_mutex_unlock (&grabber->mutex_run);

    vh_fifo_queue_push (grabber->fifo,
                        FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
    grabber->wait = 1;

    /* wake up the thread if this is asleep by dbmanager */
    pthread_mutex_lock (&grabber->mutex_grabber);
    if (grabber->sem_grabber)
      sem_post (grabber->sem_grabber);
    pthread_mutex_unlock (&grabber->mutex_grabber);
  }

  if (f & STOP_FLAG_WAIT && grabber->wait)
  {
    pthread_join (grabber->thread, NULL);
    grabber->wait = 0;
  }
}

void
vh_grabber_uninit (grabber_t *grabber)
{
  grabber_list_t *it;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  vh_fifo_queue_free (grabber->fifo);
  pthread_mutex_destroy (&grabber->mutex_run);
  pthread_mutex_destroy (&grabber->mutex_grabber);
  VH_THREAD_PAUSE_UNINIT (grabber)

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
 * NOTE: the first grabber in the linked list is considered as top priority.
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
vh_grabber_init (valhalla_t *handle)
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
  VH_THREAD_PAUSE_INIT (grabber)

  grabber->fifo = vh_fifo_queue_new ();
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
  vh_grabber_uninit (grabber);
  return NULL;
}

void
vh_grabber_action_send (grabber_t *grabber,
                        fifo_queue_prio_t prio, int action, void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  vh_fifo_queue_push (grabber->fifo, prio, action, data);
}
