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
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "fifo_queue.h"
#include "logs.h"
#include "thread_utils.h"
#include "timer_thread.h"
#include "dbmanager.h"
#include "scanner.h"

#ifndef PATH_RECURSIVENESS_MAX
#define PATH_RECURSIVENESS_MAX 42
#endif /* PATH_RECURSIVENESS_MAX */

struct scanner_s {
  valhalla_t   *valhalla;
  pthread_t     thread;
  fifo_queue_t *fifo;
  int           priority;
  int           loop;

  int             run;
  pthread_mutex_t mutex_run;

  uint16_t        timeout;
  timer_thread_t *timer;

  struct path_s {
    struct path_s *next;
    char *location;
    int recursive;
    int nb_files;
  } *paths;
  char **suffix;
};


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
scanner_is_stopped (scanner_t *scanner)
{
  int run;
  pthread_mutex_lock (&scanner->mutex_run);
  run = scanner->run;
  pthread_mutex_unlock (&scanner->mutex_run);
  return !run;
}

static void
valhalla_readdir (scanner_t *scanner,
                  const char *path, const char *dir, int recursive, int *files)
{
  DIR *dirp;
  struct dirent dp;
  struct dirent *dp_n = NULL;
  struct stat st;
  char *file;
  char *new_path;
  size_t size;

  if (!scanner || !path)
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
                    "[scanner_thread] Max recursiveness reached : %s", new_path);
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

    if (S_ISREG (st.st_mode) && !suffix_cmp (scanner->suffix, dp.d_name))
    {
      parser_data_t *data = calloc (1, sizeof (parser_data_t));
      if (data)
      {
        data->file = file;
        data->mtime = st.st_mtime;
        dbmanager_action_send (scanner->valhalla->dbmanager,
                               ACTION_DB_NEWFILE, data);
        (*files)++;
        continue;
      }
    }
    else if (S_ISDIR (st.st_mode) && recursive)
      valhalla_readdir (scanner, new_path, dp.d_name, recursive, files);

    free (file);
  }
  while (!scanner_is_stopped (scanner));

  closedir (dirp);
  free (new_path);
}

static void *
scanner_thread (void *arg)
{
  int i;
  scanner_t *scanner = arg;
  struct path_s *path;

  if (!scanner)
    pthread_exit (NULL);

  my_setpriority (scanner->priority);

  valhalla_log (VALHALLA_MSG_INFO,
                "[%s] Scanner initialized : loop = %i, timeout = %u [sec]",
                __FUNCTION__, scanner->loop, scanner->timeout);

  for (i = scanner->loop; i; i = i > 0 ? i - 1 : i)
  {
    for (path = scanner->paths; path; path = path->next)
    {
      valhalla_log (VALHALLA_MSG_INFO, "[%s] Start scanning : %s",
                    __FUNCTION__, path->location);

      path->nb_files = 0;
      valhalla_readdir (scanner,
                        path->location, NULL, path->recursive, &path->nb_files);

      valhalla_log (VALHALLA_MSG_INFO, "[%s] End scanning   : %i files",
                    __FUNCTION__, path->nb_files);
    }

    /*
     * Wait until that all files are parsed and inserted in the database
     * for each path (wait all ACKs).
     */
    for (path = scanner->paths; path; path = path->next)
    {
      int files = path->nb_files;
      while (files)
      {
        int e;
        fifo_queue_pop (scanner->fifo, &e, NULL);
        if (e == ACTION_ACKNOWLEDGE)
          files--;

        if (scanner_is_stopped (scanner))
          goto kill;
      }
    }

    /* It is not the last loop ?  */
    if (i != 1)
    {
      dbmanager_action_send (scanner->valhalla->dbmanager, ACTION_DB_NEXT_LOOP, NULL);
      timer_thread_sleep (scanner->timer, scanner->timeout);
    }

    if (scanner_is_stopped (scanner))
      goto kill;
  }

  pthread_exit (NULL);

 kill:
  valhalla_log (VALHALLA_MSG_WARNING, "[%s] Kill forced", __FUNCTION__);
  pthread_exit (NULL);
}

int
scanner_run (scanner_t *scanner, int loop, uint16_t timeout, int priority)
{
  int res = SCANNER_SUCCESS;
  pthread_attr_t attr;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return SCANNER_ERROR_HANDLER;

  if (!scanner->paths)
    return SCANNER_ERROR_PATH;

  /* if timeout is 0, there is no sleep between loops */
  if (timeout)
  {
    scanner->timeout = timeout;
    timer_thread_start (scanner->timer);
  }

  /* -1 for infinite loop */
  scanner->loop = loop < 1 ? -1 : loop;

  scanner->priority = priority;
  scanner->run      = 1;

  pthread_attr_init (&attr);
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE);

  res = pthread_create (&scanner->thread, &attr, scanner_thread, scanner);
  if (res)
  {
    res = SCANNER_ERROR_THREAD;
    scanner->run = 0;
  }

  pthread_attr_destroy (&attr);
  return res;
}

void
scanner_wait (scanner_t *scanner)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  pthread_join (scanner->thread, NULL);

  scanner->run = 0;
}

void
scanner_cleanup (scanner_t *scanner)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  valhalla_queue_cleanup (scanner->fifo);
}

void
scanner_stop (scanner_t *scanner)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  if (scanner_is_stopped (scanner))
    return;

  timer_thread_stop (scanner->timer);

  pthread_mutex_lock (&scanner->mutex_run);
  scanner->run = 0;
  pthread_mutex_unlock (&scanner->mutex_run);

  fifo_queue_push (scanner->fifo, ACTION_KILL_THREAD, NULL);
  timer_thread_stop (scanner->timer);
  pthread_join (scanner->thread, NULL);
}

void
scanner_uninit (scanner_t *scanner)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  timer_thread_delete (scanner->timer);

  if (scanner->paths)
    path_free (scanner->paths);

  if (scanner->suffix)
  {
    char **it;
    for (it = scanner->suffix; *it; it++)
      free (*it);
    free (scanner->suffix);
  }

  fifo_queue_free (scanner->fifo);
  pthread_mutex_destroy (&scanner->mutex_run);

  free (scanner);
}

int
scanner_path_cmp (scanner_t *scanner, const char *file)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return -1;

  return path_cmp (scanner->paths, file);
}

void
scanner_path_add (scanner_t *scanner, const char *location, int recursive)
{
  struct path_s *path;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  if (!scanner->paths)
  {
    scanner->paths = path_new (location, recursive);
    return;
  }

  for (path = scanner->paths; path->next; path = path->next)
    ;

  path->next = path_new (location, recursive);
}

int
scanner_suffix_cmp (scanner_t *scanner, const char *file)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return -1;

  return suffix_cmp (scanner->suffix, file);
}


void
scanner_suffix_add (scanner_t *scanner, const char *suffix)
{
  int n;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  n = get_list_length (scanner->suffix) + 1;

  scanner->suffix =
    realloc (scanner->suffix, (n + 1) * sizeof (*scanner->suffix));
  if (!scanner->suffix)
    return;

  scanner->suffix[n] = NULL;
  scanner->suffix[n - 1] = strdup (suffix);
}

scanner_t *
scanner_init (valhalla_t *handle)
{
  scanner_t *scanner;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

  scanner = calloc (1, sizeof (scanner_t));
  if (!scanner)
    return NULL;

  scanner->fifo = fifo_queue_new ();
  if (!scanner->fifo)
    goto err;

  scanner->timer = timer_thread_create ();
  if (!scanner->timer)
    goto err;

  scanner->valhalla = handle;

  pthread_mutex_init (&scanner->mutex_run, NULL);

  return scanner;

 err:
  scanner_uninit (scanner);
  return NULL;
}

void
scanner_action_send (scanner_t *scanner, int action, void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!scanner)
    return;

  fifo_queue_push (scanner->fifo, action, data);
}
