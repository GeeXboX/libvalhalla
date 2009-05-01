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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "parser.h"
#include "scanner.h"
#include "fifo_queue.h"
#include "database.h"
#include "thread_utils.h"
#include "logs.h"

#define COMMIT_INTERVAL_DEFAULT 128


static inline int
valhalla_is_stopped (valhalla_t *handle)
{
  int alive;
  pthread_mutex_lock (&handle->mutex_alive);
  alive = handle->alive;
  pthread_mutex_unlock (&handle->mutex_alive);
  return !alive;
}

static void
parser_data_free (parser_data_t *data)
{
  if (!data)
    return;

  if (data->file)
    free (data->file);
  if (data->meta)
    metadata_free (data->meta);
  free (data);
}

/******************************************************************************/
/*                                                                            */
/*                                 Database                                   */
/*                                                                            */
/******************************************************************************/

static int
db_manage_queue (valhalla_t *handle,
                 int *stats_insert, int *stats_update, int *stats_nochange)
{
  int res;
  int e;
  void *data = NULL;
  parser_data_t *pdata;

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = fifo_queue_pop (handle->fifo_database, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD || e == ACTION_DB_NEXT_LOOP)
      return e;

    pdata = data;

    /* Manage BEGIN / COMMIT transactions */
    database_step_transaction (handle->database, handle->commit_int,
                               *stats_insert + *stats_update);

    switch (e)
    {
    default:
      break;

    /* received from the parser */
    case ACTION_DB_INSERT:
      database_file_data_insert (handle->database, pdata);
      (*stats_insert)++;
      break;

    /* received from the parser */
    case ACTION_DB_UPDATE:
      database_file_data_update (handle->database, pdata);
      (*stats_update)++;
      break;

    /* received from the scanner */
    case ACTION_DB_NEWFILE:
    {
      int mtime = database_file_get_mtime (handle->database, pdata->file);
      /*
       * File is parsed only if mtime has changed or if it is unexistant
       * in the database.
       */
      if (mtime < 0 || (int) pdata->mtime != mtime)
      {
        parser_action_send (handle->parser,
                            mtime < 0 ? ACTION_DB_INSERT : ACTION_DB_UPDATE,
                            pdata);
        continue;
      }

      (*stats_nochange)++;
    }
    }

    parser_data_free (pdata);
    scanner_action_send (handle->scanner, ACTION_ACKNOWLEDGE, NULL);
  }
  while (!valhalla_is_stopped (handle));

  return ACTION_KILL_THREAD;
}

static void *
thread_database (void *arg)
{
  int rc;
  const char *file;
  valhalla_t *handle = arg;

  if (!handle)
    pthread_exit (NULL);

  my_setpriority (handle->priority);

  do
  {
    int stats_insert   = 0;
    int stats_update   = 0;
    int stats_delete   = 0;
    int stats_nochange = 0;
    int stats_cleanup  = 0;

    /* Clear all checked__ files */
    database_file_checked_clear (handle->database);

    database_begin_transaction (handle->database);
    rc =
      db_manage_queue (handle, &stats_insert, &stats_update, &stats_nochange);
    database_end_transaction (handle->database);

    /*
     * Get all files that have checked__ to 0 and verify if the file is valid.
     * The entry is deleted otherwise.
     */
    database_begin_transaction (handle->database);
    while ((file = database_file_get_checked_clear (handle->database)))
      if (scanner_path_cmp (handle->scanner, file)
          || scanner_suffix_cmp (handle->scanner, file)
          || access (file, R_OK))
      {
        /* Manage BEGIN / COMMIT transactions */
        database_step_transaction (handle->database,
                                   handle->commit_int, stats_delete);

        database_file_data_delete (handle->database, file);
        stats_delete++;
      }
    database_end_transaction (handle->database);

    /* Clean all relations */
    if (stats_update || stats_delete)
      stats_cleanup = database_cleanup (handle->database);

    /* Statistics */
    valhalla_log (VALHALLA_MSG_INFO,
                  "[%s] Files inserted    : %i", __FUNCTION__, stats_insert);
    valhalla_log (VALHALLA_MSG_INFO,
                  "[%s] Files updated     : %i", __FUNCTION__, stats_update);
    valhalla_log (VALHALLA_MSG_INFO,
                  "[%s] Files deleted     : %i", __FUNCTION__, stats_delete);
    valhalla_log (VALHALLA_MSG_INFO,
                  "[%s] Files unchanged   : %i", __FUNCTION__, stats_nochange);
    valhalla_log (VALHALLA_MSG_INFO,
                  "[%s] Relations cleanup : %i", __FUNCTION__, stats_cleanup);
  }
  while (rc == ACTION_DB_NEXT_LOOP);

  pthread_exit (NULL);
}

/******************************************************************************/
/*                                                                            */
/*                             Valhalla Handling                              */
/*                                                                            */
/******************************************************************************/

void
valhalla_wait (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  scanner_wait (handle->scanner);

  fifo_queue_push (handle->fifo_database, ACTION_KILL_THREAD, NULL);
  pthread_join (handle->th_database, NULL);

  parser_stop (handle->parser);

  pthread_mutex_lock (&handle->mutex_alive);
  handle->alive = 0;
  pthread_mutex_unlock (&handle->mutex_alive);
}

void
valhalla_queue_cleanup (fifo_queue_t *queue)
{
  int e;
  void *data;

  fifo_queue_push (queue, ACTION_CLEANUP_END, NULL);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;
    fifo_queue_pop (queue, &e, &data);

    switch (e)
    {
    default:
      break;

    case ACTION_DB_INSERT:
    case ACTION_DB_UPDATE:
    case ACTION_DB_NEWFILE:
      if (data)
        parser_data_free (data);
      break;
    }
  }
  while (e != ACTION_CLEANUP_END);
}

static void
valhalla_force_stop (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_WARNING, "force to stop all threads");

  /* force everybody to be killed on the next wake up */
  pthread_mutex_lock (&handle->mutex_alive);
  handle->alive = 0;
  pthread_mutex_unlock (&handle->mutex_alive);

  scanner_stop (handle->scanner);

  /* ask to auto-kill (force wake-up) */
  fifo_queue_push (handle->fifo_database, ACTION_KILL_THREAD, NULL);
  pthread_join (handle->th_database, NULL);

  /* cleanup all queues to prevent memleaks */
  valhalla_queue_cleanup (handle->fifo_database);

  parser_stop (handle->parser);
}

void
valhalla_uninit (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  if (!valhalla_is_stopped (handle))
    valhalla_force_stop (handle);

  scanner_uninit (handle->scanner);
  fifo_queue_free (handle->fifo_database);
  parser_uninit (handle->parser);

  if (handle->database)
    database_uninit (handle->database);

  pthread_mutex_destroy (&handle->mutex_alive);

  free (handle);
}

int
valhalla_run (valhalla_t *handle, int loop, uint16_t timeout, int priority)
{
  int res = VALHALLA_SUCCESS;
  pthread_attr_t attr;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return VALHALLA_ERROR_HANDLER;

  if (handle->run)
    return VALHALLA_ERROR_DEAD;

  handle->priority = priority;
  handle->alive = 1;
  handle->run = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = scanner_run (handle->scanner, loop, timeout, priority);
  if (res)
  {
    res = VALHALLA_ERROR_THREAD;
    goto out;
  }

  res = pthread_create (&handle->th_database, &attr, thread_database, handle);
  if (res)
  {
    res = VALHALLA_ERROR_THREAD;
    goto out;
  }

  res = parser_run (handle->parser, priority);
  if (res)
    res = VALHALLA_ERROR_THREAD;

 out:
  pthread_attr_destroy (&attr);
  return res;
}

void
valhalla_path_add (valhalla_t *handle, const char *location, int recursive)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, location);

  if (!handle || !location)
    return;

  scanner_path_add (handle->scanner, location, recursive);
}

void
valhalla_suffix_add (valhalla_t *handle, const char *suffix)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, suffix);

  if (!handle || !suffix)
    return;

  scanner_suffix_add (handle->scanner, suffix);
}

void
valhalla_verbosity (valhalla_verb_t level)
{
  vlog_verb (level);
}

valhalla_t *
valhalla_init (const char *db,
               unsigned int parser_nb, unsigned int commit_int)
{
  static int preinit = 0;
  valhalla_t *handle;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!db)
    return NULL;

  handle = calloc (1, sizeof (valhalla_t));
  if (!handle)
    return NULL;

  handle->parser = parser_init (handle, parser_nb);
  if (!handle->parser)
    goto err;

  handle->scanner = scanner_init (handle);
  if (!handle->scanner)
    goto err;

  handle->fifo_database = fifo_queue_new ();
  if (!handle->fifo_database)
    goto err;

  handle->database = database_init (db);
  if (!handle->database)
    goto err;

  if (commit_int <= 0)
    commit_int = COMMIT_INTERVAL_DEFAULT;
  handle->commit_int = commit_int;

  pthread_mutex_init (&handle->mutex_alive, NULL);

  if (!preinit)
  {
    av_log_set_level (AV_LOG_ERROR);
    av_register_all ();
    preinit = 1;
  }

  return handle;

 err:
  valhalla_uninit (handle);
  return NULL;
}

/******************************************************************************/
/*                                                                            */
/*                         Public Database Selections                         */
/*                                                                            */
/******************************************************************************/

int
valhalla_db_author (valhalla_t *handle,
                    valhalla_db_author_t *author, int64_t album)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !author)
    return -1;

  return database_select_author (handle->database,
                                 &author->id, &author->name, album);
}

int
valhalla_db_album (valhalla_t *handle,
                   valhalla_db_album_t *album, int64_t where_id, int what)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !album)
    return -1;

  return database_select_album (handle->database,
                                &album->id, &album->name, where_id, what);
}

int
valhalla_db_genre (valhalla_t *handle, valhalla_db_genre_t *genre)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !genre)
    return -1;

  return database_select_genre (handle->database, &genre->id, &genre->name);
}

int
valhalla_db_file (valhalla_t *handle, valhalla_db_file_t *file,
                  valhalla_db_file_where_t *where)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !file)
    return -1;

  return database_select_file (handle->database, file, where);
}
