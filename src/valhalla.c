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

/* handle thread (process) priorities */
#include <sys/resource.h>
#include <sys/syscall.h>

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "fifo_queue.h"
#include "timer_thread.h"
#include "database.h"
#include "logs.h"
#include "lavf_utils.h"


#ifndef PARSER_NB_MAX
#define PARSER_NB_MAX 8
#endif /* PARSER_NB_MAX */

#ifndef PATH_RECURSIVENESS_MAX
#define PATH_RECURSIVENESS_MAX 42
#endif /* PATH_RECURSIVENESS_MAX */

#define COMMIT_INTERVAL_DEFAULT 128

typedef enum action_list {
  ACTION_KILL_THREAD  = -1, /* auto-kill when all pending commands are ended */
  ACTION_NO_OPERATION =  0, /* wake-up for nothing */
  ACTION_DB_INSERT,         /* parser: metadata okay, then insert in the DB */
  ACTION_DB_UPDATE,         /* parser: metadata okay, then update in the DB */
  ACTION_DB_NEWFILE,        /* scanner: new file to handle */
  ACTION_DB_NEXT_LOOP,      /* scanner: stop db manage queue for next loop */
  ACTION_ACKNOWLEDGE,       /* database: ack scanner for each file handled */
  ACTION_CLEANUP_END,       /* special case for garbage collector */
} action_list_t;

struct valhalla_s {
  pthread_t     th_scanner;
  pthread_t     th_parser[PARSER_NB_MAX];
  pthread_t     th_database;
  database_t   *database;
  fifo_queue_t *fifo_scanner;
  fifo_queue_t *fifo_parser;
  fifo_queue_t *fifo_database;

  unsigned int parser_nb;
  unsigned int commit_int;

  int priority; /* priority of all threads */
  int run;      /* prevent a bug if valhalla_run() is called two times */
  int alive;    /* used for killing all threads with uninit */
  pthread_mutex_t mutex_alive;

  int loop;
  uint16_t timeout;
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

static const char *
suffix_fmt_guess (const char *file)
{
  const char *it;

  if (!file)
    return NULL;

  it = strrchr (file, '.');
  if (it)
    it++;

  return lavf_utils_fmtname_get (it);
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

static inline void
my_setpriority (int prio)
{
  pid_t pid = syscall (__NR_gettid); /* gettid() is not available with glibc */
  setpriority (PRIO_PROCESS, pid, prio);
}

/******************************************************************************/
/*                                                                            */
/*                                  Parser                                    */
/*                                                                            */
/******************************************************************************/

static void
parser_metadata_free (parser_metadata_t *meta)
{
  if (!meta)
    return;

  if (meta->title)
    free (meta->title);
  if (meta->author)
    free (meta->author);
  if (meta->album)
    free (meta->album);
  if (meta->genre)
    free (meta->genre);
  free (meta);
}

static void
parser_data_free (parser_data_t *data)
{
  if (!data)
    return;

  if (data->file)
    free (data->file);
  if (data->meta)
    parser_metadata_free (data->meta);
  free (data);
}

static char *
parser_trim (char *str)
{
  char *its, *ite;

  its = str;
  ite = strchr (str, '\0');

  /* remove whitespaces at the right */
  while (ite > its && isspace (*(ite - 1)))
    ite--;
  *ite = '\0';

  /* remove whitespaces at the left */
  while (isspace (*its))
    its++;

  return its;
}

static parser_metadata_t *
parser_metadata_get (AVFormatContext *ctx, AVInputFormat *fmt)
{
  parser_metadata_t *meta;
  char *title, *author, *album, *genre;
  const char *genre_name = NULL;

  if (!ctx)
    return NULL;

  meta = calloc (1, sizeof (parser_metadata_t));
  if (!meta)
    return NULL;

  /* remove whitespaces */
  title  = parser_trim (ctx->title);
  author = parser_trim (ctx->author);
  album  = parser_trim (ctx->album);
  genre  = parser_trim (ctx->genre);

  /*
   * Sometimes with crappy MP3, ID3v1 genre number is returned
   * instead of the name.
   */
  if (fmt && *genre && !strcmp (fmt->name, "mp3"))
  {
    int res, id;

    res = sscanf (genre, "%i", &id);
    if (res != 1)
      res = sscanf (genre, "(%i)", &id);
    if (res == 1)
      genre_name = lavf_utils_id3v1_genre (id);
  }

  meta->title  = *title  ? strdup (title)  : NULL;
  meta->author = *author ? strdup (author) : NULL;
  meta->album  = *album  ? strdup (album)  : NULL;

  if (genre_name)
    meta->genre = strdup (genre_name);
  else
    meta->genre = *genre ? strdup (genre) : NULL;

  meta->year  = ctx->year;
  meta->track = ctx->track;

  return meta;
}

#define PROBE_BUF_MIN 2048
#define PROBE_BUF_MAX (1 << 20)

/*
 * This function is fully inspired of (libavformat/utils.c v52.28.0
 * "av_open_input_file()") to probe data in order to test if *ftm argument
 * is the right fmt or not.
 *
 * The original function from avformat "av_probe_input_format2()" is really
 * slow because it probes the fmt of _all_ demuxers for each probe_data
 * buffer. Here, the test is only for _one_ fmt and returns the score
 * (if > score_max) provided by fmt->probe().
 *
 * WARNING: this function depends of some internal behaviours of libavformat
 *          and can be "broken" with future versions of FFmpeg.
 */
static int
parser_probe (AVInputFormat *fmt, const char *file)
{
  FILE *fd;
  int rc = 0;
  int p_size;
  AVProbeData p_data;

  if ((fmt->flags & AVFMT_NOFILE) || !fmt->read_probe)
    return 0;

  fd = fopen (file, "r");
  if (!fd)
    return 0;

  p_data.filename = file;
  p_data.buf = NULL;

  for (p_size = PROBE_BUF_MIN; p_size <= PROBE_BUF_MAX; p_size <<= 1)
  {
    int score;
    int score_max = p_size < PROBE_BUF_MAX ? AVPROBE_SCORE_MAX / 4 : 0;

    p_data.buf = realloc (p_data.buf, p_size + AVPROBE_PADDING_SIZE);
    if (!p_data.buf)
      break;

    p_data.buf_size = fread (p_data.buf, 1, p_size, fd);
    if (p_data.buf_size != p_size) /* EOF is reached? */
      break;

    memset (p_data.buf + p_data.buf_size, 0, AVPROBE_PADDING_SIZE);

    if (fseek (fd, 0, SEEK_SET))
      break;

    score = fmt->read_probe (&p_data);
    if (score > score_max)
    {
      rc = score;
      break;
    }
  }

  if (p_data.buf)
    free (p_data.buf);

  fclose (fd);
  return rc;
}

static parser_metadata_t *
parser_metadata (const char *file)
{
  int res;
  const char *name;
  AVFormatContext   *ctx;
  AVInputFormat     *fmt = NULL;
  parser_metadata_t *metadata;

  /*
   * Try a format in function of the suffix.
   * We gain a lot of speed if the fmt is already the right.
   */
  name = suffix_fmt_guess (file);
  if (name)
    fmt = av_find_input_format (name);

  if (fmt)
  {
    int score = parser_probe (fmt, file);
    if (!score) /* Bad score? */
      fmt = NULL;
  }

  res = av_open_input_file (&ctx, file, fmt, 0, NULL);
  if (res)
  {
    valhalla_log (VALHALLA_MSG_WARNING, "FFmpeg can't open file : %s", file);
    return NULL;
  }

#if 0
  /*
   * Can be useful in the future in order to detect if the file
   * is audio or video.
   */
  res = av_find_stream_info (ctx);
  if (res < 0)
  {
    valhalla_log (VALHALLA_MSG_WARNING,
                  "FFmpeg can't find stream info: %s", file);
  }
  else
#endif /* 0 */

  metadata = parser_metadata_get (ctx, fmt);

  av_close_input_file (ctx);
  return metadata;
}

static void *
thread_parser (void *arg)
{
  int res;
  int e;
  void *data = NULL;
  parser_data_t *pdata;
  valhalla_t *handle = arg;

  if (!handle)
    pthread_exit (NULL);

  my_setpriority (handle->priority);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;

    res = fifo_queue_pop (handle->fifo_parser, &e, &data);
    if (res || e == ACTION_NO_OPERATION)
      continue;

    if (e == ACTION_KILL_THREAD)
      break;

    pdata = data;
    if (pdata)
      pdata->meta = parser_metadata (pdata->file);

    fifo_queue_push (handle->fifo_database, e, pdata);
  }
  while (!valhalla_is_stopped (handle));

  pthread_exit (NULL);
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
        fifo_queue_push (handle->fifo_parser,
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
  int i;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  pthread_join (handle->th_scanner, NULL);

  fifo_queue_push (handle->fifo_database, ACTION_KILL_THREAD, NULL);
  pthread_join (handle->th_database, NULL);

  for (i = 0; i < handle->parser_nb; i++)
    fifo_queue_push (handle->fifo_parser, ACTION_KILL_THREAD, NULL);

  for (i = 0; i < handle->parser_nb; i++)
    pthread_join (handle->th_parser[i], NULL);

  pthread_mutex_lock (&handle->mutex_alive);
  handle->alive = 0;
  pthread_mutex_unlock (&handle->mutex_alive);
}

static void
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
  int i;

  valhalla_log (VALHALLA_MSG_WARNING, "force to stop all threads");

  /* force everybody to be killed on the next wake up */
  pthread_mutex_lock (&handle->mutex_alive);
  handle->alive = 0;
  pthread_mutex_unlock (&handle->mutex_alive);

  /* ask to auto-kill (force wake-up) */
  fifo_queue_push (handle->fifo_scanner, ACTION_KILL_THREAD, NULL);
  fifo_queue_push (handle->fifo_database, ACTION_KILL_THREAD, NULL);
  for (i = 0; i < handle->parser_nb; i++)
    fifo_queue_push (handle->fifo_parser, ACTION_KILL_THREAD, NULL);

  timer_thread_stop (handle->timer);

  pthread_join (handle->th_scanner, NULL);
  pthread_join (handle->th_database, NULL);
  for (i = 0; i < handle->parser_nb; i++)
    pthread_join (handle->th_parser[i], NULL);

  /* cleanup all queues to prevent memleaks */
  valhalla_queue_cleanup (handle->fifo_scanner);
  valhalla_queue_cleanup (handle->fifo_database);
  valhalla_queue_cleanup (handle->fifo_parser);
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
  fifo_queue_free (handle->fifo_parser);
  fifo_queue_free (handle->fifo_database);

  if (handle->database)
    database_uninit (handle->database);

  pthread_mutex_destroy (&handle->mutex_alive);

  free (handle);
}

int
valhalla_run (valhalla_t *handle, int loop, uint16_t timeout, int priority)
{
  int i, res = VALHALLA_SUCCESS;
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

  for (i = 0; i < handle->parser_nb; i++)
  {
    res = pthread_create (&handle->th_parser[i], &attr, thread_parser, handle);
    if (res)
    {
      res = VALHALLA_ERROR_THREAD;
      break;
    }
  }

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

  if (!parser_nb || parser_nb > ARRAY_NB_ELEMENTS (handle->th_parser))
    goto err;

  handle->fifo_scanner = fifo_queue_new ();
  if (!handle->fifo_scanner)
    goto err;

  handle->fifo_parser = fifo_queue_new ();
  if (!handle->fifo_parser)
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

  handle->parser_nb = parser_nb;

  if (commit_int <= 0)
    commit_int = COMMIT_INTERVAL_DEFAULT;
  handle->commit_int = commit_int;

  pthread_mutex_init (&handle->mutex_alive, NULL);

  if (!preinit)
  {
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
