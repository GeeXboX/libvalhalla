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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "parser.h"
#include "fifo_queue.h"
#include "timer_thread.h"
#include "database.h"
#include "thread_utils.h"
#include "logs.h"


#ifndef PATH_RECURSIVENESS_MAX
#define PATH_RECURSIVENESS_MAX 42
#endif /* PATH_RECURSIVENESS_MAX */

#define COMMIT_INTERVAL_DEFAULT 128


static int
get_list_length (void *list)
{
  void **l = list;
  int n = 0;

  while (l && *l++)
    n++;
  return n;
}

static char *
my_strrcasestr (const char *buf, const char *str)
{
  char *ptr, *res = NULL;

  while ((ptr = strcasestr (buf, str)))
  {
    res = ptr;
    buf = ptr + strlen (str);
  }

  return res;
}

static void
path_free (struct path_s *path)
{
  struct path_s *path_tmp;

  while (path)
  {
    free (path->location);
    path_tmp = path->next;
    free (path);
    path = path_tmp;
  }
}

static struct path_s *
path_new (const char *location, int recursive)
{
  char *it;
  struct path_s *path;

  if (!location)
    return NULL;

  path = calloc (1, sizeof (struct path_s));
  if (!path)
    return NULL;

  path->location = strdup (location);
  it = strrchr (path->location, '/');
  if (*(it + 1) == '\0')
    *it = '\0';

  path->recursive = recursive ? PATH_RECURSIVENESS_MAX : 0;
  return path;
}

static int
path_cmp (struct path_s *path, const char *file)
{
  for (; path; path = path->next)
    if (strstr (file, path->location) == file)
    {
      int sf = 0;
      const char *it;

      if (!path->recursive)
        return -1;

      it = file + strlen (path->location);
      while ((it = strchr (it, '/')))
      {
        it++;
        sf++;
      }

      if (path->recursive < sf)
        return -1;
      return 0;
    }

  return -1;
}

static int
suffix_cmp (char **suffix, const char *file)
{
  const char *it;

  if (!file)
    return -1;

  if (!suffix) /* always accepted */
    return 0;

  for (; *suffix; suffix++)
  {
    it = my_strrcasestr (file, *suffix);
    if (it && (it > file)
        && *(it + strlen (*suffix)) == '\0' && *(it - 1) == '.')
      return 0;
  }

  return -1;
}

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
    fifo_queue_push (handle->fifo_scanner, ACTION_ACKNOWLEDGE, NULL);
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
      if (path_cmp (handle->paths, file)
          || suffix_cmp (handle->suffix, file)
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
/*                                  Scanner                                   */
/*                                                                            */
/******************************************************************************/

static void
valhalla_readdir (valhalla_t *handle,
                  const char *path, const char *dir, int recursive, int *files)
{
  DIR *dirp;
  struct dirent dp;
  struct dirent *dp_n = NULL;
  struct stat st;
  char *file;
  char *new_path;
  size_t size;

  if (!handle || !path)
    return;

  if (dir)
  {
    size_t size = strlen (path) + strlen (dir) + 2;
    new_path = malloc (size);
    if (!new_path)
      return;

    snprintf (new_path, size, "%s/%s", path, dir);
  }
  else
    new_path = strdup (path);

  dirp = opendir (new_path);
  if (!dirp)
  {
    free (new_path);
    return;
  }

  if (recursive > 0)
  {
    recursive--;
    if (!recursive)
      valhalla_log (VALHALLA_MSG_WARNING,
                    "[thread_scanner] Max recursiveness reached : %s", new_path);
  }

  do
  {
    readdir_r (dirp, &dp, &dp_n);
    if (!dp_n)
      break;

    if (!strcmp (dp.d_name, ".") || !strcmp (dp.d_name, ".."))
      continue;

    size = strlen (new_path) + strlen (dp.d_name) + 2;

    file = malloc (size);
    if (!file)
      continue;

    snprintf (file, size, "%s/%s", new_path, dp.d_name);
    if (lstat (file, &st))
    {
      free (file);
      continue;
    }

    if (S_ISREG (st.st_mode) && !suffix_cmp (handle->suffix, dp.d_name))
    {
      parser_data_t *data = calloc (1, sizeof (parser_data_t));
      if (data)
      {
        data->file = file;
        data->mtime = st.st_mtime;
        fifo_queue_push (handle->fifo_database, ACTION_DB_NEWFILE, data);
        (*files)++;
        continue;
      }
    }
    else if (S_ISDIR (st.st_mode) && recursive)
      valhalla_readdir (handle, new_path, dp.d_name, recursive, files);

    free (file);
  }
  while (!valhalla_is_stopped (handle));

  closedir (dirp);
  free (new_path);
}

static void *
thread_scanner (void *arg)
{
  int i;
  valhalla_t *handle = arg;
  struct path_s *path;

  if (!handle)
    pthread_exit (NULL);

  my_setpriority (handle->priority);

  valhalla_log (VALHALLA_MSG_INFO,
                "[%s] Scanner initialized : loop = %i, timeout = %u [sec]",
                __FUNCTION__, handle->loop, handle->timeout);

  for (i = handle->loop; i; i = i > 0 ? i - 1 : i)
  {
    for (path = handle->paths; path; path = path->next)
    {
      valhalla_log (VALHALLA_MSG_INFO, "[%s] Start scanning : %s",
                    __FUNCTION__, path->location);

      path->nb_files = 0;
      valhalla_readdir (handle,
                        path->location, NULL, path->recursive, &path->nb_files);

      valhalla_log (VALHALLA_MSG_INFO, "[%s] End scanning   : %i files",
                    __FUNCTION__, path->nb_files);
    }

    /*
     * Wait until that all files are parsed and inserted in the database
     * for each path (wait all ACKs).
     */
    for (path = handle->paths; path; path = path->next)
    {
      int files = path->nb_files;
      while (files)
      {
        int e;
        fifo_queue_pop (handle->fifo_scanner, &e, NULL);
        if (e == ACTION_ACKNOWLEDGE)
          files--;

        if (valhalla_is_stopped (handle))
          goto kill;
      }
    }

    /* It is not the last loop ?  */
    if (i != 1)
    {
      fifo_queue_push (handle->fifo_database, ACTION_DB_NEXT_LOOP, NULL);
      timer_thread_sleep (handle->timer, handle->timeout);
    }

    if (valhalla_is_stopped (handle))
      goto kill;
  }

  pthread_exit (NULL);

 kill:
  valhalla_log (VALHALLA_MSG_WARNING, "[%s] Kill forced", __FUNCTION__);
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

  pthread_join (handle->th_scanner, NULL);

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

  /* ask to auto-kill (force wake-up) */
  fifo_queue_push (handle->fifo_scanner, ACTION_KILL_THREAD, NULL);
  fifo_queue_push (handle->fifo_database, ACTION_KILL_THREAD, NULL);

  timer_thread_stop (handle->timer);

  pthread_join (handle->th_scanner, NULL);
  pthread_join (handle->th_database, NULL);

  /* cleanup all queues to prevent memleaks */
  valhalla_queue_cleanup (handle->fifo_scanner);
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
  else
    timer_thread_stop (handle->timer);

  timer_thread_delete (handle->timer);

  if (handle->paths)
    path_free (handle->paths);

  if (handle->suffix)
  {
    char **it;
    for (it = handle->suffix; *it; it++)
      free (*it);
    free (handle->suffix);
  }

  fifo_queue_free (handle->fifo_scanner);
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

  if (!handle->paths)
    return VALHALLA_ERROR_PATH;

  if (handle->run)
    return VALHALLA_ERROR_DEAD;

  /* -1 for infinite loop */
  handle->loop = loop < 1 ? -1 : loop;

  /* if timeout is 0, there is no sleep between loops */
  if (timeout)
  {
    handle->timeout = timeout;
    timer_thread_start (handle->timer);
  }

  handle->priority = priority;
  handle->alive = 1;
  handle->run = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = pthread_create (&handle->th_scanner, &attr, thread_scanner, handle);
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
  struct path_s *path;

  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, location);

  if (!handle || !location)
    return;

  if (!handle->paths)
  {
    handle->paths = path_new (location, recursive);
    return;
  }

  for (path = handle->paths; path->next; path = path->next)
    ;

  path->next = path_new (location, recursive);
}

void
valhalla_suffix_add (valhalla_t *handle, const char *suffix)
{
  int n;

  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, suffix);

  if (!handle || !suffix)
    return;

  n = get_list_length (handle->suffix) + 1;

  handle->suffix = realloc (handle->suffix, (n + 1) * sizeof (*handle->suffix));
  if (!handle->suffix)
    return;

  handle->suffix[n] = NULL;
  handle->suffix[n - 1] = strdup (suffix);
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

  handle->fifo_scanner = fifo_queue_new ();
  if (!handle->fifo_scanner)
    goto err;

  handle->fifo_database = fifo_queue_new ();
  if (!handle->fifo_database)
    goto err;

  handle->database = database_init (db);
  if (!handle->database)
    goto err;

  handle->timer = timer_thread_create ();
  if (!handle->timer)
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
