/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009-2011 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sqlite3.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "metadata.h"
#include "sql_statements.h"
#include "database.h"
#include "logs.h"


#define SQL_BUFFER 8192

typedef struct stmt_list_s {
  const char   *sql;
  sqlite3_stmt *stmt;
} stmt_list_t;

typedef struct item_list_s {
  int64_t     id;
  const char *name;
} item_list_t;

struct database_s {
  sqlite3      *db;
  char         *path;
  stmt_list_t  *stmts;
  item_list_t  *file_type;
  int64_t      *groups_id;
  int64_t      *langs_id;
};

#define VHSTMT_MAXCOLS  8

struct valhalla_db_stmt_s {
  char         *sql;
  sqlite3_stmt *stmt;

  unsigned int  cnt;
  const char   *cols[VHSTMT_MAXCOLS];

  union {
    valhalla_db_metares_t metares;
    valhalla_db_fileres_t fileres;
  } u;
};

static const item_list_t g_file_type[] = {
  [VALHALLA_FILE_TYPE_NULL]     = { 0, "null"     },
  [VALHALLA_FILE_TYPE_AUDIO]    = { 0, "audio"    },
  [VALHALLA_FILE_TYPE_IMAGE]    = { 0, "image"    },
  [VALHALLA_FILE_TYPE_PLAYLIST] = { 0, "playlist" },
  [VALHALLA_FILE_TYPE_VIDEO]    = { 0, "video"    },
};

typedef enum database_stmt {
  STMT_SELECT_FILE_INTERRUP,
  STMT_SELECT_FILE_MTIME,
  STMT_SELECT_TYPE_ID,
  STMT_SELECT_META_ID,
  STMT_SELECT_DATA_ID,
  STMT_SELECT_LANG_ID,
  STMT_SELECT_GROUP_ID,
  STMT_SELECT_GRABBER_ID,
  STMT_SELECT_FILE_ID,
  STMT_SELECT_FILE_ID_BY_META,
  STMT_SELECT_FILE_ID_BY_METADATA,
  STMT_SELECT_FILE_GRABBER_NAME,
  STMT_SELECT_FILE_DLCONTEXT,
  STMT_SELECT_ASSOC_FILE_METADATA,
  STMT_INSERT_FILE,
  STMT_INSERT_TYPE,
  STMT_INSERT_META,
  STMT_INSERT_DATA,
  STMT_INSERT_LANG,
  STMT_INSERT_GROUP,
  STMT_INSERT_GRABBER,
  STMT_INSERT_DLCONTEXT,
  STMT_INSERT_ASSOC_FILE_METADATA,
  STMT_INSERT_ASSOC_FILE_GRABBER,
  STMT_UPDATE_FILE,
  STMT_UPDATE_ASSOC_FILE_METADATA,
  STMT_UPDATE_ASSOC_FILE_MD_P,
  STMT_UPDATE_ASSOC_FILE_MD_PM,
  STMT_UPDATE_ASSOC_FILE_MD_PMD,
  STMT_DELETE_FILE,
  STMT_DELETE_ASSOC_FILE_METADATA,
  STMT_DELETE_ASSOC_FILE_METADATA2,
  STMT_DELETE_ASSOC_FILE_GRABBER,
  STMT_DELETE_DLCONTEXT,

  STMT_CLEANUP_ASSOC_FILE_METADATA,
  STMT_CLEANUP_ASSOC_FILE_GRABBER,
  STMT_CLEANUP_META,
  STMT_CLEANUP_DATA,
  STMT_CLEANUP_GRABBER,

  STMT_UPDATE_FILE_CHECKED_CLEAR,
  STMT_SELECT_FILE_CHECKED_CLEAR,
  STMT_UPDATE_FILE_INTERRUP_CLEAR,
  STMT_UPDATE_FILE_INTERRUP_FIX,
  STMT_SELECT_FILE_OUTOFPATH_SET,
  STMT_BEGIN_TRANSACTION,
  STMT_END_TRANSACTION,
} database_stmt_t;

static const stmt_list_t g_stmts[] = {
  [STMT_SELECT_FILE_INTERRUP]        = { SELECT_FILE_INTERRUP,        NULL },
  [STMT_SELECT_FILE_MTIME]           = { SELECT_FILE_MTIME,           NULL },
  [STMT_SELECT_TYPE_ID]              = { SELECT_TYPE_ID,              NULL },
  [STMT_SELECT_META_ID]              = { SELECT_META_ID,              NULL },
  [STMT_SELECT_DATA_ID]              = { SELECT_DATA_ID,              NULL },
  [STMT_SELECT_LANG_ID]              = { SELECT_LANG_ID,              NULL },
  [STMT_SELECT_GROUP_ID]             = { SELECT_GROUP_ID,             NULL },
  [STMT_SELECT_GRABBER_ID]           = { SELECT_GRABBER_ID,           NULL },
  [STMT_SELECT_FILE_ID]              = { SELECT_FILE_ID,              NULL },
  [STMT_SELECT_FILE_ID_BY_META]      = { SELECT_FILE_ID_BY_META,      NULL },
  [STMT_SELECT_FILE_ID_BY_METADATA]  = { SELECT_FILE_ID_BY_METADATA,  NULL },
  [STMT_SELECT_FILE_GRABBER_NAME]    = { SELECT_FILE_GRABBER_NAME,    NULL },
  [STMT_SELECT_FILE_DLCONTEXT]       = { SELECT_FILE_DLCONTEXT,       NULL },
  [STMT_SELECT_ASSOC_FILE_METADATA]  = { SELECT_ASSOC_FILE_METADATA,  NULL },
  [STMT_INSERT_FILE]                 = { INSERT_FILE,                 NULL },
  [STMT_INSERT_TYPE]                 = { INSERT_TYPE,                 NULL },
  [STMT_INSERT_META]                 = { INSERT_META,                 NULL },
  [STMT_INSERT_DATA]                 = { INSERT_DATA,                 NULL },
  [STMT_INSERT_LANG]                 = { INSERT_LANG,                 NULL },
  [STMT_INSERT_GROUP]                = { INSERT_GROUP,                NULL },
  [STMT_INSERT_GRABBER]              = { INSERT_GRABBER,              NULL },
  [STMT_INSERT_DLCONTEXT]            = { INSERT_DLCONTEXT,            NULL },
  [STMT_INSERT_ASSOC_FILE_METADATA]  = { INSERT_ASSOC_FILE_METADATA,  NULL },
  [STMT_INSERT_ASSOC_FILE_GRABBER]   = { INSERT_ASSOC_FILE_GRABBER,   NULL },
  [STMT_UPDATE_FILE]                 = { UPDATE_FILE,                 NULL },
  [STMT_UPDATE_ASSOC_FILE_METADATA]  = { UPDATE_ASSOC_FILE_METADATA,  NULL },
  [STMT_UPDATE_ASSOC_FILE_MD_P]      = { UPDATE_ASSOC_FILE_MD_P,      NULL },
  [STMT_UPDATE_ASSOC_FILE_MD_PM]     = { UPDATE_ASSOC_FILE_MD_PM,     NULL },
  [STMT_UPDATE_ASSOC_FILE_MD_PMD]    = { UPDATE_ASSOC_FILE_MD_PMD,    NULL },
  [STMT_DELETE_FILE]                 = { DELETE_FILE,                 NULL },
  [STMT_DELETE_ASSOC_FILE_METADATA]  = { DELETE_ASSOC_FILE_METADATA,  NULL },
  [STMT_DELETE_ASSOC_FILE_METADATA2] = { DELETE_ASSOC_FILE_METADATA2, NULL },
  [STMT_DELETE_ASSOC_FILE_GRABBER]   = { DELETE_ASSOC_FILE_GRABBER,   NULL },
  [STMT_DELETE_DLCONTEXT]            = { DELETE_DLCONTEXT,            NULL },

  [STMT_CLEANUP_ASSOC_FILE_METADATA] = { CLEANUP_ASSOC_FILE_METADATA, NULL },
  [STMT_CLEANUP_ASSOC_FILE_GRABBER]  = { CLEANUP_ASSOC_FILE_GRABBER,  NULL },
  [STMT_CLEANUP_META]                = { CLEANUP_META,                NULL },
  [STMT_CLEANUP_DATA]                = { CLEANUP_DATA,                NULL },
  [STMT_CLEANUP_GRABBER]             = { CLEANUP_GRABBER,             NULL },

  [STMT_UPDATE_FILE_CHECKED_CLEAR]   = { UPDATE_FILE_CHECKED_CLEAR,   NULL },
  [STMT_SELECT_FILE_CHECKED_CLEAR]   = { SELECT_FILE_CHECKED_CLEAR,   NULL },
  [STMT_UPDATE_FILE_INTERRUP_CLEAR]  = { UPDATE_FILE_INTERRUP_CLEAR,  NULL },
  [STMT_UPDATE_FILE_INTERRUP_FIX]    = { UPDATE_FILE_INTERRUP_FIX,    NULL },
  [STMT_SELECT_FILE_OUTOFPATH_SET]   = { SELECT_FILE_OUTOFPATH_SET,   NULL },
  [STMT_BEGIN_TRANSACTION]           = { BEGIN_TRANSACTION,           NULL },
  [STMT_END_TRANSACTION]             = { END_TRANSACTION,             NULL },
};

#define STMT_GET(id) database->stmts ? database->stmts[id].stmt : NULL

#define VH_DB_BIND_TEXT_OR_GOTO(stmt, col, value, label)            \
  do                                                                \
  {                                                                 \
    res = sqlite3_bind_text (stmt, col, value, -1, SQLITE_STATIC);  \
    if (res != SQLITE_OK)                                           \
      goto label;                                                   \
  }                                                                 \
  while (0)
#define VH_DB_BIND_INT_OR_GOTO(stmt, col, value, label)             \
  do                                                                \
  {                                                                 \
    res = sqlite3_bind_int (stmt, col, value);                      \
    if (res != SQLITE_OK)                                           \
      goto label;                                                   \
  }                                                                 \
  while (0)
#define VH_DB_BIND_INT64_OR_GOTO(stmt, col, value, label)           \
  do                                                                \
  {                                                                 \
    res = sqlite3_bind_int64 (stmt, col, value);                    \
    if (res != SQLITE_OK)                                           \
      goto label;                                                   \
  }                                                                 \
  while (0)

/******************************************************************************/
/*                                                                            */
/*                                 Internal                                   */
/*                                                                            */
/******************************************************************************/

static void
database_vhstmt_free (valhalla_db_stmt_t *vhstmt)
{
  if (!vhstmt)
    return;

  if (vhstmt->sql)
    free (vhstmt->sql);
  free (vhstmt);
}

/*
 * This function is a replacement of sqlite3_exec() which should not be used
 * anymore. Only one SQL query must be passed in the argument.
 */
static int
database_sql_exec (sqlite3 *db, const char *sql,
                   valhalla_db_stmt_t *vhstmt, char **errmsg)
{
  int res;
  sqlite3_stmt *stmt = NULL;

  if (vhstmt)
    stmt = vhstmt->stmt;

  /* new query ? */
  if (!stmt)
  {
    res = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
    if (res != SQLITE_OK)
      goto out;
  }

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW && vhstmt)
  {
    unsigned int i;

    vhstmt->stmt = stmt;
    vhstmt->cnt  = sqlite3_column_count (stmt);
    for (i = 0; i < vhstmt->cnt && i < VHSTMT_MAXCOLS; i++)
      vhstmt->cols[i] = (const char *) sqlite3_column_text (stmt, i);

    return 0;
  }

 out:
  if (res != SQLITE_DONE && res != SQLITE_OK)
  {
    const char *err = sqlite3_errmsg (db);
    if (err)
      *errmsg = strdup (err);
  }

  database_vhstmt_free (vhstmt);
  sqlite3_finalize (stmt);
  return 1;
}

static valhalla_meta_grp_t
database_group_get (database_t *database, int64_t id)
{
  unsigned int i;

  if (!database)
    return VALHALLA_META_GRP_NIL;

  for (i = 0; i < vh_metadata_group_size; i++)
    if (database->groups_id[i] == id)
      return i;

  return VALHALLA_META_GRP_NIL;
}

static int64_t
database_groupid_get (database_t *database, valhalla_meta_grp_t grp)
{
  if (!database)
    return 0;

  if (grp < vh_metadata_group_size)
    return database->groups_id[grp];

  return 0;
}

static valhalla_lang_t
database_lang_get (database_t *database, int64_t id)
{
  unsigned int i;

  if (!database)
    return VALHALLA_LANG_UNDEF;

  for (i = 0; i < vh_metadata_lang_size; i++)
    if (database->langs_id[i] == id)
      return i;

  return VALHALLA_LANG_UNDEF;
}

static int64_t
database_langid_get (database_t *database, valhalla_lang_t lang)
{
  if (!database)
    return 0;

  if (lang >= 0 && lang < (int) vh_metadata_lang_size)
    return database->langs_id[lang];

  return 0;
}

static valhalla_file_type_t
database_file_type_get (database_t *database, int64_t id)
{
  unsigned int i;

  if (!database)
    return VALHALLA_FILE_TYPE_NULL;

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_file_type); i++)
    if (database->file_type[i].id == id)
      return i;

  return VALHALLA_FILE_TYPE_NULL;
}

static int64_t
database_file_typeid_get (database_t *database, valhalla_file_type_t type)
{
  if (!database)
    return 0;

  if (type < ARRAY_NB_ELEMENTS (g_file_type))
    return database->file_type[type].id;

  return 0;
}

/*
 * The query plan is useful in order to found the best way for indexing
 * the tables.
 */
static void
database_query_plan (database_t *database, const char *sql)
{
  int row = 0;
  size_t size;
  const char *plan = "EXPLAIN QUERY PLAN ";
  valhalla_db_stmt_t *vhstmt;

  if (!vh_log_test (VALHALLA_MSG_VERBOSE))
    return;

  if (!sql)
    return;

  vhstmt = calloc (1, sizeof (*vhstmt));
  if (!vhstmt)
    return;

  size = strlen (sql) + strlen (plan) + 1;
  vhstmt->sql = malloc (size);
  if (!vhstmt->sql)
  {
    free (vhstmt);
    return;
  }

  snprintf (vhstmt->sql, size, "%s%s", plan, sql);
  while (!database_sql_exec (database->db, vhstmt->sql, vhstmt, NULL))
  {
    if (!row)
    {
      row = 1;
      vh_log (VALHALLA_MSG_VERBOSE, "Query plan for : %s", sql);
    }

    vh_log (VALHALLA_MSG_VERBOSE, "| %s | %s | %s",
            vhstmt->cols[0], vhstmt->cols[1], vhstmt->cols[2]);
  }

  if (row)
    vh_log (VALHALLA_MSG_VERBOSE, "");
}

static int
database_prepare_stmt (database_t *database)
{
  unsigned int i;

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
      vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
      return -1;
    }

    database_query_plan (database, database->stmts[i].sql);
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

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, name, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
    val = sqlite3_column_int64 (stmt, 0);

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
  err = 0;
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static inline int64_t
database_step_rowid (database_t *database,
                     sqlite3_stmt *stmt, int *res, int *err)
{
  int64_t val = 0, val_tmp;

  val_tmp = sqlite3_last_insert_rowid (database->db);
  *res = sqlite3_step (stmt);
  if (*res == SQLITE_DONE)
  {
    *err = 0;
    val = sqlite3_last_insert_rowid (database->db);

    if (val == val_tmp)
      return 0;
  }

  return val;
}

static int64_t
database_insert_name (database_t *database,
                      sqlite3_stmt *stmt, const char *name)
{
  int res, err = -1;
  int64_t val = 0;

  if (!name)
    return 0;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, name, out);

  val = database_step_rowid (database, stmt, &res, &err);

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static int64_t
database_insert_data (database_t *database,
                      sqlite3_stmt *stmt, const char *data, int64_t id)
{
  int res, err = -1;
  int64_t val = 0;

  if (!data)
    return 0;

  VH_DB_BIND_TEXT_OR_GOTO  (stmt, 1, data, out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, id,   out_clear);

  val = database_step_rowid (database, stmt, &res, &err);

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static int64_t
database_insert_lang (database_t *database,
                      sqlite3_stmt *stmt, const char *lshort, const char *llong)
{
  int res, err = -1;
  int64_t val = 0;

  if (!lshort)
    return 0;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, lshort, out);
  VH_DB_BIND_TEXT_OR_GOTO (stmt, 2, llong,  out_clear);

  val = database_step_rowid (database, stmt, &res, &err);

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static inline int64_t
database_type_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_TYPE), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_TYPE_ID), name);

  return val;
}

static inline int64_t
database_meta_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_META), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_META_ID), name);

  return val;
}

static inline int64_t
database_data_insert (database_t *database, const char *value, int64_t langid)
{
  int64_t val;
  val =
    database_insert_data (database, STMT_GET (STMT_INSERT_DATA), value, langid);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), value);

  return val;
}

static inline int64_t
database_lang_insert (database_t *database,
                      const char *lshort, const char *llong)
{
  int64_t val;
  val =
    database_insert_lang (database, STMT_GET (STMT_INSERT_LANG), lshort, llong);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_LANG_ID), lshort);

  return val;
}

static inline int64_t
database_group_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_GROUP), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_GROUP_ID), name);

  return val;
}

static inline int64_t
database_grabber_insert (database_t *database, const char *name)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_GRABBER), name);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_GRABBER_ID), name);

  return val;
}

static void
database_assoc_filemd_insert (database_t *database,
                              int64_t file_id, int64_t meta_id,
                              int64_t data_id, int64_t group_id,
                              int ext, int priority)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_INSERT_ASSOC_FILE_METADATA);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id,  out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, meta_id,  out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 3, data_id,  out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 4, group_id, out_clear);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 5, ext,      out_clear);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 6, priority, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_assoc_filemd_update (database_t *database,
                              int64_t file_id, int64_t meta_id,
                              int64_t data_id, int64_t group_id,
                              int ext, int priority)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_UPDATE_ASSOC_FILE_METADATA);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, group_id, out);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 2, ext,      out_clear);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 3, priority, out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 4, file_id,  out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 5, meta_id,  out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 6, data_id,  out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_assoc_filemd_update_p (database_t *database,
                                int64_t file_id, int64_t meta_id,
                                int64_t data_id, int priority)
{
  int res, err = -1;
  sqlite3_stmt *stmt;

  if (meta_id)
  {
    if (data_id)
    {
      stmt = STMT_GET (STMT_UPDATE_ASSOC_FILE_MD_PMD);
      VH_DB_BIND_INT64_OR_GOTO (stmt, 4, data_id, out_clear);
    }
    else
      stmt = STMT_GET (STMT_UPDATE_ASSOC_FILE_MD_PM);

    VH_DB_BIND_INT64_OR_GOTO (stmt, 3, meta_id, out_clear);
  }
  else
    stmt = STMT_GET (STMT_UPDATE_ASSOC_FILE_MD_P);

  VH_DB_BIND_INT_OR_GOTO   (stmt, 1, priority, out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, file_id,  out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_assoc_filemd_delete (database_t *database,
                              int64_t file_id, int64_t meta_id,
                              int64_t data_id)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_DELETE_ASSOC_FILE_METADATA2);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id, out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, meta_id, out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 3, data_id, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static int
database_assoc_filemd_get (database_t *database,
                           int64_t file_id, int64_t meta_id, int64_t data_id,
                           int64_t *group_id, int *ext)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_ASSOC_FILE_METADATA);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id, out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, meta_id, out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 3, data_id, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
  {
    *group_id = sqlite3_column_int64 (stmt, 0);
    *ext      = sqlite3_column_int (stmt, 1);
    err = 0;
  }

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return err;
}

static int64_t
database_file_id_by_metadata (database_t *database, const char *path,
                              const char *meta, const char *data)
{
  int64_t val = 0;
  int res, err = -1;
  sqlite3_stmt *stmt;

  if (data)
    stmt = STMT_GET (STMT_SELECT_FILE_ID_BY_METADATA);
  else
    stmt = STMT_GET (STMT_SELECT_FILE_ID_BY_META);

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, path, out);
  VH_DB_BIND_TEXT_OR_GOTO (stmt, 2, meta, out_clear);
  if (data)
    VH_DB_BIND_TEXT_OR_GOTO (stmt, 3, data, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
    val = sqlite3_column_int64 (stmt, 0);

  err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static void
database_assoc_filegrab_insert (database_t *database,
                                int64_t file_id, int64_t grabber_id)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_INSERT_ASSOC_FILE_GRABBER);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id,    out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, grabber_id, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_file_insert (database_t *database, file_data_t *data)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_INSERT_FILE);

  VH_DB_BIND_TEXT_OR_GOTO  (stmt, 1, data->file.path,  out);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 2, data->file.mtime, out_clear);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 3, data->outofpath,  out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_file_update (database_t *database, file_data_t *data, int64_t type_id)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_UPDATE_FILE);

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, data->file.mtime, out);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 2, data->outofpath,  out_clear);

  if (type_id)
    VH_DB_BIND_INT64_OR_GOTO (stmt, 3, type_id, out_clear);

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 4, data->file.path, out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

static void
database_file_metadata (database_t *database,
                        int64_t file_id, metadata_t *meta, int ext)
{
  int64_t meta_id = 0, data_id = 0, group_id = 0;
  const metadata_t *tag = NULL;

  if (!file_id || !meta)
    return;

  while (!vh_metadata_get (meta, "", METADATA_IGNORE_SUFFIX, &tag))
  {
    int64_t lang_id;

    meta_id  = database_meta_insert (database, tag->name);
    lang_id  = database_langid_get  (database, tag->lang);
    data_id  = database_data_insert (database, tag->value, lang_id);
    group_id = database_groupid_get (database, tag->group);

    database_assoc_filemd_insert (database,
                                  file_id, meta_id, data_id,
                                  group_id, ext, tag->priority);
  }
}

static void
database_file_data (database_t *database, file_data_t *data, int insert)
{
  int64_t file_id, type_id;

  if (insert)
    database_file_insert (database, data);
  else
  {
    char v[32];
    metadata_t *meta = NULL;
    const metadata_plist_t pl = {
      .metadata = NULL,
      .priority = VALHALLA_METADATA_PL_HIGHEST
    };

    type_id = database_file_typeid_get (database, data->file.type);
    database_file_update (database, data, type_id);
    file_id = database_table_get_id (database, STMT_GET (STMT_SELECT_FILE_ID),
                                     data->file.path);
    database_file_metadata (database, file_id, data->meta_parser, 0);

    /*
     * handle filesize like a metadata
     *
     * TODO: it should be made somewhere else. It is acceptable for now
     *       because there is only one property which is added in this way.
     */
    snprintf (v, sizeof (v), "%"PRIi64, data->file.size);
    vh_metadata_add_auto (&meta, VALHALLA_METADATA_FILESIZE,
                          v, VALHALLA_LANG_UNDEF, &pl);
    database_file_metadata (database, file_id, meta, 0);
    vh_metadata_free (meta);
  }
}

static void
database_file_grab (database_t *database, file_data_t *data)
{
  int64_t file_id, grabber_id;

  file_id = database_table_get_id (database,
                                   STMT_GET (STMT_SELECT_FILE_ID),
                                   data->file.path);
  database_file_metadata (database, file_id, data->meta_grabber, 0);

  if (!data->grabber_name)
    return;

  grabber_id = database_grabber_insert (database, data->grabber_name);
  database_assoc_filegrab_insert (database, file_id, grabber_id);
}

void
vh_database_file_insert (database_t *database, file_data_t *data)
{
  database_file_data (database, data, 1);
}

void
vh_database_file_data_update (database_t *database, file_data_t *data)
{
  database_file_data (database, data, 0);
}

void
vh_database_file_grab_insert (database_t *database, file_data_t *data)
{
  database_file_grab (database, data);
}

void
vh_database_file_grab_update (database_t *database, file_data_t *data)
{
  database_file_grab (database, data);
}

void
vh_database_file_delete (database_t *database, const char *file)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_DELETE_FILE);

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
vh_database_file_data_delete (database_t *database, const char *file)
{
  int64_t file_id;
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_DELETE_ASSOC_FILE_METADATA);

  file_id = database_table_get_id (database,
                                   STMT_GET (STMT_SELECT_FILE_ID), file);
  if (!file_id)
    return;

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
vh_database_file_grab_delete (database_t *database, const char *file)
{
  int64_t file_id;
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_DELETE_ASSOC_FILE_GRABBER);

  file_id = database_table_get_id (database,
                                   STMT_GET (STMT_SELECT_FILE_ID), file);
  if (!file_id)
    return;

  VH_DB_BIND_INT64_OR_GOTO (stmt, 1, file_id, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

int64_t
vh_database_file_get_mtime (database_t *database, const char *file)
{
  int res, err = -1;
  int64_t val = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_MTIME);

  if (!file)
    return -1;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
    val = sqlite3_column_int64 (stmt, 0);

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
  err = 0;
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

void
vh_database_file_get_grabber (database_t *database,
                              const char *file, list_t *l)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_GRABBER_NAME);

  if (!file || !l)
    return;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  while (sqlite3_step (stmt) == SQLITE_ROW)
  {
    const char *grabber_name = (const char *) sqlite3_column_text (stmt, 0);
    if (grabber_name)
      vh_list_append (l, grabber_name, strlen (grabber_name) + 1);
  }

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
  err = 0;
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

/******************************************************************************/
/*                          Checked files handling                            */
/******************************************************************************/

void
vh_database_file_checked_clear (database_t *database)
{
  int res;
  sqlite3_stmt *stmt = STMT_GET (STMT_UPDATE_FILE_CHECKED_CLEAR);

  res = sqlite3_step (stmt);

  sqlite3_reset (stmt);
  if (res != SQLITE_DONE)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

const char *
vh_database_file_get_checked_clear (database_t *database, int rst)
{
  int res = SQLITE_DONE;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR);

  if (!rst)
  {
    res = sqlite3_step (stmt);
    if (res == SQLITE_ROW)
      return (const char *) sqlite3_column_text (stmt, 0);
  }

  sqlite3_reset (stmt);
  if (res != SQLITE_DONE && res != SQLITE_ROW)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));

  return NULL;
}

/******************************************************************************/
/*                        Interrupted files handling                          */
/******************************************************************************/

void
vh_database_file_interrupted_clear (database_t *database, const char *file)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_UPDATE_FILE_INTERRUP_CLEAR);

  if (!file)
    return;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
vh_database_file_interrupted_fix (database_t *database)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_UPDATE_FILE_INTERRUP_FIX);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

int
vh_database_file_get_interrupted (database_t *database, const char *file)
{
  int res, err = -1, val = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_INTERRUP);

  if (!file)
    return -1;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
    val = sqlite3_column_int (stmt, 0);

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
  err = 0;
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

/******************************************************************************/
/*                         Outofpath file handling                            */
/******************************************************************************/

const char *
vh_database_file_get_outofpath_set (database_t *database, int rst)
{
  int res = SQLITE_DONE;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_OUTOFPATH_SET);

  if (!rst)
  {
    res = sqlite3_step (stmt);
    if (res == SQLITE_ROW)
      return (const char *) sqlite3_column_text (stmt, 0);
  }

  sqlite3_reset (stmt);
  if (res != SQLITE_DONE && res != SQLITE_ROW)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));

  return NULL;
}

/******************************************************************************/
/*                       Downloader Contexts handling                         */
/******************************************************************************/

static void
database_insert_dlcontext (database_t *database, file_dl_t *dl, int64_t file_id)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_INSERT_DLCONTEXT);

  VH_DB_BIND_TEXT_OR_GOTO  (stmt, 1, dl->url,  out);
  VH_DB_BIND_INT_OR_GOTO   (stmt, 2, dl->dst,  out_clear);
  VH_DB_BIND_TEXT_OR_GOTO  (stmt, 3, dl->name, out_clear);
  VH_DB_BIND_INT64_OR_GOTO (stmt, 4, file_id,  out_clear);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
 out_clear:
  sqlite3_clear_bindings (stmt);
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
vh_database_file_insert_dlcontext (database_t *database, file_data_t *data)
{
  file_dl_t *it;
  int64_t file_id;

  file_id = database_table_get_id (database,
                                   STMT_GET (STMT_SELECT_FILE_ID),
                                   data->file.path);
  if (!file_id)
    return;

  for (it = data->list_downloader; it; it = it->next)
    database_insert_dlcontext (database, it, file_id);
}

void
vh_database_file_get_dlcontext (database_t *database,
                                const char *file, file_dl_t **dl)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_SELECT_FILE_DLCONTEXT);

  if (!file || !dl)
    return;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, file, out);

  while (sqlite3_step (stmt) == SQLITE_ROW)
  {
    const char *url  = (const char *) sqlite3_column_text (stmt, 0);
    const char *name = (const char *) sqlite3_column_text (stmt, 2);
    int dst = sqlite3_column_int (stmt, 1);
    if (url && name)
      vh_file_dl_add (dl, url, name, dst);
  }

  sqlite3_reset (stmt);
  sqlite3_clear_bindings (stmt);
  err = 0;
 out:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
vh_database_delete_dlcontext (database_t *database)
{
  int res, err = -1;
  sqlite3_stmt *stmt = STMT_GET (STMT_DELETE_DLCONTEXT);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

  sqlite3_reset (stmt);
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

/******************************************************************************/
/*                            INFO table handling                             */
/******************************************************************************/

static char *
database_info_get (database_t *database, const char *name)
{
  char *ret = NULL;
  int res, err = -1;
  sqlite3_stmt *stmt;

  res = sqlite3_prepare_v2 (database->db, SELECT_INFO_VALUE, -1, &stmt, NULL);
  if (res != SQLITE_OK)
    goto out_err;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, name, out_free);

  res = sqlite3_step (stmt);
  if (res == SQLITE_ROW)
  {
    const char *val;
    val = (const char *) sqlite3_column_text (stmt, 0);
    ret = strdup (val);
  }

  sqlite3_clear_bindings (stmt);
  err = 0;

 out_free:
  sqlite3_finalize (stmt);
 out_err:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return ret;
}

static void
database_info_set (database_t *database, const char *name, const char *value)
{
  int res, err = -1;
  sqlite3_stmt *stmt;

  res = sqlite3_prepare_v2 (database->db, INSERT_INFO, -1, &stmt, NULL);
  if (res != SQLITE_OK)
    goto out_err;

  VH_DB_BIND_TEXT_OR_GOTO (stmt, 1, name,  out_free);
  VH_DB_BIND_TEXT_OR_GOTO (stmt, 2, value, out_free);

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

 out_free:
  sqlite3_finalize (stmt);
 out_err:
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

/******************************************************************************/
/*                               Main Functions                               */
/******************************************************************************/

int
vh_database_cleanup (database_t *database)
{
  int res, val, val_tmp, err = -1;

  val_tmp = sqlite3_total_changes (database->db);

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ASSOC_FILE_METADATA));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ASSOC_FILE_GRABBER));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_META));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_DATA));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_GRABBER));
  if (res == SQLITE_DONE)
    err = 0;

 out:
  val = sqlite3_total_changes (database->db);
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ASSOC_FILE_METADATA));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ASSOC_FILE_GRABBER));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_META));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_DATA));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_GRABBER));
  if (err < 0)
    vh_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val - val_tmp;
}

void
vh_database_begin_transaction (database_t *database)
{
  sqlite3_step (STMT_GET (STMT_BEGIN_TRANSACTION));
  sqlite3_reset (STMT_GET (STMT_BEGIN_TRANSACTION));
}

void
vh_database_end_transaction (database_t *database)
{
  sqlite3_step (STMT_GET (STMT_END_TRANSACTION));
  sqlite3_reset (STMT_GET (STMT_END_TRANSACTION));
}

void
vh_database_step_transaction (database_t *database,
                              unsigned int interval, int value)
{
  if (value && !(value % interval))
  {
    vh_database_end_transaction (database);
    vh_database_begin_transaction (database);
  }
}

#define DB_SQL_EXEC_OR_GOTO(d, s, m, g)       \
  database_sql_exec (d, s, NULL, &m);         \
  if (m)                                      \
    goto g;

static void
database_create_table (database_t *database)
{
  char *m = NULL;

  DB_SQL_EXEC_OR_GOTO (database->db, BEGIN_TRANSACTION,                m, err);

  /* Create tables */
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_INFO,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_FILE,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_TYPE,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_META,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_DATA,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_LANG,                m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_GROUP,               m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_GRABBER,             m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_DLCONTEXT,           m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_ASSOC_FILE_METADATA, m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_TABLE_ASSOC_FILE_GRABBER,  m, err);

  /* Create indexes */
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_CHECKED,             m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_INTERRUPTED,         m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_OUTOFPATH,           m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_ASSOC,               m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_FK_FILE,             m, err);
  DB_SQL_EXEC_OR_GOTO (database->db, CREATE_INDEX_FK_ASSOC,            m, err);

  DB_SQL_EXEC_OR_GOTO (database->db, END_TRANSACTION,                  m, err);
  return;

 err:
  vh_log (VALHALLA_MSG_ERROR, "%s", m);
  free (m);
}

#define VH_INFO_DB_VERSION  "vh_db_version"   /* LIBVALHALLA_DB_VERSION     */

static int
database_info (database_t *database)
{
  int ver = LIBVALHALLA_DB_VERSION;
  char *val;

  val = database_info_get (database, VH_INFO_DB_VERSION);
  if (val)
  {
    ver = (int) strtol (val, NULL, 10);
    free (val);
  }
  else
    database_info_set (database, VH_INFO_DB_VERSION,
                       VH_TOSTRING (LIBVALHALLA_DB_VERSION));

  vh_log (VALHALLA_MSG_INFO, "Database version : %i", ver);
  if (ver > LIBVALHALLA_DB_VERSION)
  {
    vh_log (VALHALLA_MSG_ERROR, "Please, upgrade libvalhalla to a newest "
                                "build (the version of your database is "
                                "unsupported)");
    return -1;
  }
  else if (ver < LIBVALHALLA_DB_VERSION)
  {
    char *err = NULL;

    if (ver == 1 && LIBVALHALLA_DB_VERSION == 2)
    {
      const char *const up[] = {
        DB_UPDATER_FROM_1_TO_2_A,
        DB_UPDATER_FROM_1_TO_2_B,
      };
      unsigned int i;

      vh_log (VALHALLA_MSG_WARNING,
              "Upgrade the database from the version %i to the version %i",
              ver, LIBVALHALLA_DB_VERSION);

      for (i = 0; i < ARRAY_NB_ELEMENTS (up) && !err; i++)
        database_sql_exec (database->db, up[i], NULL, &err);

      if (!err)
      {
        database_info_set (database, VH_INFO_DB_VERSION,
                           VH_TOSTRING (LIBVALHALLA_DB_VERSION));
        return 0;
      }
    }

    if (err)
    {
      vh_log (VALHALLA_MSG_ERROR, "%s", err);
      free (err);
    }

    vh_log (VALHALLA_MSG_ERROR, "Please, delete your database (%s), your "
                                "version (%i) is too old and can not be "
                                "upgraded to the version %i.",
                                database->path, ver, LIBVALHALLA_DB_VERSION);
    return -1;
  }

  return 0;
}

void
vh_database_uninit (database_t *database)
{
  unsigned int i;

  if (!database)
    return;

  if (database->path)
    free (database->path);

  if (database->stmts)
  {
    for (i = 0; i < ARRAY_NB_ELEMENTS (g_stmts); i++)
      if (STMT_GET (i))
        sqlite3_finalize (STMT_GET (i));
    free (database->stmts);
  }

  if (database->file_type)
    free (database->file_type);
  if (database->groups_id)
    free (database->groups_id);
  if (database->langs_id)
    free (database->langs_id);

  if (database->db)
    sqlite3_close (database->db);

  free (database);
}

database_t *
vh_database_init (const char *path)
{
  int res, exists;
  unsigned int i;
  database_t *database;

  if (!path)
    return NULL;

  if (sqlite3_threadsafe () != 1)
  {
    res = sqlite3_config (SQLITE_CONFIG_SERIALIZED);
    if (res != SQLITE_OK)
    {
      vh_log (VALHALLA_MSG_ERROR,
              "SQLite3 is not compiled with serialized threading mode!");
      return NULL;
    }
  }

  database = calloc (1, sizeof (database_t));
  if (!database)
    return NULL;

  res = sqlite3_initialize ();
  if (res != SQLITE_OK)
    return NULL;

  exists = vh_file_exists (path);

  res = sqlite3_open (path, &database->db);
  if (res)
  {
    vh_log (VALHALLA_MSG_ERROR,
            "Can't open database: %s", sqlite3_errmsg (database->db));
    goto err;
  }

  database->path = strdup (path);

  if (exists && database_info (database))
    goto err;

  database_create_table (database);

  res = database_prepare_stmt (database);
  if (res)
    goto err;

  if (!exists && database_info (database))
    goto err;

  database->file_type = malloc (sizeof (g_file_type));
  database->groups_id = calloc (vh_metadata_group_size, sizeof (int64_t));
  database->langs_id  = calloc (vh_metadata_lang_size,  sizeof (int64_t));

  if (!database->file_type || !database->groups_id || !database->langs_id)
    goto err;

  memcpy (database->file_type, g_file_type, sizeof (g_file_type));

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_file_type); i++)
    database->file_type[i].id =
      database_type_insert (database, database->file_type[i].name);

  for (i = 0; i < vh_metadata_group_size; i++)
  {
    const char *group = vh_metadata_group_str (i);
    database->groups_id[i] = database_group_insert (database, group);
  }

  for (i = 0; i < vh_metadata_lang_size; i++)
  {
    const char *lshort, *llong;
    vh_metadata_lang_str (i, &lshort, &llong);
    database->langs_id[i] = database_lang_insert (database, lshort, llong);
  }

  return database;

 err:
  vh_database_uninit (database);
  return NULL;
}

/******************************************************************************/
/*                                                                            */
/*                           For Public Selections                            */
/*                                                                            */
/******************************************************************************/

#define SQL_CONCAT(sql, str, args...)                   \
  do                                                    \
  {                                                     \
    char buf[256];                                      \
    sqlite3_snprintf (sizeof (buf), buf, str, ##args);  \
    if (strlen (sql) + strlen (buf) < SQL_BUFFER)       \
      strcat (sql, buf);                                \
  }                                                     \
  while (0)

#define SQL_CONCAT_TYPE(sql, item, def)                              \
  do                                                                 \
  {                                                                  \
    switch ((item).type)                                             \
    {                                                                \
    case VALHALLA_DB_TYPE_ID:                                        \
      SQL_CONCAT (sql, SELECT_LIST_WHERE_##def##_ID, (item).id);     \
      break;                                                         \
                                                                     \
    case VALHALLA_DB_TYPE_TEXT:                                      \
      SQL_CONCAT (sql, SELECT_LIST_WHERE_##def##_NAME, (item).text); \
      break;                                                         \
                                                                     \
    default:                                                         \
      break;                                                         \
    }                                                                \
  }                                                                  \
  while (0)

#define VH_DB_RETURN_SQL_PREPARE(d, s, v)                         \
  {                                                               \
    int rc;                                                       \
    database_query_plan (d, s);                                   \
    v->sql = strdup (s);                                          \
    rc = sqlite3_prepare_v2 (d->db, v->sql, -1, &v->stmt, NULL);  \
    if (rc != SQLITE_OK)                                          \
    {                                                             \
      vh_log (VALHALLA_MSG_ERROR,                                 \
              "%s - query: %s", sqlite3_errmsg (d->db), s);       \
      database_vhstmt_free (v);                                   \
      return NULL;                                                \
    }                                                             \
    return v;                                                     \
  }

static inline int
database_sql_vhstmt (sqlite3 *db, valhalla_db_stmt_t *vhstmt)
{
  int rc;
  char *msg = NULL;
  valhalla_verb_t verb = VALHALLA_MSG_VERBOSE;

  vh_log (verb, "query: %s", vhstmt->sql);

  rc = database_sql_exec (db, vhstmt->sql, vhstmt, &msg);
  if (msg)
  {
    vh_log (VALHALLA_MSG_ERROR, "%s", msg);
    free (msg);
    verb = VALHALLA_MSG_ERROR;
  }

  return rc;
}

static inline void
database_list_get_restriction_common (database_t *database,
                                      valhalla_db_restrict_t *restriction,
                                      char *sql)
{
  SQL_CONCAT_TYPE (sql, restriction->meta, META);
  if (restriction->data.text || restriction->data.id)
  {
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT_TYPE (sql, restriction->data, DATA);
  }

  if (restriction->data.lang >= 0)
  {
    int64_t lang_id;

    lang_id = database_langid_get (database, restriction->data.lang);
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT (sql, SELECT_LIST_WHERE_LANG_ID, lang_id);
  }

  SQL_CONCAT (sql, SELECT_LIST_AND);
  SQL_CONCAT (sql, SELECT_LIST_WHERE_PRIORITY, restriction->meta.priority);
}

static void
database_list_get_restriction_sub (database_t *database,
                                   valhalla_db_restrict_t *restriction,
                                   char *sql)
{
  /* sub-query */
  switch (restriction->op)
  {
  case VALHALLA_DB_OPERATOR_IN:
    SQL_CONCAT (sql, SELECT_LIST_WHERE_SUB_IN);
    break;

  case VALHALLA_DB_OPERATOR_NOTIN:
    SQL_CONCAT (sql, SELECT_LIST_WHERE_SUB_NOTIN);
    break;

  default:
    return; /* impossible or bug */
  }

  SQL_CONCAT (sql, SELECT_LIST_WHERE_SUB);

  /* sub-where */
  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  database_list_get_restriction_common (database, restriction, sql);

  /* sub-end */
  SQL_CONCAT (sql, SELECT_LIST_WHERE_SUB_END);
}

static void
database_list_get_restriction_equal (database_t *database,
                                     valhalla_db_restrict_t *restriction,
                                     char *sql, int equal)
{
  if (equal)
    SQL_CONCAT (sql, SELECT_LIST_OR);

  SQL_CONCAT (sql, "( ");

  database_list_get_restriction_common (database, restriction, sql);

  SQL_CONCAT (sql, ") ");
}

static void
database_list_get_restriction (database_t *database,
                               valhalla_db_restrict_t *restriction, char *sql)
{
  int equal = 0, restr = 0;
  char sql_tmp[SQL_BUFFER] = "( ";

  for (; restriction; restriction = restriction->next)
  {
    if (!restriction->meta.id && !restriction->meta.text)
      continue; /* a restriction without meta is wrong */

    switch (restriction->op)
    {
    case VALHALLA_DB_OPERATOR_IN:
    case VALHALLA_DB_OPERATOR_NOTIN:
      database_list_get_restriction_sub (database, restriction, sql);
      restr = 1;
      break;

    case VALHALLA_DB_OPERATOR_EQUAL:
      database_list_get_restriction_equal (database,
                                           restriction, sql_tmp, equal);
      equal = 1;
      break;

    default:
      continue;
    }

    if (restriction->next
        && (restriction->next->op == VALHALLA_DB_OPERATOR_IN ||
            restriction->next->op == VALHALLA_DB_OPERATOR_NOTIN))
      SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  if (equal)
  {
    if (restr)
      SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT (sql, "%s", sql_tmp);
    SQL_CONCAT (sql, ") ");
  }
}

const valhalla_db_metares_t *
vh_database_metalist_read (database_t *database, valhalla_db_stmt_t *vhstmt)
{
  int rc;
  valhalla_db_metares_t *metares = &vhstmt->u.metares;

  rc = database_sql_vhstmt (database->db, vhstmt);
  if (rc) /* no more row */
    return NULL;

  if (vhstmt->cnt != 7)
    goto err;

  metares->meta_id    = (int64_t) strtoimax (vhstmt->cols[0], NULL, 10);
  metares->meta_name  = vhstmt->cols[2];
  metares->data_id    = (int64_t) strtoimax (vhstmt->cols[1], NULL, 10);
  metares->data_value = vhstmt->cols[3];
  metares->external   = (int)     strtol    (vhstmt->cols[6], NULL, 10);
  metares->group      =
    database_group_get (database,
                        (int64_t) strtoimax (vhstmt->cols[5], NULL, 10));
  metares->lang       =
    database_lang_get  (database,
                        (int64_t) strtoimax (vhstmt->cols[4], NULL, 10));

  return metares;

 err:
  sqlite3_finalize (vhstmt->stmt);
  database_vhstmt_free (vhstmt);
  return NULL;
}

valhalla_db_stmt_t *
vh_database_metalist_get (database_t *database,
                          valhalla_db_item_t *search,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction)
{
  valhalla_db_stmt_t *vhstmt;
  /*
   * SELECT meta.meta_id, data.data_id,
   *        meta.meta_name, data.data_value,
   *        data._lang_id,
   *        assoc._grp_id, assoc.external
   * FROM (
   *   data INNER JOIN assoc_file_metadata AS assoc
   *   ON data.data_id = assoc.data_id
   * ) INNER JOIN meta
   * ON assoc.meta_id = meta.meta_id
   */
  char sql[SQL_BUFFER] = SELECT_LIST_METADATA_FROM;

  vhstmt = calloc (1, sizeof (valhalla_db_stmt_t));
  if (!vhstmt)
    return NULL;

  /* WHERE */
  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  /*
   * assoc.file_id IN (
   *   SELECT file_id
   *   FROM file
   *   WHERE _type_id = <ID>
   * )
   * <AND>
   */
  if (filetype)
  {
    SQL_CONCAT (sql, SELECT_LIST_METADATA_WHERE_TYPE_ID,
                database_file_typeid_get (database, filetype));
    /* AND */
    if (restriction || search->id || search->text)
      SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  /*
   * -- A sub query is created by every restriction.
   * -- Every restriction is separated by AND.
   * assoc.file_id <IN|NOT IN> (
   *   SELECT assoc.file_id
   *   FROM (
   *     data INNER JOIN assoc_file_metadata AS assoc
   *     ON data.data_id = assoc.data_id
   *   ) INNER JOIN meta
   *   ON assoc.meta_id = meta.meta_id
   *   WHERE meta.<meta_id|meta_name>  = <ID|"TEXT">
   *     AND data.<data_id|data_value> = <ID|"TEXT">
   *     AND data._lang_id = <ID>
   *     AND assoc.priority__ <= <PRIORITY>
   * )
   * <AND>
   */
  if (restriction)
  {
    database_list_get_restriction (database, restriction, sql);
    /* AND */
    if (search->id || search->text)
      SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  /*
   * -- Metadata and/or group to list in the results.
   * meta.<meta_id|meta_name> = <ID|"TEXT">
   */
  SQL_CONCAT_TYPE (sql, *search, META);
  if (search->group)
  {
    /* AND */
    if (search->id || search->text)
      SQL_CONCAT (sql, SELECT_LIST_AND);
    /* assoc._grp_id = <ID> */
    SQL_CONCAT (sql, SELECT_LIST_WHERE_GROUP_ID,
                database_groupid_get (database, search->group));
  }
  if (search->lang >= 0)
  {
    /* AND */
    if (search->group || search->id || search->text || restriction)
      SQL_CONCAT (sql, SELECT_LIST_AND);
    /* data._lang_id = <ID> */
    SQL_CONCAT (sql, SELECT_LIST_WHERE_LANG_ID,
                database_langid_get (database, search->lang));
  }
  /* AND */
  if (search->group || search->id || search->text || restriction
      || search->lang >= 0)
    SQL_CONCAT (sql, SELECT_LIST_AND);
  /* assoc.priority__ <= <PRIORITY> */
  SQL_CONCAT (sql, SELECT_LIST_WHERE_PRIORITY, search->priority);

  /*
   * GROUP BY assoc.meta_id, assoc.data_id
   * ORDER BY data.data_value;
   */
  SQL_CONCAT (sql, SELECT_LIST_METADATA_END);

  VH_DB_RETURN_SQL_PREPARE (database, sql, vhstmt)
}

const valhalla_db_fileres_t *
vh_database_filelist_read (database_t *database, valhalla_db_stmt_t *vhstmt)
{
  int rc;
  valhalla_db_fileres_t *fileres = &vhstmt->u.fileres;

  rc = database_sql_vhstmt (database->db, vhstmt);
  if (rc) /* no more row */
    return NULL;

  if (vhstmt->cnt != 3)
    goto err;

  fileres->id   = (int64_t) strtoimax (vhstmt->cols[0], NULL, 10);
  fileres->path = vhstmt->cols[1];
  fileres->type =
    database_file_type_get (database,
                            (int64_t) strtoimax (vhstmt->cols[2], NULL, 10));

  return fileres;

 err:
  sqlite3_finalize (vhstmt->stmt);
  database_vhstmt_free (vhstmt);
  return NULL;
}

valhalla_db_stmt_t *
vh_database_filelist_get (database_t *database,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction)
{
  valhalla_db_stmt_t *vhstmt;
  /*
   * SELECT file_id, file_path, _type_id
   * FROM file AS assoc
   */
  char sql[SQL_BUFFER] = SELECT_LIST_FILE_FROM;

  vhstmt = calloc (1, sizeof (valhalla_db_stmt_t));
  if (!vhstmt)
    return NULL;

  /* WHERE */
  if (restriction || filetype)
    SQL_CONCAT (sql, SELECT_LIST_WHERE);

  /*
   * -- A sub query is created by every restriction.
   * -- Every restriction is separated by AND.
   * assoc.file_id <IN|NOT IN> (
   *   SELECT assoc.file_id
   *   FROM (
   *     data INNER JOIN assoc_file_metadata AS assoc
   *     ON data.data_id = assoc.data_id
   *   ) INNER JOIN meta
   *   ON assoc.meta_id = meta.meta_id
   *   WHERE meta.<meta_id|meta_name>  = <ID|"TEXT">
   *     AND data.<data_id|data_value> = <ID|"TEXT">
   *     AND data._lang_id = <ID>
   *     AND assoc.priority__ <= <PRIORITY>
   * )
   * <AND>
   */
  if (restriction)
    database_list_get_restriction (database, restriction, sql);

  if (filetype)
  {
    /* AND */
    if (restriction)
      SQL_CONCAT (sql, SELECT_LIST_AND);
    /* _type_id = <ID> */
    SQL_CONCAT (sql, SELECT_LIST_WHERE_TYPE_ID,
                database_file_typeid_get (database, filetype));
  }

  /* ORDER BY file_id; */
  SQL_CONCAT (sql, SELECT_LIST_FILE_END);

  VH_DB_RETURN_SQL_PREPARE (database, sql, vhstmt)
}

const valhalla_db_metares_t *
vh_database_file_read (database_t *database, valhalla_db_stmt_t *vhstmt)
{
  int rc;
  valhalla_db_metares_t *metares = &vhstmt->u.metares;

  rc = database_sql_vhstmt (database->db, vhstmt);
  if (rc) /* no more row */
    return NULL;

  if (vhstmt->cnt != 8)
    goto err;

  metares->meta_id    = (int64_t) strtoimax (vhstmt->cols[2], NULL, 10);
  metares->meta_name  = vhstmt->cols[4];
  metares->data_id    = (int64_t) strtoimax (vhstmt->cols[3], NULL, 10);
  metares->data_value = vhstmt->cols[5];
  metares->external   = (int)     strtol    (vhstmt->cols[7], NULL, 10);
  metares->group      =
    database_group_get (database,
                        (int64_t) strtoimax (vhstmt->cols[1], NULL, 10));
  metares->lang       =
    database_lang_get  (database,
                        (int64_t) strtoimax (vhstmt->cols[6], NULL, 10));

  return metares;

 err:
  sqlite3_finalize (vhstmt->stmt);
  database_vhstmt_free (vhstmt);
  return NULL;
}

valhalla_db_stmt_t *
vh_database_file_get (database_t *database,
                      int64_t id, const char *path,
                      valhalla_db_restrict_t *restriction)
{
  valhalla_db_stmt_t *vhstmt;
  /*
   * SELECT file.file_id, assoc._grp_id,
   *        meta.meta_id, data.data_id,
   *        meta.meta_name, data.data_value,
   *        data._lang_id, assoc.external
   * FROM ((
   *     file INNER JOIN assoc_file_metadata AS assoc
   *     ON file.file_id = assoc.file_id
   *   ) INNER JOIN data
   *   ON data.data_id = assoc.data_id
   * ) INNER JOIN meta
   * ON assoc.meta_id = meta.meta_id
   */
  char sql[SQL_BUFFER] = SELECT_FILE_FROM;

  vhstmt = calloc (1, sizeof (valhalla_db_stmt_t));
  if (!vhstmt)
    return NULL;

  /* WHERE */
  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  /*
   * -- The restrictions must use only the type EQUAL.
   * -- Every restriction is separated by OR.
   * (
   *   (
   *     meta.<meta_id|meta_name> = <ID|"TEXT">
   *       AND data.<data_id|data_value> = <ID|"TEXT">
   *       AND data._lang_id = <ID>
   *       AND assoc.priority__ <= <PRIORITY>
   *   )
   *   <OR>
   * )
   */
  if (restriction)
  {
    database_list_get_restriction (database, restriction, sql);
    /* AND */
    SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  /* file.<file_id|file_path> = <ID|"PATH"> */
  if (id)
    SQL_CONCAT (sql, SELECT_FILE_WHERE_FILE_ID, id);
  else if (path)
    SQL_CONCAT (sql, SELECT_FILE_WHERE_FILE_PATH, path);
  else
  {
    database_vhstmt_free (vhstmt);
    return NULL;
  }

  /* ORDER BY assoc.priority__; */
  SQL_CONCAT (sql, SELECT_FILE_END);

  VH_DB_RETURN_SQL_PREPARE (database, sql, vhstmt)
}

/******************************************************************************/
/*                                                                            */
/*                  For Public Insertions/Updates/Deletions                   */
/*                                                                            */
/******************************************************************************/

int
vh_database_metadata_insert (database_t *database, const char *path,
                             const char *meta, const char *data,
                             valhalla_lang_t lang, valhalla_meta_grp_t group)
{
  int res = -1;
  int64_t file_id, meta_id, data_id, group_id;

  if (!path || !meta || !data)
    return -1;

  file_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_FILE_ID), path);
  if (!file_id)
    return -1; /* file unknown */

  meta_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_META_ID), meta);
  data_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), data);
  if (meta_id && data_id)
  {
    int ext;
    int64_t ngroup_id;
    res = database_assoc_filemd_get (database, file_id, meta_id, data_id,
                                     &ngroup_id, &ext);
    if (!res)
    {
      group_id = database_groupid_get (database, group);
      if (ngroup_id != group_id)
        return -2; /* go out because an entry exists with an other group ID */

      if (ext == 1)
        return 0; /* already inserted */
    }
  }

  if (!res)
    database_assoc_filemd_update (database,
                                  file_id, meta_id, data_id,
                                  group_id, 1, VALHALLA_METADATA_PL_HIGHEST);
  else
  {
    if (!meta_id)
      meta_id = database_meta_insert (database, meta);
    if (!data_id)
    {
      int64_t lang_id = database_langid_get (database, lang);
      data_id = database_data_insert (database, data, lang_id);
    }
    group_id = database_groupid_get (database, group);
    database_assoc_filemd_insert (database,
                                  file_id, meta_id, data_id,
                                  group_id, 1, VALHALLA_METADATA_PL_HIGHEST);
  }

  return 0;
}

int
vh_database_metadata_update (database_t *database, const char *path,
                             const char *meta, const char *data,
                             const char *ndata, valhalla_lang_t lang)
{
  int res, ext;
  int64_t file_id, meta_id, data_id, group_id, lang_id;

  if (!path || !meta || !data || !ndata)
    return -1;

  file_id = database_file_id_by_metadata (database, path, meta, data);
  if (!file_id)
    return -1; /* association unknown */

  meta_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_META_ID), meta);
  data_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), data);
  if (!meta_id || !data_id)
    return -2; /* at least one association is not cleanup'ed */

  res = database_assoc_filemd_get (database, file_id, meta_id, data_id,
                                   &group_id, &ext);
  if (res)
    return -3;

  database_assoc_filemd_delete (database, file_id, meta_id, data_id);
  lang_id = database_langid_get (database, lang);
  data_id = database_data_insert (database, ndata, lang_id);
  database_assoc_filemd_insert (database,
                                file_id, meta_id, data_id,
                                group_id, 1, VALHALLA_METADATA_PL_HIGHEST);
  return 0;
}

int
vh_database_metadata_delete (database_t *database, const char *path,
                             const char *meta, const char *data)
{
  int res, ext = 0;
  int64_t file_id, meta_id, data_id, group_id;

  if (!path || !meta || !data)
    return -1;

  file_id = database_file_id_by_metadata (database, path, meta, data);
  if (!file_id)
    return 0; /* association unknown */

  meta_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_META_ID), meta);
  data_id =
    database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), data);
  if (!meta_id || !data_id)
    return -1; /* at least one association is not cleanup'ed */

  res = database_assoc_filemd_get (database, file_id, meta_id, data_id,
                                   &group_id, &ext);
  if (res || !ext)
    return -2;

  database_assoc_filemd_delete (database, file_id, meta_id, data_id);
  return 0;
}

int
vh_database_metadata_priority (database_t *database, const char *path,
                               const char *meta, const char *data,
                               valhalla_metadata_pl_t p)
{
  int64_t file_id, meta_id = 0, data_id = 0;

  if (!path)
    return -1;

  if (!meta && data)
    return -1;

  if (!METADATA_PL_CHECK (p))
    return -1;

  if (meta)
    file_id = database_file_id_by_metadata (database, path, meta, data);
  else
    file_id =
      database_table_get_id (database, STMT_GET (STMT_SELECT_FILE_ID), path);

  if (!file_id)
    return 0; /* association unknown */

  if (meta)
  {
    meta_id =
      database_table_get_id (database, STMT_GET (STMT_SELECT_META_ID), meta);
    if (!meta_id)
      return -2;
  }

  if (data)
  {
    data_id =
      database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), data);
    if (!data_id)
      return -2;
  }

  database_assoc_filemd_update_p (database, file_id, meta_id, data_id, p);
  return 0;
}
