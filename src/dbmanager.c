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
#include <inttypes.h>
#include <unistd.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "fifo_queue.h"
#include "logs.h"
#include "stats.h"
#include "thread_utils.h"
#include "event_handler.h"
#include "database.h"
#include "scanner.h"
#include "dbmanager.h"
#include "dispatcher.h"

struct dbmanager_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             wait;
  int             run;
  pthread_mutex_t mutex_run;

  VH_THREAD_PAUSE_ATTRS

  database_t   *database;
  unsigned int  commit_int;

  vh_stats_cnt_t *st_insert;
  vh_stats_cnt_t *st_update;
  vh_stats_cnt_t *st_delete;
  vh_stats_cnt_t *st_nochange;
  vh_stats_cnt_t *st_cleanup;
};

#define STATS_GROUP     "dbmanager"
#define STATS_INSERT    "insert"
#define STATS_UPDATE    "update"
#define STATS_DELETE    "delete"
#define STATS_NOCHANGE  "nochange"
#define STATS_CLEANUP   "cleanup"


static inline int
dbmanager_is_stopped (dbmanager_t *dbmanager)
{
  int run;
  pthread_mutex_lock (&dbmanager->mutex_run);
  run = dbmanager->run;
  pthread_mutex_unlock (&dbmanager->mutex_run);
  return !run;
}

void
vh_dbmanager_extmd_free (dbmanager_extmd_t *extmd)
{
  if (!extmd)
    return;

  if (extmd->path)
    free (extmd->path);
  if (extmd->meta)
    free (extmd->meta);
  if (extmd->data)
    free (extmd->data);
  if (extmd->ndata)
    free (extmd->ndata);
  free (extmd);
}

static int
dbmanager_queue (dbmanager_t *dbmanager)
{
  int res;
  int e;
  int grab = 0;
  void *data = NULL;
  file_data_t *pdata;

  do
  {
    int interval;
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = vh_fifo_queue_pop (dbmanager->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      goto out;

    if (e == ACTION_DB_NEXT_LOOP)
    {
      vh_dispatcher_action_send (dbmanager->valhalla->dispatcher,
                                 FIFO_QUEUE_PRIORITY_NORMAL, e, NULL);
      goto out;
    }

    /* External Metadata Handling */
    switch (e)
    {
    default:
      break;

    case ACTION_DB_EXT_INSERT:
    {
      dbmanager_extmd_t *extmd = data;

      if (!extmd)
        continue;

      vh_database_metadata_insert (dbmanager->database, extmd->path,
                                   extmd->meta, extmd->data, extmd->group);
      vh_dbmanager_extmd_free (extmd);
      continue;
    }

    case ACTION_DB_EXT_UPDATE:
    {
      dbmanager_extmd_t *extmd = data;

      if (!extmd)
        continue;

      vh_database_metadata_update (dbmanager->database, extmd->path,
                                   extmd->meta, extmd->data, extmd->ndata);
      vh_dbmanager_extmd_free (extmd);
      continue;
    }

    case ACTION_DB_EXT_DELETE:
    {
      dbmanager_extmd_t *extmd = data;

      if (!extmd)
        continue;

      vh_database_metadata_delete (dbmanager->database, extmd->path,
                                   extmd->meta, extmd->data);
      vh_dbmanager_extmd_free (extmd);
      continue;
    }
    }

    /* Manage BEGIN / COMMIT transactions */
    interval  = (int) vh_stats_counter_read (dbmanager->st_insert);
    interval += (int) vh_stats_counter_read (dbmanager->st_update);
    interval += grab;
    vh_database_step_transaction (dbmanager->database,
                                  dbmanager->commit_int, interval);

    pdata = data;

    switch (e)
    {
    default:
      break;

    case ACTION_PAUSE_THREAD:
      VH_THREAD_PAUSE_ACTION (dbmanager)
      continue;

    /* received from the dispatcher */
    case ACTION_DB_END:
      vh_database_file_interrupted_clear (dbmanager->database,
                                          pdata->file.path);
      if (pdata->od != OD_TYPE_DEF)
        vh_event_handler_od_send (dbmanager->valhalla->event_handler,
                                  pdata->file.path,
                                  VALHALLA_EVENTOD_ENDED, NULL);
      break;

    /* received from the dispatcher (grabbed data) */
    case ACTION_DB_INSERT_G:
      vh_database_file_grab_insert (dbmanager->database, pdata);
    case ACTION_DB_UPDATE_G:
    {
      int res;

      if (e == ACTION_DB_UPDATE_G)
        vh_database_file_grab_update (dbmanager->database, pdata);

      if (pdata->od != OD_TYPE_DEF)
        vh_event_handler_od_send (dbmanager->valhalla->event_handler,
                                  pdata->file.path, VALHALLA_EVENTOD_GRABBED,
                                  pdata->grabber_name);
      res = vh_event_handler_md_send (dbmanager->valhalla->event_handler,
                                      VALHALLA_EVENTMD_GRABBER,
                                      pdata->grabber_name, &pdata->file,
                                      pdata->meta_grabber);
      if (res)
        vh_metadata_free (pdata->meta_grabber);
      pdata->meta_grabber = NULL;

      if (pdata->wait)
        sem_post (&pdata->sem_grabber);

      grab++;
      continue;
    }

    /* received from the dispatcher (parsed data) */
    case ACTION_DB_UPDATE_P:
      VH_STATS_COUNTER_INC (dbmanager->st_update);
    case ACTION_DB_INSERT_P:
      vh_database_file_data_update (dbmanager->database, pdata);
      if (pdata->od != OD_TYPE_DEF)
        vh_event_handler_od_send (dbmanager->valhalla->event_handler,
                                  pdata->file.path,
                                  VALHALLA_EVENTOD_PARSED, NULL);
      vh_event_handler_md_send (dbmanager->valhalla->event_handler,
                                VALHALLA_EVENTMD_PARSER,
                                NULL, &pdata->file, pdata->meta_parser);
      continue;

    /* received from the scanner */
    case ACTION_DB_NEWFILE:
    {
      int interrup = 0;
      int64_t mtime =
        vh_database_file_get_mtime (dbmanager->database, pdata->file.path);
      /*
       * File is parsed only if mtime has changed, if the grabbing/downloading
       * was interrupted or if it is unexistant in the database.
       *
       * 'interrup' takes three possible values.
       *   0: the file is fully handled (set by ACTION_DB_END).
       *  -1: the file is only inserted in the DB (set by ACTION_DB_NEWFILE),
       *      but the data provided by the parser are not available. This
       *      state is only useful in the case where an ondemand query is
       *      running for the same file between ACTION_DB_NEWFILE and
       *      ACTION_DB_INSERT_P.
       *   1: the data from the parser are in the DB, but the file is not
       *      fully handled by the grabbers and the downloader (set by
       *      ACTION_DB_INSERT_P/UPDATE_P).
       */
      if (mtime >= 0)
      {
        interrup =
          vh_database_file_get_interrupted (dbmanager->database,
                                            pdata->file.path);
        /*
         * Retrieve the list of all grabbers already handled for this file
         * and search if there are files to download since the interruption.
         * But if mtime has changed, the file must be _fully_ updated.
         */
        if (interrup == 1 && pdata->file.mtime == mtime)
        {
          vh_database_file_get_grabber (dbmanager->database,
                                        pdata->file.path, pdata->grabber_list);
          vh_database_file_get_dlcontext (dbmanager->database, pdata->file.path,
                                          &pdata->list_downloader);
        }
        /*
         * Delete all previous associations on the file because the main
         * metadata have changed.
         */
        else if (pdata->file.mtime != mtime)
        {
          vh_database_file_data_delete (dbmanager->database, pdata->file.path);
          vh_database_file_grab_delete (dbmanager->database, pdata->file.path);
        }
      }
      else
      {
        vh_database_file_insert (dbmanager->database, pdata);
        VH_STATS_COUNTER_INC (dbmanager->st_insert);
      }

      if (mtime < 0 || pdata->file.mtime != mtime || interrup == 1)
      {
        vh_dispatcher_action_send (dbmanager->valhalla->dispatcher,
                                   pdata->priority,
                                   mtime < 0
                                   ? ACTION_DB_INSERT_P : ACTION_DB_UPDATE_P,
                                   pdata);
        continue;
      }

      if (pdata->od != OD_TYPE_DEF)
        vh_event_handler_od_send (dbmanager->valhalla->event_handler,
                                  pdata->file.path,
                                  VALHALLA_EVENTOD_ENDED, NULL);
      VH_STATS_COUNTER_INC (dbmanager->st_nochange);
    }
    }

    /* Must not come from "On-demand" */
    if (pdata->od == OD_TYPE_DEF || pdata->od == OD_TYPE_UPD)
      vh_scanner_action_send (dbmanager->valhalla->scanner,
                              FIFO_QUEUE_PRIORITY_NORMAL,
                              ACTION_ACKNOWLEDGE, NULL);
    vh_file_data_free (pdata);
  }
  while (!dbmanager_is_stopped (dbmanager));

  e = ACTION_KILL_THREAD;

 out:
  /* Change files where interrupted__ is -1 to 1. */
  vh_database_file_interrupted_fix (dbmanager->database);

  return e;
}

static void *
dbmanager_thread (void *arg)
{
  int rc, tid, loop = 0;
  const char *file;
  dbmanager_t *dbmanager = arg;

  if (!dbmanager)
    pthread_exit (NULL);

  tid = vh_setpriority (dbmanager->priority);

  vh_log (VALHALLA_MSG_VERBOSE,
          "[%s] tid: %i priority: %i", __FUNCTION__, tid, dbmanager->priority);

  /*
   * Change files where interrupted__ is -1 to 1, in the case where Valhalla
   * was not closed properly (by a signal for example).
   */
  vh_database_file_interrupted_fix (dbmanager->database);

  do
  {
    int stats_delete   = 0;
    unsigned long int stats_update = 0;
    int rst = 0;

    vh_log (VALHALLA_MSG_INFO, "[%s] Begin loop %i", __FUNCTION__, loop);

    /* Clear all checked__ files */
    vh_database_file_checked_clear (dbmanager->database);

    vh_database_begin_transaction (dbmanager->database);
    rc = dbmanager_queue (dbmanager);
    vh_database_end_transaction (dbmanager->database);

    /*
     * Get all files that have checked__ to 0 and verify if the file is valid.
     * The entry is deleted otherwise.
     */
    vh_database_begin_transaction (dbmanager->database);
    while ((file =
              vh_database_file_get_checked_clear (dbmanager->database, rst)))
    {
      if (vh_scanner_path_cmp (dbmanager->valhalla->scanner, file)
          || vh_scanner_suffix_cmp (dbmanager->valhalla->scanner, file)
          || access (file, R_OK))
      {
        /* Manage BEGIN / COMMIT transactions */
        vh_database_step_transaction (dbmanager->database,
                                      dbmanager->commit_int, stats_delete);

        vh_database_file_delete (dbmanager->database, file);
        stats_delete++;
      }

      if (dbmanager_is_stopped (dbmanager))
        rst = 1;
    }

    VH_STATS_COUNTER_ACC (dbmanager->st_delete, (unsigned) stats_delete);

    /* Clean all relations */
    stats_update = vh_stats_counter_read (dbmanager->st_update);
    if (stats_update || stats_delete)
    {
      int val = vh_database_cleanup (dbmanager->database);
      if (val > 0)
        VH_STATS_COUNTER_ACC (dbmanager->st_cleanup, (unsigned) val);
    }

    vh_database_end_transaction (dbmanager->database);

    vh_log (VALHALLA_MSG_INFO, "[%s] End loop %i", __FUNCTION__, loop++);
  }
  while (rc == ACTION_DB_NEXT_LOOP && !dbmanager_is_stopped (dbmanager));

  pthread_exit (NULL);
}

int
vh_dbmanager_run (dbmanager_t *dbmanager, int priority)
{
  int res = DBMANAGER_SUCCESS;
  pthread_attr_t attr;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return DBMANAGER_ERROR_HANDLER;

  dbmanager->priority = priority;
  dbmanager->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = pthread_create (&dbmanager->thread, &attr, dbmanager_thread, dbmanager);
  if (res)
  {
    res = DBMANAGER_ERROR_THREAD;
    dbmanager->run = 0;
  }

  pthread_attr_destroy (&attr);
  return res;
}

fifo_queue_t *
vh_dbmanager_fifo_get (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return NULL;

  return dbmanager->fifo;
}

void
vh_dbmanager_pause (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  VH_THREAD_PAUSE_FCT (dbmanager, 1)
}

void
vh_dbmanager_wait (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  vh_fifo_queue_push (dbmanager->fifo,
                      FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
  pthread_join (dbmanager->thread, NULL);

  dbmanager->run = 0;
}

void
vh_dbmanager_stop (dbmanager_t *dbmanager, int f)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  if (f & STOP_FLAG_REQUEST && !dbmanager_is_stopped (dbmanager))
  {
    pthread_mutex_lock (&dbmanager->mutex_run);
    dbmanager->run = 0;
    pthread_mutex_unlock (&dbmanager->mutex_run);

    vh_fifo_queue_push (dbmanager->fifo,
                        FIFO_QUEUE_PRIORITY_HIGH, ACTION_KILL_THREAD, NULL);
    dbmanager->wait = 1;
  }

  if (f & STOP_FLAG_WAIT && dbmanager->wait)
  {
    pthread_join (dbmanager->thread, NULL);
    dbmanager->wait = 0;
  }
}

void
vh_dbmanager_uninit (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  if (dbmanager->database)
    vh_database_uninit (dbmanager->database);

  vh_fifo_queue_free (dbmanager->fifo);
  pthread_mutex_destroy (&dbmanager->mutex_run);
  VH_THREAD_PAUSE_UNINIT (dbmanager)

  free (dbmanager);
}

static void
dbmanager_stats_dump (vh_stats_t *stats, void *data)
{
  dbmanager_t *dbmanager = data;

  if (!stats || !dbmanager)
    return;

  vh_log (VALHALLA_MSG_INFO, "==============================");
  vh_log (VALHALLA_MSG_INFO, "Statistics dump (" STATS_GROUP ")");
  vh_log (VALHALLA_MSG_INFO, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

  vh_log (VALHALLA_MSG_INFO, "Files inserted    | %lu",
          vh_stats_counter_read (dbmanager->st_insert));
  vh_log (VALHALLA_MSG_INFO, "Files updated     | %lu",
          vh_stats_counter_read (dbmanager->st_update));
  vh_log (VALHALLA_MSG_INFO, "Files deleted     | %lu",
          vh_stats_counter_read (dbmanager->st_delete));
  vh_log (VALHALLA_MSG_INFO, "Files unchanged   | %lu",
          vh_stats_counter_read (dbmanager->st_nochange));
  vh_log (VALHALLA_MSG_INFO, "Relations cleaned | %lu",
          vh_stats_counter_read (dbmanager->st_cleanup));
}

dbmanager_t *
vh_dbmanager_init (valhalla_t *handle, const char *db, unsigned int commit_int)
{
  dbmanager_t *dbmanager;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  dbmanager = calloc (1, sizeof (dbmanager_t));
  if (!dbmanager)
    return NULL;

  dbmanager->fifo = vh_fifo_queue_new ();
  if (!dbmanager->fifo)
    goto err;

  dbmanager->database = vh_database_init (db);
  if (!dbmanager->database)
    goto err;

  if (!commit_int)
    commit_int = DBMANAGER_COMMIT_INTERVAL_DEF;
  dbmanager->commit_int = commit_int;

  dbmanager->valhalla = handle;

  pthread_mutex_init (&dbmanager->mutex_run, NULL);
  VH_THREAD_PAUSE_INIT (dbmanager)

  /* init statistics */
  vh_stats_grp_add (handle->stats,
                    STATS_GROUP, dbmanager_stats_dump, dbmanager);
  dbmanager->st_insert =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_INSERT,   NULL);
  dbmanager->st_update =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_UPDATE,   NULL);
  dbmanager->st_delete =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_DELETE,   NULL);
  dbmanager->st_nochange =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_NOCHANGE, NULL);
  dbmanager->st_cleanup =
    vh_stats_grp_counter_add (handle->stats, STATS_GROUP, STATS_CLEANUP,  NULL);

  return dbmanager;

 err:
  vh_dbmanager_uninit (dbmanager);
  return NULL;
}

void
vh_dbmanager_action_send (dbmanager_t *dbmanager,
                          fifo_queue_prio_t prio, int action, void *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  vh_fifo_queue_push (dbmanager->fifo, prio, action, data);
}

int
vh_dbmanager_file_complete (dbmanager_t *dbmanager,
                            const char *file, int64_t mtime)
{
  int res;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager || !file)
    return 0;

  res = vh_database_file_get_interrupted (dbmanager->database, file);
  if (!res)
  {
    int64_t mt = vh_database_file_get_mtime (dbmanager->database, file);
    if (mt != mtime)
      res = 1; /* must be updated */
    else
      vh_event_handler_od_send (dbmanager->valhalla->event_handler,
                                file, VALHALLA_EVENTOD_ENDED, NULL);
  }

  return !res;
}

void
vh_dbmanager_db_dlcontext_save (dbmanager_t *dbmanager, file_data_t *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager || !data)
    return;

  vh_database_file_insert_dlcontext (dbmanager->database, data);
}

void
vh_dbmanager_db_dlcontext_delete (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  vh_database_delete_dlcontext (dbmanager->database);
}

void
vh_dbmanager_db_begin_transaction (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  vh_database_begin_transaction (dbmanager->database);
}

void
vh_dbmanager_db_end_transaction (dbmanager_t *dbmanager)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  vh_database_end_transaction (dbmanager->database);
}

int
vh_dbmanager_db_metalist_get (dbmanager_t *dbmanager,
                              valhalla_db_item_t *search,
                              valhalla_db_restrict_t *restriction,
                              int (*select_cb) (void *data,
                                                valhalla_db_metares_t *res),
                              void *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return vh_database_metalist_get (dbmanager->database,
                                   search, restriction, select_cb, data);
}

int
vh_dbmanager_db_filelist_get (dbmanager_t *dbmanager,
                              valhalla_file_type_t filetype,
                              valhalla_db_restrict_t *restriction,
                              int (*select_cb) (void *data,
                                                valhalla_db_fileres_t *res),
                              void *data)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return vh_database_filelist_get (dbmanager->database,
                                   filetype, restriction, select_cb, data);
}

int
vh_dbmanager_db_file_get (dbmanager_t *dbmanager,
                          int64_t id, const char *path,
                          valhalla_db_restrict_t *restriction,
                          valhalla_db_filemeta_t **res)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return vh_database_file_get (dbmanager->database, id, path, restriction, res);
}
