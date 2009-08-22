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
#include "thread_utils.h"
#include "database.h"
#include "scanner.h"
#include "dbmanager.h"
#include "dispatcher.h"

#define COMMIT_INTERVAL_DEFAULT 128

struct dbmanager_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;

  int             run;
  pthread_mutex_t mutex_run;

  database_t   *database;
  unsigned int  commit_int;
};

typedef struct dbmanager_stats_s {
  int file_insert, file_update, file_nochange;
  int grab_insert, grab_update;
} dbmanager_stats_t;


static inline int
dbmanager_is_stopped (dbmanager_t *dbmanager)
{
  int run;
  pthread_mutex_lock (&dbmanager->mutex_run);
  run = dbmanager->run;
  pthread_mutex_unlock (&dbmanager->mutex_run);
  return !run;
}

#define METADATA_GRABBER_POST           \
  metadata_free (pdata->meta_grabber);  \
  pdata->meta_grabber = NULL;           \
  if (pdata->wait)                      \
    sem_post (&pdata->sem_grabber);

static int
dbmanager_queue (dbmanager_t *dbmanager, dbmanager_stats_t *stats)
{
  int res;
  int e;
  void *data = NULL;
  file_data_t *pdata;

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = fifo_queue_pop (dbmanager->fifo, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      return e;

    if (e == ACTION_DB_NEXT_LOOP)
    {
      dispatcher_action_send (dbmanager->valhalla->dispatcher, e, NULL);
      return e;
    }

    pdata = data;

    /* Manage BEGIN / COMMIT transactions */
    database_step_transaction (dbmanager->database, dbmanager->commit_int,
                               stats->file_insert + stats->file_update +
                               stats->grab_insert + stats->grab_update);

    switch (e)
    {
    default:
      break;

    /* received from the dispatcher */
    case ACTION_DB_END:
      database_file_interrupted_clear (dbmanager->database, pdata->file);
      break;

    /* received from the dispatcher (parsed data) */
    case ACTION_DB_INSERT_P:
      database_file_data_insert (dbmanager->database, pdata);
      stats->file_insert++;
      continue;

    /* received from the dispatcher (grabbed data) */
    case ACTION_DB_INSERT_G:
      database_file_grab_insert (dbmanager->database, pdata);
      METADATA_GRABBER_POST
      stats->grab_insert++;
      continue;

    /* received from the dispatcher (parsed data) */
    case ACTION_DB_UPDATE_P:
      database_file_data_update (dbmanager->database, pdata);
      stats->file_update++;
      continue;

    /* received from the dispatcher (grabbed data) */
    case ACTION_DB_UPDATE_G:
      database_file_grab_update (dbmanager->database, pdata);
      METADATA_GRABBER_POST
      stats->grab_update++;
      continue;

    /* received from the scanner */
    case ACTION_DB_NEWFILE:
    {
      int mtime = database_file_get_mtime (dbmanager->database, pdata->file);
      /*
       * File is parsed only if mtime has changed, if the grabbing/downloading
       * was interrupted or if it is unexistant in the database.
       */
      if (mtime < 0 || (int) pdata->mtime != mtime
          || database_file_get_interrupted (dbmanager->database, pdata->file))
      {
        dispatcher_action_send (dbmanager->valhalla->dispatcher,
                                mtime < 0
                                ? ACTION_DB_INSERT_P : ACTION_DB_UPDATE_P,
                                pdata);
        continue;
      }

      stats->file_nochange++;
    }
    }

    file_data_free (pdata);
    scanner_action_send (dbmanager->valhalla->scanner,
                         ACTION_ACKNOWLEDGE, NULL);
  }
  while (!dbmanager_is_stopped (dbmanager));

  return ACTION_KILL_THREAD;
}

static void *
dbmanager_thread (void *arg)
{
  int rc;
  const char *file;
  dbmanager_t *dbmanager = arg;

  if (!dbmanager)
    pthread_exit (NULL);

  my_setpriority (dbmanager->priority);

  do
  {
    dbmanager_stats_t stats = {
      0, 0, 0, 0, 0
    };
    int stats_delete   = 0;
    int stats_cleanup  = 0;

    /* Clear all checked__ files */
    database_file_checked_clear (dbmanager->database);

    database_begin_transaction (dbmanager->database);
    rc = dbmanager_queue (dbmanager, &stats);
    database_end_transaction (dbmanager->database);

    /*
     * Get all files that have checked__ to 0 and verify if the file is valid.
     * The entry is deleted otherwise.
     */
    database_begin_transaction (dbmanager->database);
    while ((file = database_file_get_checked_clear (dbmanager->database)))
      if (scanner_path_cmp (dbmanager->valhalla->scanner, file)
          || scanner_suffix_cmp (dbmanager->valhalla->scanner, file)
          || access (file, R_OK))
      {
        /* Manage BEGIN / COMMIT transactions */
        database_step_transaction (dbmanager->database,
                                   dbmanager->commit_int, stats_delete);

        database_file_data_delete (dbmanager->database, file);
        stats_delete++;
      }
    database_end_transaction (dbmanager->database);

    /* Clean all relations */
    if (stats.file_update || stats.grab_update || stats_delete)
      stats_cleanup = database_cleanup (dbmanager->database);

    /* Statistics */
    valhalla_log (VALHALLA_MSG_INFO, "[%s] Files inserted    : %i",
                  __FUNCTION__, stats.file_insert);
    valhalla_log (VALHALLA_MSG_INFO, "[%s] Files updated     : %i",
                  __FUNCTION__, stats.file_update);
    valhalla_log (VALHALLA_MSG_INFO, "[%s] Files deleted     : %i",
                  __FUNCTION__, stats_delete);
    valhalla_log (VALHALLA_MSG_INFO, "[%s] Files unchanged   : %i",
                  __FUNCTION__, stats.file_nochange);
    valhalla_log (VALHALLA_MSG_INFO, "[%s] Relations cleanup : %i",
                  __FUNCTION__, stats_cleanup);
  }
  while (rc == ACTION_DB_NEXT_LOOP);

  pthread_exit (NULL);
}

int
dbmanager_run (dbmanager_t *dbmanager, int priority)
{
  int res = DBMANAGER_SUCCESS;
  pthread_attr_t attr;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
dbmanager_fifo_get (dbmanager_t *dbmanager)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return NULL;

  return dbmanager->fifo;
}

void
dbmanager_stop (dbmanager_t *dbmanager)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  if (dbmanager_is_stopped (dbmanager))
    return;

  pthread_mutex_lock (&dbmanager->mutex_run);
  dbmanager->run = 0;
  pthread_mutex_unlock (&dbmanager->mutex_run);

  fifo_queue_push (dbmanager->fifo, ACTION_KILL_THREAD, NULL);
  pthread_join (dbmanager->thread, NULL);
}

void
dbmanager_uninit (dbmanager_t *dbmanager)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  if (dbmanager->database)
    database_uninit (dbmanager->database);

  fifo_queue_free (dbmanager->fifo);
  pthread_mutex_destroy (&dbmanager->mutex_run);

  free (dbmanager);
}

dbmanager_t *
dbmanager_init (valhalla_t *handle, const char *db, unsigned int commit_int)
{
  dbmanager_t *dbmanager;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  dbmanager = calloc (1, sizeof (dbmanager_t));
  if (!dbmanager)
    return NULL;

  dbmanager->fifo = fifo_queue_new ();
  if (!dbmanager->fifo)
    goto err;

  dbmanager->database = database_init (db);
  if (!dbmanager->database)
    goto err;

  if (commit_int <= 0)
    commit_int = COMMIT_INTERVAL_DEFAULT;
  dbmanager->commit_int = commit_int;

  dbmanager->valhalla = handle;

  pthread_mutex_init (&dbmanager->mutex_run, NULL);

  return dbmanager;

 err:
  dbmanager_uninit (dbmanager);
  return NULL;
}

void
dbmanager_action_send (dbmanager_t *dbmanager, int action, void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return;

  fifo_queue_push (dbmanager->fifo, action, data);
}

int
dbmanager_db_metalist_get (dbmanager_t *dbmanager,
                           valhalla_db_item_t *search,
                           valhalla_db_restrict_t *restriction,
                           int (*select_cb) (void *data,
                                             valhalla_db_metares_t *res),
                           void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return database_metalist_get (dbmanager->database,
                                search, restriction, select_cb, data);
}

int
dbmanager_db_filelist_get (dbmanager_t *dbmanager,
                           valhalla_file_type_t filetype,
                           valhalla_db_restrict_t *restriction,
                           int (*select_cb) (void *data,
                                             valhalla_db_fileres_t *res),
                           void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return database_filelist_get (dbmanager->database,
                                filetype, restriction, select_cb, data);
}

int
dbmanager_db_file_get (dbmanager_t *dbmanager,
                       int64_t id, const char *path,
                       valhalla_db_restrict_t *restriction,
                       valhalla_db_filemeta_t **res)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!dbmanager)
    return -1;

  return database_file_get (dbmanager->database, id, path, restriction, res);
}
