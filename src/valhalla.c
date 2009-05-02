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

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "fifo_queue.h"
#include "parser.h"
#include "scanner.h"
#include "dbmanager.h"
#include "logs.h"


void
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

  dbmanager_stop (handle->dbmanager);
  parser_stop (handle->parser);

  dbmanager_cleanup (handle->dbmanager);
  parser_cleanup (handle->parser);
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
  scanner_stop (handle->scanner);
  dbmanager_stop (handle->dbmanager);
  parser_stop (handle->parser);

  scanner_cleanup (handle->scanner);
  dbmanager_cleanup (handle->dbmanager);
  parser_cleanup (handle->parser);
}

void
valhalla_uninit (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  valhalla_force_stop (handle);

  scanner_uninit (handle->scanner);
  dbmanager_uninit (handle->dbmanager);
  parser_uninit (handle->parser);

  free (handle);
}

int
valhalla_run (valhalla_t *handle, int loop, uint16_t timeout, int priority)
{
  int res;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return VALHALLA_ERROR_HANDLER;

  if (handle->run)
    return VALHALLA_ERROR_DEAD;

  handle->run = 1;

  res = scanner_run (handle->scanner, loop, timeout, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = dbmanager_run (handle->dbmanager, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = parser_run (handle->parser, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  return VALHALLA_SUCCESS;
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

  handle->dbmanager = dbmanager_init (handle, db, commit_int);
  if (!handle->dbmanager)
    goto err;

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

  return dbmanager_db_author (handle->dbmanager, author, album);
}

int
valhalla_db_album (valhalla_t *handle,
                   valhalla_db_album_t *album, int64_t where_id, int what)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !album)
    return -1;

  return dbmanager_db_album (handle->dbmanager, album, where_id, what);
}

int
valhalla_db_genre (valhalla_t *handle, valhalla_db_genre_t *genre)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !genre)
    return -1;

  return dbmanager_db_genre (handle->dbmanager, genre);
}

int
valhalla_db_file (valhalla_t *handle, valhalla_db_file_t *file,
                  valhalla_db_file_where_t *where)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !file)
    return -1;

  return dbmanager_db_file (handle->dbmanager, file, where);
}
