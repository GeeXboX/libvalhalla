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

#include <sqlite3.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "sql_statements.h"
#include "database.h"
#include "logs.h"


#define SQL_BUFFER 4096

typedef struct stmt_list_s {
  const char   *sql;
  sqlite3_stmt *stmt;
} stmt_list_t;

struct database_s {
  sqlite3      *db;
  char         *path;
  stmt_list_t  *stmts;
  sqlite3_stmt *stmt_file;
};

typedef enum database_stmt {
  STMT_SELECT_AUTHOR,
  STMT_SELECT_AUTHOR_BY_ALBUM,
  STMT_SELECT_ALBUM,
  STMT_SELECT_ALBUM_BY_AUTHOR,
  STMT_SELECT_ALBUM_BY_GENRE,
  STMT_SELECT_GENRE,

  STMT_SELECT_FILE_MTIME,
  STMT_SELECT_AUTHOR_ID,
  STMT_SELECT_ALBUM_ID,
  STMT_SELECT_GENRE_ID,
  STMT_INSERT_FILE,
  STMT_INSERT_AUTHOR,
  STMT_INSERT_ALBUM,
  STMT_INSERT_GENRE,
  STMT_INSERT_ALLOC_AUTHOR_ALBUM,
  STMT_INSERT_ALLOC_GENRE_ALBUM,
  STMT_UPDATE_FILE,
  STMT_DELETE_FILE,

  STMT_CLEANUP_GENRE,
  STMT_CLEANUP_AUTHOR,
  STMT_CLEANUP_ALBUM,
  STMT_CLEANUP_ALLOC_AUTHOR_ALBUM,
  STMT_CLEANUP_ALLOC_GENRE_ALBUM,

  STMT_UPDATE_FILE_CHECKED_CLEAR,
  STMT_SELECT_FILE_CHECKED_CLEAR,
  STMT_BEGIN_TRANSACTION,
  STMT_END_TRANSACTION,
} database_stmt_t;

static const stmt_list_t g_stmts[] = {
  [STMT_SELECT_AUTHOR]              = { SELECT_AUTHOR,              NULL },
  [STMT_SELECT_AUTHOR_BY_ALBUM]     = { SELECT_AUTHOR_BY_ALBUM,     NULL },
  [STMT_SELECT_ALBUM]               = { SELECT_ALBUM,               NULL },
  [STMT_SELECT_ALBUM_BY_AUTHOR]     = { SELECT_ALBUM_BY_AUTHOR,     NULL },
  [STMT_SELECT_ALBUM_BY_GENRE]      = { SELECT_ALBUM_BY_GENRE,      NULL },
  [STMT_SELECT_GENRE]               = { SELECT_GENRE,               NULL },

  [STMT_SELECT_FILE_MTIME]          = { SELECT_FILE_MTIME,          NULL },
  [STMT_SELECT_AUTHOR_ID]           = { SELECT_AUTHOR_ID,           NULL },
  [STMT_SELECT_ALBUM_ID]            = { SELECT_ALBUM_ID,            NULL },
  [STMT_SELECT_GENRE_ID]            = { SELECT_GENRE_ID,            NULL },
  [STMT_INSERT_FILE]                = { INSERT_FILE,                NULL },
  [STMT_INSERT_AUTHOR]              = { INSERT_AUTHOR,              NULL },
  [STMT_INSERT_ALBUM]               = { INSERT_ALBUM,               NULL },
  [STMT_INSERT_GENRE]               = { INSERT_GENRE,               NULL },
  [STMT_INSERT_ALLOC_AUTHOR_ALBUM]  = { INSERT_ALLOC_AUTHOR_ALBUM,  NULL },
  [STMT_INSERT_ALLOC_GENRE_ALBUM]   = { INSERT_ALLOC_GENRE_ALBUM,   NULL },
  [STMT_UPDATE_FILE]                = { UPDATE_FILE,                NULL },
  [STMT_DELETE_FILE]                = { DELETE_FILE,                NULL },

  [STMT_CLEANUP_GENRE]              = { CLEANUP_GENRE,              NULL },
  [STMT_CLEANUP_AUTHOR]             = { CLEANUP_AUTHOR,             NULL },
  [STMT_CLEANUP_ALBUM]              = { CLEANUP_ALBUM,              NULL },
  [STMT_CLEANUP_ALLOC_AUTHOR_ALBUM] = { CLEANUP_ALLOC_AUTHOR_ALBUM, NULL },
  [STMT_CLEANUP_ALLOC_GENRE_ALBUM]  = { CLEANUP_ALLOC_GENRE_ALBUM,  NULL },

  [STMT_UPDATE_FILE_CHECKED_CLEAR]  = { UPDATE_FILE_CHECKED_CLEAR,  NULL },
  [STMT_SELECT_FILE_CHECKED_CLEAR]  = { SELECT_FILE_CHECKED_CLEAR,  NULL },
  [STMT_BEGIN_TRANSACTION]          = { BEGIN_TRANSACTION,          NULL },
  [STMT_END_TRANSACTION]            = { END_TRANSACTION,            NULL },
};

#define STMT_GET(id) database->stmts[id].stmt

/******************************************************************************/
/*                                                                            */
/*                                 Internal                                   */
/*                                                                            */
/******************************************************************************/

void
database_begin_transaction (database_t *database)
{
  sqlite3_step (STMT_GET (STMT_BEGIN_TRANSACTION));
  sqlite3_reset (STMT_GET (STMT_BEGIN_TRANSACTION));
}

void
database_end_transaction (database_t *database)
{
  sqlite3_step (STMT_GET (STMT_END_TRANSACTION));
  sqlite3_reset (STMT_GET (STMT_END_TRANSACTION));
}

void
database_step_transaction (database_t *database,
                           unsigned int interval, int value)
{
  if (value && !(value % interval))
  {
    database_end_transaction (database);
    database_begin_transaction (database);
  }
}

static void
database_create_table (database_t *database)
{
  char *msg = NULL;

  /* Create tables */
  sqlite3_exec (database->db, CREATE_TABLE_FILE
                              CREATE_TABLE_AUTHOR
                              CREATE_TABLE_ALBUM
                              CREATE_TABLE_GENRE
                              CREATE_TABLE_ALLOC_AUTHOR_ALBUM
                              CREATE_TABLE_ALLOC_GENRE_ALBUM,
                NULL, NULL, &msg);
  if (msg)
    goto err;

  /* Create indexes */
  sqlite3_exec (database->db, CREATE_INDEX_FILE_PATH
                              CREATE_INDEX_AUTHOR_NAME
                              CREATE_INDEX_ALBUM_NAME
                              CREATE_INDEX_GENRE_NAME,
                NULL, NULL, &msg);
  if (msg)
    goto err;

  return;

 err:
  valhalla_log (VALHALLA_MSG_ERROR, "%s", msg);
  sqlite3_free (msg);
}

static int
database_prepare_stmt (database_t *database)
{
  int i;

  database->stmts = malloc (sizeof (g_stmts));
  if (!database->stmts)
    return -1;

  memcpy (database->stmts, g_stmts, sizeof (g_stmts));

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_stmts); i++)
  {
    int res = sqlite3_prepare_v2 (database->db, database->stmts[i].sql,
                                  -1, &database->stmts[i].stmt, NULL);
    if (res != SQLITE_OK)
    {
      valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
      return -1;
    }
  }

  return 0;
}

static int
database_table_get_id (database_t *database,
                       sqlite3_stmt *stmt, const char *name)
{
  int res, err = -1;
  int64_t val = 0;

  if (!name)
    return 0;

  res = sqlite3_bind_text (stmt, 1, name, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out;

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
    val = sqlite3_column_int64 (stmt, 0);

  sqlite3_clear_bindings (stmt);
  err = 0;

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  sqlite3_reset (stmt);
  return val;
}

static int64_t
database_insert_name (database_t *database,
                      sqlite3_stmt *stmt, const char *name)
{
  int res, err = -1;
  int64_t val = 0, val_tmp;

  if (!name)
    return 0;

  res = sqlite3_bind_text (stmt, 1, name, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out;

  val_tmp = sqlite3_last_insert_rowid (database->db);
  res = sqlite3_step (stmt);
  if (res != SQLITE_DONE)
    goto out;
  val = sqlite3_last_insert_rowid (database->db);

  if (val == val_tmp)
    val = 0;

  sqlite3_clear_bindings (stmt);
  err = 0;

 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  sqlite3_reset (stmt);
  return val;
}

static inline int64_t
database_author_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_AUTHOR), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_AUTHOR_ID), name);

  return val;
}

static inline int64_t
database_album_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_ALBUM), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_ALBUM_ID), name);

  return val;
}

static inline int64_t
database_genre_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_GENRE), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_GENRE_ID), name);

  return val;
}

static void
database_alloc_insert (sqlite3_stmt *stmt,
                       int64_t field1_id, int64_t field2_id)
{
  int res, err = -1;

  res = sqlite3_bind_int64 (stmt, 1, field1_id);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int64 (stmt, 2, field2_id);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

 out_clear:
  sqlite3_clear_bindings (stmt);
 out_reset:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    valhalla_log (VALHALLA_MSG_ERROR,
                  "%s", sqlite3_errmsg (sqlite3_db_handle (stmt)));
  sqlite3_reset (stmt);
}

static void
database_file_insert (database_t *database, parser_data_t *data,
                      int64_t author_id, int64_t album_id, int64_t genre_id)
{
  int res, err = -1;

  res = sqlite3_bind_text (STMT_GET (STMT_INSERT_FILE), 1,
                           data->file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int (STMT_GET (STMT_INSERT_FILE), 2, data->mtime);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_bind_text (STMT_GET (STMT_INSERT_FILE), 3,
                           metadata_get (data->meta, "title"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_bind_text (STMT_GET (STMT_INSERT_FILE), 4,
                           metadata_get (data->meta, "year"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_bind_text (STMT_GET (STMT_INSERT_FILE), 5,
                           metadata_get (data->meta, "track"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  if (author_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_INSERT_FILE), 6, author_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  if (album_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_INSERT_FILE), 7, album_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  if (genre_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_INSERT_FILE), 8, genre_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  res = sqlite3_step (STMT_GET (STMT_INSERT_FILE));
  if (res == SQLITE_DONE)
    err = 0;

 out_clear:
  sqlite3_clear_bindings (STMT_GET (STMT_INSERT_FILE));
 out_reset:
  sqlite3_reset (STMT_GET (STMT_INSERT_FILE));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
database_file_data_insert (database_t *database, parser_data_t *data)
{
  int64_t author_id = 0, album_id = 0, genre_id = 0;

  if (data->meta)
  {
    author_id =
      database_author_insert (database, metadata_get (data->meta, "author"));
    album_id =
      database_album_insert (database, metadata_get (data->meta, "album"));
    genre_id =
      database_genre_insert (database, metadata_get (data->meta, "genre"));

    if (author_id && album_id)
      database_alloc_insert (STMT_GET (STMT_INSERT_ALLOC_AUTHOR_ALBUM),
                             author_id, album_id);
    if (genre_id && album_id)
      database_alloc_insert (STMT_GET (STMT_INSERT_ALLOC_GENRE_ALBUM),
                             genre_id, album_id);
  }

  database_file_insert (database, data, author_id, album_id, genre_id);
}

static void
database_file_update (database_t *database, parser_data_t *data,
                      int64_t author_id, int64_t album_id, int64_t genre_id)
{
  int res, err = -1;

  res = sqlite3_bind_int (STMT_GET (STMT_UPDATE_FILE), 1, data->mtime);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_text (STMT_GET (STMT_UPDATE_FILE), 2,
                           metadata_get (data->meta, "title"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_bind_text (STMT_GET (STMT_UPDATE_FILE), 4,
                           metadata_get (data->meta, "year"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_bind_text (STMT_GET (STMT_UPDATE_FILE), 5,
                           metadata_get (data->meta, "track"),
                           -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  if (author_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_UPDATE_FILE), 5, author_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  if (album_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_UPDATE_FILE), 6, album_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  if (genre_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_UPDATE_FILE), 7, genre_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  res = sqlite3_bind_text (STMT_GET (STMT_UPDATE_FILE), 8,
                           data->file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_step (STMT_GET (STMT_UPDATE_FILE));
  if (res == SQLITE_DONE)
    err = 0;

 out_clear:
  sqlite3_clear_bindings (STMT_GET (STMT_UPDATE_FILE));
 out_reset:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  sqlite3_reset (STMT_GET (STMT_UPDATE_FILE));
}

void
database_file_data_update (database_t *database, parser_data_t *data)
{
  int64_t author_id = 0, album_id = 0, genre_id = 0;

  if (data->meta)
  {
    author_id =
      database_author_insert (database, metadata_get (data->meta, "author"));
    album_id =
      database_album_insert (database, metadata_get (data->meta, "album"));
    genre_id =
      database_genre_insert (database, metadata_get (data->meta, "genre"));

    if (author_id && album_id)
      database_alloc_insert (STMT_GET (STMT_INSERT_ALLOC_AUTHOR_ALBUM),
                             author_id, album_id);
    if (genre_id && album_id)
      database_alloc_insert (STMT_GET (STMT_INSERT_ALLOC_GENRE_ALBUM),
                             genre_id, album_id);
  }

  database_file_update (database, data, author_id, album_id, genre_id);
}

void
database_file_data_delete (database_t *database, const char *file)
{
  int res, err = -1;

  res = sqlite3_bind_text (STMT_GET (STMT_DELETE_FILE),
                           1, file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_DELETE_FILE));
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_clear_bindings (STMT_GET (STMT_DELETE_FILE));

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  sqlite3_reset (STMT_GET (STMT_DELETE_FILE));
}

void
database_file_checked_clear (database_t *database)
{
  int res;

  res = sqlite3_step (STMT_GET (STMT_UPDATE_FILE_CHECKED_CLEAR));
  if (res != SQLITE_DONE)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));

  sqlite3_reset (STMT_GET (STMT_UPDATE_FILE_CHECKED_CLEAR));
}

const char *
database_file_get_checked_clear (database_t *database)
{
  int res;

  res = sqlite3_step (STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR));
  if (res == SQLITE_ROW)
    return (const char *) sqlite3_column_text (
                            STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR), 0);

  if (res != SQLITE_DONE)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));

  sqlite3_reset (STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR));
  return NULL;
}

int
database_file_get_mtime (database_t *database, const char *file)
{
  int res, err = -1, val = -1;

  if (!file)
    return -1;

  res = sqlite3_bind_text (STMT_GET (STMT_SELECT_FILE_MTIME),
                           1, file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_SELECT_FILE_MTIME));
  if (res == SQLITE_ROW)
    val = sqlite3_column_int (STMT_GET (STMT_SELECT_FILE_MTIME), 0);

  sqlite3_clear_bindings (STMT_GET (STMT_SELECT_FILE_MTIME));
  err = 0;

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  sqlite3_reset (STMT_GET (STMT_SELECT_FILE_MTIME));
  return val;
}

int
database_cleanup (database_t *database)
{
  int res, val, val_tmp, err = -1;

  val_tmp = sqlite3_total_changes (database->db);

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_GENRE));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_AUTHOR));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ALBUM));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ALLOC_AUTHOR_ALBUM));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ALLOC_GENRE_ALBUM));
  if (res == SQLITE_DONE)
    err = 0;

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  val = sqlite3_total_changes (database->db);
  sqlite3_reset (STMT_GET (STMT_CLEANUP_GENRE));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_AUTHOR));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ALBUM));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ALLOC_AUTHOR_ALBUM));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ALLOC_GENRE_ALBUM));
  return val - val_tmp;
}

void
database_uninit (database_t *database)
{
  int i;

  if (!database)
    return;

  if (database->path)
    free (database->path);

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_stmts); i++)
    if (STMT_GET (i))
      sqlite3_finalize (STMT_GET (i));

  if (database->stmts)
    free (database->stmts);

  if (database->db)
    sqlite3_close (database->db);

  free (database);
}

database_t *
database_init (const char *path)
{
  int res;
  database_t *database;

  if (!path)
    return NULL;

  database = calloc (1, sizeof (database_t));
  if (!database)
    return NULL;

  res = sqlite3_open (path, &database->db);
  if (res)
  {
    valhalla_log (VALHALLA_MSG_ERROR,
                  "Can't open database: %s", sqlite3_errmsg (database->db));
    sqlite3_close (database->db);
    return NULL;
  }

  database->path = strdup (path);
  database_create_table (database);

  res = database_prepare_stmt (database);
  if (res)
  {
    database_uninit (database);
    return NULL;
  }

  return database;
}

/******************************************************************************/
/*                                                                            */
/*                           For Public Selections                            */
/*                                                                            */
/******************************************************************************/

static int
database_common_simple_select (sqlite3_stmt *stmt,
                               int64_t *id, const char **name, int64_t sid)
{
  int err = -1, res, rc = 0;
  static int bind = 0;

  /* bind */
  if (!bind && sid > 0)
  {
    res = sqlite3_bind_int64 (stmt, 1, sid);
    if (res != SQLITE_OK)
    {
      rc = -1;
      goto out;
    }
    bind = 1;
  }

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
  {
    if (id)
      *id = sqlite3_column_int64 (stmt, 0);
    if (name)
      *name = (const char *) sqlite3_column_text (stmt, 1);
    return 0;
  }
  else if (res == SQLITE_DONE)
    rc = -1;

  if (sid)
    sqlite3_clear_bindings (stmt);
  err = 0;

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR,
                  "%s", sqlite3_errmsg (sqlite3_db_handle (stmt)));
  sqlite3_reset (stmt);
  bind = 0;
  return rc;
}

int
database_select_author (database_t *database,
                        int64_t *id, const char **name, int64_t album)
{
  database_stmt_t stmt;

  if (!album)
    stmt = STMT_SELECT_AUTHOR;
  else
    stmt = STMT_SELECT_AUTHOR_BY_ALBUM;

  return database_common_simple_select (STMT_GET (stmt), id, name, album);
}

int
database_select_album (database_t *database, int64_t *id,
                       const char **name, int64_t where_id, int what)
{
  database_stmt_t stmt;

  if (where_id <= 0)
    stmt = STMT_SELECT_ALBUM;
  else if (!what)
    stmt = STMT_SELECT_ALBUM_BY_AUTHOR;
  else
    stmt = STMT_SELECT_ALBUM_BY_GENRE;

  return database_common_simple_select (STMT_GET (stmt), id, name, where_id);
}

int
database_select_genre (database_t *database, int64_t *id, const char **name)
{
  return
    database_common_simple_select (STMT_GET (STMT_SELECT_GENRE), id, name, 0);
}

static void
database_select_file_sql (char *sql, size_t size, const char *part, int *where)
{
  if (strlen (sql) + strlen (part) >= size)
    return;

  if (*where)
    strcat (sql, SELECT_FILE_WHERE_);
  else
    strcat (sql, SELECT_FILE_AND_);

  strcat (sql, part);
  *where = 0;
}

int
database_select_file (database_t *database, valhalla_db_file_t *file,
                      valhalla_db_file_where_t *where)
{
  int err = -1, res, rc = 0;
  static int bind = 0;
  valhalla_db_file_where_t *loop;

  if (!bind)
  {
    int i;
    int col = 1;
    int w = 1;
    char sql[SQL_BUFFER] = SELECT_FILE_FROM;
    rc = -1;

    /* Create SQL query */
    for (loop = where; loop; loop = loop->next)
      for (i = 0; i < 3; i++)
      {
        valhalla_db_cond_t cond;
        const char *sql_null;
        const char *sql_equal;
        const char *sql_notequal;

        switch (i)
        {
        case 0:
          cond = loop->author_w;
          sql_null     = SELECT_FILE_WHERE_AUTHOR_NULL;
          sql_equal    = SELECT_FILE_WHERE_AUTHOR_EQUAL;
          sql_notequal = SELECT_FILE_WHERE_AUTHOR_NOTEQUAL;
          break;

        case 1:
          cond = loop->album_w;
          sql_null     = SELECT_FILE_WHERE_ALBUM_NULL;
          sql_equal    = SELECT_FILE_WHERE_ALBUM_EQUAL;
          sql_notequal = SELECT_FILE_WHERE_ALBUM_NOTEQUAL;
          break;

        case 2:
          cond = loop->genre_w;
          sql_null     = SELECT_FILE_WHERE_GENRE_NULL;
          sql_equal    = SELECT_FILE_WHERE_GENRE_EQUAL;
          sql_notequal = SELECT_FILE_WHERE_GENRE_NOTEQUAL;
          break;

        default:
          continue;
        }

        switch (cond)
        {
        case VALHALLA_DB_NULL:
          database_select_file_sql (sql, sizeof (sql), sql_null, &w);
          break;

        case VALHALLA_DB_EQUAL:
          database_select_file_sql (sql, sizeof (sql), sql_equal, &w);
          break;

        case VALHALLA_DB_NOTEQUAL:
          database_select_file_sql (sql, sizeof (sql), sql_notequal, &w);
          break;

        default:
          break;
        }
      }

    if (strlen (sql) + strlen (SELECT_FILE_ORDER) < sizeof (sql))
      strcat (sql, SELECT_FILE_ORDER);

    valhalla_log (VALHALLA_MSG_VERBOSE, "SQL query : %s", sql);

    res =
      sqlite3_prepare_v2 (database->db, sql, -1, &database->stmt_file, NULL);
    if (res != SQLITE_OK)
      goto out;

    /* Binds */
    for (loop = where; loop; loop = loop->next)
      for (i = 0; i < 3; i++)
      {
        valhalla_db_cond_t cond;
        int64_t id;

        switch (i)
        {
        case 0:
          cond = loop->author_w;
          id = loop->author_id;
          break;

        case 1:
          cond = loop->album_w;
          id = loop->album_id;
          break;

        case 2:
          cond = loop->genre_w;
          id = loop->genre_id;
          break;

        default:
          continue;
        }

        switch (cond)
        {
        case VALHALLA_DB_EQUAL:
        case VALHALLA_DB_NOTEQUAL:
          res = sqlite3_bind_int64 (database->stmt_file, col++, id);
          if (res != SQLITE_OK)
            goto out;
          break;

        default:
          break;
        }
      }

    bind = 1;
  }

  res = sqlite3_step (database->stmt_file);
  if (res == SQLITE_ROW)
  {
    file->id    = sqlite3_column_int64 (database->stmt_file, 0);
    file->path  = (const char *) sqlite3_column_text (database->stmt_file, 1);
    file->title = (const char *) sqlite3_column_text (database->stmt_file, 2);
    file->year  = (const char *) sqlite3_column_text (database->stmt_file, 3);
    file->track = (const char *) sqlite3_column_text (database->stmt_file, 4);
    return 0;
  }
  else if (res == SQLITE_DONE)
    rc = -1;

  err = 0;

 out:
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s",
                  sqlite3_errmsg (sqlite3_db_handle (database->stmt_file)));
  sqlite3_finalize (database->stmt_file);
  bind = 0;
  return rc;
}
