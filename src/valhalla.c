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

#include <stdlib.h>

#include <libavformat/avformat.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "parser.h"
#include "scanner.h"
#include "dbmanager.h"
#include "dispatcher.h"
#include "utils.h"
#include "logs.h"

#ifdef USE_GRABBER
#include "grabber.h"
#include "downloader.h"
#include "url_utils.h"
#endif /* USE_GRABBER */


/******************************************************************************/
/*                                                                            */
/*                             Valhalla Handling                              */
/*                                                                            */
/******************************************************************************/

static void
valhalla_mrproper (valhalla_t *handle)
{
  unsigned int i;
  fifo_queue_t *fifo_o;

  fifo_queue_t *fifo_i[] = {
    vh_scanner_fifo_get (handle->scanner),
    vh_dbmanager_fifo_get (handle->dbmanager),
    vh_dispatcher_fifo_get (handle->dispatcher),
    vh_parser_fifo_get (handle->parser),
#ifdef USE_GRABBER
    vh_grabber_fifo_get (handle->grabber),
    vh_downloader_fifo_get (handle->downloader),
#endif /* USE_GRABBER */
  };

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

#ifdef USE_GRABBER
  /* remove all previous contexts */
  vh_dbmanager_db_dlcontext_delete (handle->dbmanager);
#endif /* USE_GRABBER */

  fifo_o = vh_fifo_queue_new ();
  if (!fifo_o)
    return;

  /*
   * The same data pointer can exist in several queues at the same time.
   * The goal is to identify all unique entries from all queues and to save
   * these entries in the fifo_o queue.
   * Then, all data pointers can be safety freed (prevents double free).
   */
  for (i = 0; i < ARRAY_NB_ELEMENTS (fifo_i) && fifo_i[i]; i++)
  {
    int e;
    void *data;

    vh_fifo_queue_push (fifo_i[i],
                        FIFO_QUEUE_PRIORITY_NORMAL, ACTION_CLEANUP_END, NULL);

    do
    {
      e = ACTION_NO_OPERATION;
      data = NULL;
      vh_fifo_queue_pop (fifo_i[i], &e, &data);

      switch (e)
      {
      case ACTION_DB_INSERT_P:
      case ACTION_DB_INSERT_G:
      case ACTION_DB_UPDATE_P:
      case ACTION_DB_UPDATE_G:
      case ACTION_DB_END:
      case ACTION_DB_NEWFILE:
      {
        file_data_t *file = data;
        if (!file || file->clean_f)
          break;

        file->clean_f = 1;
        vh_fifo_queue_push (fifo_o, FIFO_QUEUE_PRIORITY_NORMAL, e, data);

#ifdef USE_GRABBER
        /* save downloader context */
        if (file->step < STEP_ENDING && file->list_downloader)
          vh_dbmanager_db_dlcontext_save (handle->dbmanager, file);
#endif /* USE_GRABBER */
        break;
      }

      default:
        break;
      }
    }
    while (e != ACTION_CLEANUP_END);
  }

  vh_queue_cleanup (fifo_o);
  vh_fifo_queue_free (fifo_o);
}

void
valhalla_wait (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  vh_scanner_wait (handle->scanner);

  vh_dbmanager_stop (handle->dbmanager);
  vh_dispatcher_stop (handle->dispatcher);
  vh_parser_stop (handle->parser);
#ifdef USE_GRABBER
  vh_grabber_stop (handle->grabber);
  vh_downloader_stop (handle->downloader);
#endif /* USE_GRABBER */

  valhalla_mrproper (handle);
}

static void
valhalla_force_stop (valhalla_t *handle)
{
  vh_scanner_stop (handle->scanner);
  vh_dbmanager_stop (handle->dbmanager);
  vh_dispatcher_stop (handle->dispatcher);
  vh_parser_stop (handle->parser);
#ifdef USE_GRABBER
  vh_grabber_stop (handle->grabber);
  vh_downloader_stop (handle->downloader);
#endif /* USE_GRABBER */

  valhalla_mrproper (handle);
}

void
valhalla_uninit (valhalla_t *handle)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return;

  valhalla_force_stop (handle);

  vh_scanner_uninit (handle->scanner);
  vh_dbmanager_uninit (handle->dbmanager);
  vh_dispatcher_uninit (handle->dispatcher);
  vh_parser_uninit (handle->parser);
#ifdef USE_GRABBER
  vh_grabber_uninit (handle->grabber);
  vh_downloader_uninit (handle->downloader);

  vh_url_global_uninit ();
#endif /* USE_GRABBER */

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

  res = vh_scanner_run (handle->scanner, loop, timeout, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = vh_dbmanager_run (handle->dbmanager, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = vh_dispatcher_run (handle->dispatcher, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = vh_parser_run (handle->parser, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

#ifdef USE_GRABBER
  res = vh_grabber_run (handle->grabber, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;

  res = vh_downloader_run (handle->downloader, priority);
  if (res)
    return VALHALLA_ERROR_THREAD;
#endif /* USE_GRABBER */

  return VALHALLA_SUCCESS;
}

void
valhalla_path_add (valhalla_t *handle, const char *location, int recursive)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, location);

  if (!handle || !location)
    return;

  vh_scanner_path_add (handle->scanner, location, recursive);
}

void
valhalla_suffix_add (valhalla_t *handle, const char *suffix)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, suffix);

  if (!handle || !suffix)
    return;

  vh_scanner_suffix_add (handle->scanner, suffix);
}

void
valhalla_bl_keyword_add (valhalla_t *handle, const char *keyword)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : %s", __FUNCTION__, keyword);

  if (!handle || !keyword)
    return;

  vh_parser_bl_keyword_add (handle->parser, keyword);
}

void
valhalla_downloader_dest_set (valhalla_t *handle,
                              valhalla_dl_t dl, const char *dst)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, "%s : (%i) %s", __FUNCTION__, dl, dst);

  if (!handle || !dst)
    return;

#ifdef USE_GRABBER
  vh_downloader_destination_set (handle->downloader, dl, dst);
#else
  valhalla_log (VALHALLA_MSG_WARNING,
                "This function is usable only with grabbing support!");
#endif /* USE_GRABBER */
}

void
valhalla_grabber_state_set (valhalla_t *handle, const char *id, int enable)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !id)
    return;

#ifdef USE_GRABBER
  vh_grabber_state_set (handle->grabber, id, enable);
#else
  valhalla_log (VALHALLA_MSG_WARNING,
                "This function is usable only with grabbing support!");
#endif /* USE_GRABBER */
}

const char *
valhalla_grabber_list_get (valhalla_t *handle, const char *id)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle)
    return NULL;

#ifdef USE_GRABBER
  return vh_grabber_list_get (handle->grabber, id);
#else
  valhalla_log (VALHALLA_MSG_WARNING,
                "This function is usable only with grabbing support!");
  return NULL;
#endif /* USE_GRABBER */
}

void
valhalla_verbosity (valhalla_verb_t level)
{
  vh_log_verb (level);
}

valhalla_t *
valhalla_init (const char *db,
               unsigned int parser_nb, int decrapifier, unsigned int commit_int)
{
  static int preinit = 0;
  valhalla_t *handle;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!db)
    return NULL;

  handle = calloc (1, sizeof (valhalla_t));
  if (!handle)
    return NULL;

#ifdef USE_GRABBER
  vh_url_global_init ();
#endif /* USE_GRABBER */

  handle->dispatcher = vh_dispatcher_init (handle);
  if (!handle->dispatcher)
    goto err;

  handle->parser = vh_parser_init (handle, parser_nb, decrapifier);
  if (!handle->parser)
    goto err;

#ifdef USE_GRABBER
  handle->grabber = vh_grabber_init (handle);
  if (!handle->grabber)
    goto err;

  handle->downloader = vh_downloader_init (handle);
  if (!handle->downloader)
    goto err;
#endif /* USE_GRABBER */

  handle->scanner = vh_scanner_init (handle);
  if (!handle->scanner)
    goto err;

  handle->dbmanager = vh_dbmanager_init (handle, db, commit_int);
  if (!handle->dbmanager)
    goto err;

  if (!preinit)
  {
    av_log_set_level (AV_LOG_FATAL);
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

int valhalla_db_metalist_get (valhalla_t *handle,
                              valhalla_db_item_t *search,
                              valhalla_db_restrict_t *restriction,
                              int (*result_cb) (void *data,
                                                valhalla_db_metares_t *res),
                              void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !search || !result_cb)
    return -1;

  return vh_dbmanager_db_metalist_get (handle->dbmanager,
                                       search, restriction, result_cb, data);
}

int valhalla_db_filelist_get (valhalla_t *handle,
                              valhalla_file_type_t filetype,
                              valhalla_db_restrict_t *restriction,
                              int (*result_cb) (void *data,
                                                valhalla_db_fileres_t *res),
                              void *data)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !result_cb)
    return -1;

  return vh_dbmanager_db_filelist_get (handle->dbmanager,
                                       filetype, restriction, result_cb, data);
}

int
valhalla_db_file_get (valhalla_t *handle,
                      int64_t id, const char *path,
                      valhalla_db_restrict_t *restriction,
                      valhalla_db_filemeta_t **res)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!handle || !res)
    return -1;

  return
    vh_dbmanager_db_file_get (handle->dbmanager, id, path, restriction, res);
}
