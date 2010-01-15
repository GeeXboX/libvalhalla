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
#include "stats.h"
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

#ifndef GRABBER_NB_MAX
#define GRABBER_NB_MAX 16
#endif /* GRABBER_NB_MAX */

#define VH_HANDLE grabber->valhalla

struct grabber_s {
  valhalla_t   *valhalla;
  pthread_t     thread[GRABBER_NB_MAX];
  fifo_queue_t *fifo;
  unsigned int  nb;
  int           priority;

  int             wait;
  int             run;
  unsigned int    run_id;
  pthread_mutex_t mutex_run;

  VH_THREAD_PAUSE_ATTRS

  grabber_list_t *list;
  sem_t          *sem_grabber[GRABBER_NB_MAX];
  pthread_mutex_t mutex_grabber[GRABBER_NB_MAX];
};

#define STATS_GROUP   "grabber"
#define STATS_SUCCESS "success"
#define STATS_FAILURE "failure"


/*
 * In the case where concurrent grabbers are used, the following order is
 * prefered but not fully respected. Nevertheless, the priorities for the
 * metadata are not affected by the order of this list.
 */
static grabber_list_t *(*const g_grabber_register[]) (void) = {
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

#define GRABBER_IF_TEST(it, data)                                         \
  if (it->enable                                                          \
      && file_type_supported (it->caps_flag, data->file.type)             \
      && !vh_list_search (data->grabber_list, it->name, grabber_cmp_fct))

#define GRABBER_IS_AVAILABLE(it, list, data)  \
  grab = 0;                                   \
  for (it = list; it; it = it->next)          \
    GRABBER_IF_TEST (it, data)                \
    {                                         \
      grab = 1;                               \
      break;                                  \
    }

#define GRABBER_LOCK_TIMEOUT_S  0
#define GRABBER_LOCK_TIMEOUT_NS 200000000 /* 200 ms */

#define grabber_unlock(g) pthread_mutex_unlock (&g->mutex)

/*
 * This function tries to catch the first grabber available and compatible
 * with the 'type' of the file. The first pass uses the full speed. If the
 * loop reaches the last grabber and this one is not available or compatible,
 * the loop is restarted on the first interesting grabber. And to prevent
 * a useless use of the CPU, this second pass uses a timeout of
 * GRABBER_LOCK_TIMEOUT_* between every grabber.
 * If after those loops it fails, then the function returns NULL, and the data
 * are sent to the dispatcher for a new attempt in the future. This
 * functionality allows to an other fdata to have a chance. It avoids that
 * a slow grabber with many files, to monopolize all threads by locking.
 */
static inline grabber_list_t *
grabber_lock (grabber_list_t *list, file_data_t *fdata)
{
  const struct timespec ta = {
    .tv_sec  = GRABBER_LOCK_TIMEOUT_S,
    .tv_nsec = GRABBER_LOCK_TIMEOUT_NS,
  };
  struct timespec ts;
  int step = 0;
  grabber_list_t *it, *fskip = NULL;

  fdata->skip = 0;
  for (it = list; it;)
  {
    GRABBER_IF_TEST (it, fdata)
    {
      int rc;

      switch (step)
      {
      case 0: /* first pass */
        rc = pthread_mutex_trylock (&it->mutex);
        break;

      case 1: /* second pass */
        vh_clock_gettime (CLOCK_REALTIME, &ts);
        VH_TIMERADD (&ts, &ta, &ts);
        rc = pthread_mutex_timedlock (&it->mutex, &ts);
        break;

      default: /* give the chance to an other */
        fdata->skip = 1;
        return NULL;
      }

      if (!rc)
        return it; /* locked */

      if (!fskip)
        fskip = it;
    }

    if (it->next)
      it = it->next;
    else
    {
      it = fskip;
      step++;
    }
  }

  return NULL;
}

static void *
grabber_thread (void *arg)
{
  int res, tid;
  int e;
  int grab;
  unsigned int id;
  void *data = NULL;
  file_data_t *pdata;
  grabber_t *grabber = arg;
  grabber_list_t *it;

  if (!grabber)
    pthread_exit (NULL);

  pthread_mutex_lock (&grabber->mutex_run);
  id = grabber->run_id++;
  pthread_mutex_unlock (&grabber->mutex_run);

  tid = vh_setpriority (grabber->priority);

  vh_log (VALHALLA_MSG_VERBOSE,
          "[%s] tid: %i priority: %i", __FUNCTION__, tid, grabber->priority);

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
      {
        if (!it->loop)
          continue;

        pthread_mutex_lock (&it->mutex);
        it->loop (it->priv);
        pthread_mutex_unlock (&it->mutex);
      }

      vh_stats_dump (VH_HANDLE->stats, STATS_GROUP);
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
      vh_log (VALHALLA_MSG_VERBOSE,
              "[%s] waiting grabbing: %s", __FUNCTION__, pdata->file.path);

      /* sem_grabber provides a way to wake up the thread for "force_stop" */
      grabber->sem_grabber[id] = &pdata->sem_grabber;
      pthread_mutex_unlock (&grabber->mutex_grabber[id]);

      sem_wait (&pdata->sem_grabber);
      pdata->wait = 0;

      pthread_mutex_lock (&grabber->mutex_grabber[id]);
      grabber->sem_grabber[id] = NULL;

      if (grabber_is_stopped (grabber))
        break;
    }

    it = grabber_lock (grabber->list, pdata);
    if (it) /* one grabber available */
    {
      int res;

      pdata->grabber_name = it->name;
      VH_STATS_TIMER_START (it->tmr);
      res = it->grab (it->priv, pdata);
      VH_STATS_TIMER_STOP (it->tmr);
      grabber_unlock (it);
      if (res)
      {
        VH_STATS_COUNTER_INC (it->cnt_failure);
        vh_log (VALHALLA_MSG_VERBOSE,
                "[%s] grabbing failed (%i): %s",
                it->name, res, pdata->file.path);
      }
      else
        VH_STATS_COUNTER_INC (it->cnt_success);

      vh_list_append (pdata->grabber_list, it->name, strlen (it->name) + 1);
    }

    /* at least still one grabber for this file ? */
    GRABBER_IS_AVAILABLE (it, grabber->list, pdata)
    if (!grab) /* no?, then next step */
      vh_file_data_step_increase (pdata, &e);
    else
      vh_file_data_step_continue (pdata, &e);

    vh_log (VALHALLA_MSG_VERBOSE, "[%s] %s grabbing: %s",
            __FUNCTION__, grab ? "continue" : "finished", pdata->file.path);

    vh_dispatcher_action_send (VH_HANDLE->dispatcher,
                               pdata->priority, e, pdata);
  }
  while (!grabber_is_stopped (grabber));

  /* the thread is locked by vh_grabber_run() */
  pthread_mutex_unlock (&grabber->mutex_grabber[id]);

  pthread_exit (NULL);
}

int
vh_grabber_run (grabber_t *grabber, int priority)
{
  int res = GRABBER_SUCCESS;
  unsigned int i;
  pthread_attr_t attr;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return GRABBER_ERROR_HANDLER;

  grabber->priority = priority;
  grabber->run      = 1;
  grabber->run_id   = 0;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  for (i = 0; i < grabber->nb; i++)
  {
    pthread_mutex_lock (&grabber->mutex_grabber[i]);
    res = pthread_create (&grabber->thread[i], &attr, grabber_thread, grabber);
    if (res)
    {
      res = GRABBER_ERROR_THREAD;
      grabber->run = 0;
      break;
    }
  }

  if (res)
    for (i++; i > 0; i--)
      pthread_mutex_unlock (&grabber->mutex_grabber[i - 1]);

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
vh_grabber_fifo_get (grabber_t *grabber)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return NULL;

  return grabber->fifo;
}

valhalla_grabber_pl_t
vh_grabber_priority_read (grabber_t *grabber,
                          const char *id, const char **metadata)
{
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!id)
    return 0;

  for (it = grabber->list; it; it = it->next)
    if (!strcmp (id, it->name))
      return vh_metadata_plist_read (it->pl, metadata);

  return 0;
}

void
vh_grabber_priority_set (grabber_t *grabber, const char *id,
                         valhalla_grabber_pl_t p, const char *metadata)
{
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  for (it = grabber->list; it; it = it->next)
    if (!id || !strcmp (it->name, id))
    {
      vh_metadata_plist_set (&it->pl, metadata, p);
      vh_log (VALHALLA_MSG_VERBOSE, "Metadata priorities (%s) :", it->name);
      vh_metadata_plist_dump (it->pl);

      if (id)
        break;
    }
}

void
vh_grabber_state_set (grabber_t *grabber, const char *id, int enable)
{
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
vh_grabber_next (grabber_t *grabber, const char *id)
{
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  VH_THREAD_PAUSE_FCT (grabber, grabber->nb)
}

void
vh_grabber_stop (grabber_t *grabber, int f)
{
  unsigned int i;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  if (f & STOP_FLAG_REQUEST && !grabber_is_stopped (grabber))
  {
    pthread_mutex_lock (&grabber->mutex_run);
    grabber->run = 0;
    pthread_mutex_unlock (&grabber->mutex_run);

    for (i = 0; i < grabber->nb; i++)
      vh_fifo_queue_push (grabber->fifo,
                          FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);

    grabber->wait = 1;

    VH_THREAD_PAUSE_FORCESTOP (grabber, grabber->nb)

    for (i = 0; i < grabber->nb; i++)
    {
      /* wake up the thread if this is asleep by dbmanager */
      pthread_mutex_lock (&grabber->mutex_grabber[i]);
      if (grabber->sem_grabber[i])
        sem_post (grabber->sem_grabber[i]);
      pthread_mutex_unlock (&grabber->mutex_grabber[i]);
    }
  }

  if (f & STOP_FLAG_WAIT && grabber->wait)
  {
    for (i = 0; i < grabber->nb; i++)
      pthread_join (grabber->thread[i], NULL);
    grabber->wait = 0;
  }
}

void
vh_grabber_uninit (grabber_t *grabber)
{
  unsigned int i;
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  vh_fifo_queue_free (grabber->fifo);
  pthread_mutex_destroy (&grabber->mutex_run);
  for (i = 0; i < grabber->nb; i++)
    pthread_mutex_destroy (&grabber->mutex_grabber[i]);
  VH_THREAD_PAUSE_UNINIT (grabber)

  /* uninit all childs */
  for (it = grabber->list; it; it = it->next)
    it->uninit (it->priv);

  for (it = grabber->list; it;)
  {
    grabber_list_t *tmp = it->next;
    pthread_mutex_destroy (&it->mutex);
    free (it->pl);
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
 * NOTE: the first grabber in the linked list is considered as the favorite.
 *       grabber_childs_add() adds the grabber always at the end of the list.
 */
static grabber_list_t *
grabber_register_childs (void)
{
  grabber_list_t *list = NULL, *child;
  grabber_list_t *(*const *reg) (void);

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  for (reg = g_grabber_register; *reg; reg++)
  {
    child = (*reg) ();
    if (child)
      grabber_childs_add (&list, child);
  }

  return list;
}

#define STATS_DUMP(name, success, total, time)                      \
  vh_log (VALHALLA_MSG_INFO,                                        \
          "%-10s | %6lu/%-6lu (%6.2f%%) %7.2f sec  %7.2f sec/file", \
          name, success, total,                                     \
          (total) ? 100.0 * (success) / (total) : 100.0,            \
          time, (total) ? (time) / (total) : 0.0)

static void
grabber_stats_dump (vh_stats_t *stats, void *data)
{
  grabber_t *grabber = data;
  grabber_list_t *it;
  float time_all = 0.0;
  unsigned long int total_all = 0, success_all = 0;

  if (!stats || !grabber)
    return;

  vh_log (VALHALLA_MSG_INFO,
          "==================================================================");
  vh_log (VALHALLA_MSG_INFO, "Statistics dump (" STATS_GROUP ")");
  vh_log (VALHALLA_MSG_INFO,
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  for (it = grabber->list; it; it = it->next)
  {
    float time;
    unsigned long int success, failure, total;

    time    = vh_stats_timer_read (it->tmr) / 1000000000.0;
    success = vh_stats_counter_read (it->cnt_success);
    failure = vh_stats_counter_read (it->cnt_failure);
    total   = success + failure;

    time_all    += time;
    success_all += success;
    total_all   += total;

    STATS_DUMP (it->name, success, total, time);
  }

  vh_log (VALHALLA_MSG_INFO,
          "~~~~~~~~~~ | ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
#if 0
  STATS_DUMP ("GLOBAL", success_all, total_all, total_all);
#else
  vh_log (VALHALLA_MSG_INFO,
          "%-10s | %6lu/%-6lu (%6.2f%%)",
          "GLOBAL", success_all, total_all,
          (total_all) ? 100.0 * (success_all) / (total_all) : 100);
#endif /* 0 */
}

grabber_t *
vh_grabber_init (valhalla_t *handle, unsigned int nb)
{
  unsigned int i;
  grabber_t *grabber;
  grabber_list_t *it;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  grabber = calloc (1, sizeof (grabber_t));
  if (!grabber)
    return NULL;

  if (nb > ARRAY_NB_ELEMENTS (grabber->thread))
    goto err;

  pthread_mutex_init (&grabber->mutex_run, NULL);
  for (i = 0; i < nb; i++)
    pthread_mutex_init (&grabber->mutex_grabber[i], NULL);
  VH_THREAD_PAUSE_INIT (grabber)

  grabber->fifo = vh_fifo_queue_new ();
  if (!grabber->fifo)
    goto err;

  grabber->valhalla = handle; /* VH_HANDLE */
  grabber->nb       = nb ? nb : GRABBER_NUMBER_DEF;

  grabber->list = grabber_register_childs ();
  if (!grabber->list)
    goto err;

  vh_stats_grp_add (handle->stats, STATS_GROUP, grabber_stats_dump, grabber);

  /* init all childs */
  for (it = grabber->list; it; it = it->next)
  {
    const char *name = it->name;
    int res = it->init (it->priv, it->pl);
    if (res)
      goto err;

    /* init statistics */
    it->tmr = vh_stats_grp_timer_add (handle->stats, STATS_GROUP, name, NULL);
    it->cnt_success =
      vh_stats_grp_counter_add (handle->stats,
                                STATS_GROUP, name, STATS_SUCCESS);
    it->cnt_failure =
      vh_stats_grp_counter_add (handle->stats,
                                STATS_GROUP, name, STATS_FAILURE);
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
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!grabber)
    return;

  vh_fifo_queue_push (grabber->fifo, prio, action, data);
}
