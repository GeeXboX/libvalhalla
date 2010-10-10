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
#include <sys/stat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "utils.h"
#include "osdep.h"
#include "fifo_queue.h"
#include "logs.h"
#include "stats.h"
#include "thread_utils.h"
#include "dbmanager.h"
#include "dispatcher.h"
#include "parser.h"
#include "scanner.h"
#include "ondemand.h"

#ifdef USE_GRABBER
#include "grabber.h"
#include "downloader.h"
#endif /* USE_GRABBER */

#define VH_HANDLE ondemand->valhalla

struct ondemand_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             wait;
  int             run;
  pthread_mutex_t mutex_run;

  vh_stats_cnt_t *st_cnt;
  vh_stats_tmr_t *st_tmr;
};

#define STATS_GROUP "ondemand"


static inline int
ondemand_is_stopped (ondemand_t *ondemand)
{
  int run;
  pthread_mutex_lock (&ondemand->mutex_run);
  run = ondemand->run;
  pthread_mutex_unlock (&ondemand->mutex_run);
  return !run;
}

static int
ondemand_cmp_fct (const void *tocmp, int id, const void *data)
{
  const char *file = tocmp;
  const file_data_t *fdata = data;

  if (!tocmp || !data)
    return -1;

  switch (id)
  {
  case ACTION_DB_INSERT_P:
  case ACTION_DB_INSERT_G:
  case ACTION_DB_UPDATE_P:
  case ACTION_DB_UPDATE_G:
  case ACTION_DB_END:
  case ACTION_DB_NEWFILE:
    return strcmp (file, fdata->file.path);

  default:
    return -1;
  }
}

static void *
ondemand_thread (void *arg)
{
  int res, tid;
  int e;
  unsigned int i = 0;
  void *data = NULL;
  char *file;
  file_data_t *fdata;
  ondemand_t *ondemand = arg;

  struct {
    void *handler;
    void (*fct_pause) (void *handler);
    fifo_queue_t *(*fct_fifo_get) (void *handler);
  } pause[] = {
    /*
     * Because the grabber can be in waiting on the dbmanager, it must be
     * the first to be sent in the pause mode. And in all cases, it is better
     * to keep the dbmanager at the end of this list.
     */
#ifdef USE_GRABBER
    { NULL, (void *) vh_grabber_pause,    (void *) vh_grabber_fifo_get    },
    { NULL, (void *) vh_downloader_pause, (void *) vh_downloader_fifo_get },
#endif /* USE_GRABBER */
    { NULL, (void *) vh_parser_pause,     (void *) vh_parser_fifo_get     },
    { NULL, (void *) vh_dispatcher_pause, (void *) vh_dispatcher_fifo_get },
    { NULL, (void *) vh_dbmanager_pause,  (void *) vh_dbmanager_fifo_get  },
  };

  if (!ondemand)
    pthread_exit (NULL);

  tid = vh_setpriority (ondemand->priority);

  vh_log (VALHALLA_MSG_VERBOSE,
          "[%s] tid: %i priority: %i", __FUNCTION__, tid, ondemand->priority);

#ifdef USE_GRABBER
  pause[i++].handler = VH_HANDLE->grabber;
  pause[i++].handler = VH_HANDLE->downloader;
#endif /* USE_GRABBER */
  pause[i++].handler = VH_HANDLE->parser;
  pause[i++].handler = VH_HANDLE->dispatcher;
  pause[i++].handler = VH_HANDLE->dbmanager;

  do
  {
    int id = 0;
    struct stat st;
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = vh_fifo_queue_pop (ondemand->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    if (e != ACTION_OD_ENGAGE)
      continue;

    fdata = NULL;
    file  = data;

    if (lstat (file, &st))
    {
      vh_log (VALHALLA_MSG_WARNING,
              "[%s] File %s unavailable", __FUNCTION__, file);
      free (file);
      continue;
    }

    /*
     * Ignore the ondemand query if the file is already fully available.
     * The ENDED event is sent by this function if the return value is 1.
     */
    if (vh_dbmanager_file_complete (VH_HANDLE->dbmanager,
                                    file, (int64_t) st.st_mtime))
    {
      free (file);
      continue;
    }

    VH_STATS_COUNTER_INC (ondemand->st_cnt);
    VH_STATS_TIMER_START (ondemand->st_tmr);

    /*
     * On-demand engaged, all threads must be in pause mode.
     * After this loop, we are sure that "all" threads are in waiting list.
     */
    for (i = 0; i < ARRAY_NB_ELEMENTS (pause); i++)
      pause[i].fct_pause (pause[i].handler);

    /*
     * Maybe the file for "on-demand" is already in a queue, we must check
     * all queues in order to change its priority.
     */
    for (i = 0; i < ARRAY_NB_ELEMENTS (pause) && !fdata; i++)
    {
      fifo_queue_t *queue = pause[i].fct_fifo_get (pause[i].handler);
      fdata = vh_fifo_queue_search (queue, &id, file, ondemand_cmp_fct);
    }

    /* Already in queues? */
    if (fdata)
    {
      if (id != ACTION_DB_END)
      {
        fdata->priority = FIFO_QUEUE_PRIORITY_HIGH;

        /* Move up this entry in the queues. */
        for (i = 0; i < ARRAY_NB_ELEMENTS (pause); i++)
        {
          fifo_queue_t *queue = pause[i].fct_fifo_get (pause[i].handler);
          vh_fifo_queue_moveup (queue, file, ondemand_cmp_fct);
        }
      }
      if (fdata->od == OD_TYPE_DEF)
        fdata->od = OD_TYPE_UPD;
    }
    /* Check if the file is available and consistent. */
    else if (S_ISREG (st.st_mode)
             && !vh_scanner_suffix_cmp (VH_HANDLE->scanner, file))
    {
      int outofpath = !!vh_scanner_path_cmp (VH_HANDLE->scanner, file);

      fdata = vh_file_data_new (file, &st, outofpath, OD_TYPE_NEW,
                                FIFO_QUEUE_PRIORITY_HIGH, STEP_PARSING);
      if (fdata)
        vh_dbmanager_action_send (VH_HANDLE->dbmanager,
                                  fdata->priority, ACTION_DB_NEWFILE, fdata);
    }
    else
      vh_log (VALHALLA_MSG_WARNING,
              "[%s] File %s unsupported", __FUNCTION__, file);

    free (file);

    /* Wake up threads */
    for (i = 0; i < ARRAY_NB_ELEMENTS (pause); i++)
      pause[i].fct_pause (pause[i].handler);

    VH_STATS_TIMER_STOP (ondemand->st_tmr);
  }
  while (!ondemand_is_stopped (ondemand));

  pthread_exit (NULL);
}

int
vh_ondemand_run (ondemand_t *ondemand, int priority)
{
  int res = ONDEMAND_SUCCESS;
  pthread_attr_t attr;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ondemand)
    return ONDEMAND_ERROR_HANDLER;

  ondemand->priority = priority;
  ondemand->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = pthread_create (&ondemand->thread, &attr, ondemand_thread, ondemand);
  if (res)
  {
    res = ONDEMAND_ERROR_THREAD;
    ondemand->run = 0;
  }

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
vh_ondemand_fifo_get (ondemand_t *ondemand)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ondemand)
    return NULL;

  return ondemand->fifo;
}

void
vh_ondemand_stop (ondemand_t *ondemand, int f)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ondemand)
    return;

  if (f & STOP_FLAG_REQUEST && !ondemand_is_stopped (ondemand))
  {
    pthread_mutex_lock (&ondemand->mutex_run);
    ondemand->run = 0;
    pthread_mutex_unlock (&ondemand->mutex_run);

    vh_fifo_queue_push (ondemand->fifo,
                        FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
    ondemand->wait = 1;
  }

  if (f & STOP_FLAG_WAIT && ondemand->wait)
  {
    pthread_join (ondemand->thread, NULL);
    ondemand->wait = 0;
  }
}

void
vh_ondemand_uninit (ondemand_t *ondemand)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ondemand)
    return;

  vh_fifo_queue_free (ondemand->fifo);
  pthread_mutex_destroy (&ondemand->mutex_run);

  free (ondemand);
}

static void
ondemand_stats_dump (vh_stats_t *stats, void *data)
{
  ondemand_t *ondemand = data;
  uint64_t total;
  float time;

  if (!stats || !ondemand)
    return;

  vh_log (VALHALLA_MSG_INFO,
          "==================================================");
  vh_log (VALHALLA_MSG_INFO, "Statistics dump (" STATS_GROUP ")");
  vh_log (VALHALLA_MSG_INFO,
          "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  total = vh_stats_counter_read (ondemand->st_cnt);
  time  = vh_stats_timer_read (ondemand->st_tmr) / 1000000000.0;
  vh_log (VALHALLA_MSG_INFO,
          "Queries    | %6"PRIu64"  %7.2f sec  %7.2f sec/file",
          total, time, total ? time / total : 0.0);
}

ondemand_t *
vh_ondemand_init (valhalla_t *handle)
{
  ondemand_t *ondemand;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  ondemand = calloc (1, sizeof (ondemand_t));
  if (!ondemand)
    return NULL;

  ondemand->fifo = vh_fifo_queue_new ();
  if (!ondemand->fifo)
    goto err;

  ondemand->valhalla = handle; /* VH_HANDLE */

  pthread_mutex_init (&ondemand->mutex_run, NULL);

  /* init statistics */
  vh_stats_grp_add (handle->stats, STATS_GROUP, ondemand_stats_dump, ondemand);
  ondemand->st_cnt =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_GROUP, NULL);
  ondemand->st_tmr =
    vh_stats_grp_timer_add (handle->stats, STATS_GROUP, STATS_GROUP, NULL);

  return ondemand;

 err:
  vh_ondemand_uninit (ondemand);
  return NULL;
}

void
vh_ondemand_action_send (ondemand_t *ondemand,
                         fifo_queue_prio_t prio, int action, void *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!ondemand)
    return;

  vh_fifo_queue_push (ondemand->fifo, prio, action, data);
}
