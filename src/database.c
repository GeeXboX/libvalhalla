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
#include "metadata.h"
#include "sql_statements.h"
#include "database.h"
#include "logs.h"


#define SQL_BUFFER 4096

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
  item_list_t  *meta_group;
};

typedef struct database_cb_s {
  int       (*cb_mr) (void *data, valhalla_db_metares_t *res);
  int       (*cb_fr) (void *data, valhalla_db_fileres_t *res);
  void       *data;
  database_t *database;
} database_cb_t;

static const item_list_t g_file_type[] = {
  [VALHALLA_FILE_TYPE_NULL]     = { 0, "null"     },
  [VALHALLA_FILE_TYPE_AUDIO]    = { 0, "audio"    },
  [VALHALLA_FILE_TYPE_IMAGE]    = { 0, "image"    },
  [VALHALLA_FILE_TYPE_PLAYLIST] = { 0, "playlist" },
  [VALHALLA_FILE_TYPE_VIDEO]    = { 0, "video"    },
};

static const item_list_t g_meta_group[] = {
  [VALHALLA_META_GRP_NIL]             = { 0, "null"           },
  [VALHALLA_META_GRP_MISCELLANEOUS]   = { 0, "miscellaneous"  },
  [VALHALLA_META_GRP_CLASSIFICATION]  = { 0, "classification" },
  [VALHALLA_META_GRP_COMMERCIAL]      = { 0, "commercial"     },
  [VALHALLA_META_GRP_CONTACT]         = { 0, "contact"        },
  [VALHALLA_META_GRP_ENTITIES]        = { 0, "entities"       },
  [VALHALLA_META_GRP_IDENTIFIER]      = { 0, "identifier"     },
  [VALHALLA_META_GRP_LEGAL]           = { 0, "legal"          },
  [VALHALLA_META_GRP_MUSICAL]         = { 0, "musical"        },
  [VALHALLA_META_GRP_ORGANIZATIONAL]  = { 0, "organizational" },
  [VALHALLA_META_GRP_PERSONAL]        = { 0, "personal"       },
  [VALHALLA_META_GRP_SPACIAL]         = { 0, "spacial"        },
  [VALHALLA_META_GRP_TECHNICAL]       = { 0, "technical"      },
  [VALHALLA_META_GRP_TEMPORAL]        = { 0, "temporal"       },
  [VALHALLA_META_GRP_TITLES]          = { 0, "titles"         },
};

typedef enum database_stmt {
  STMT_SELECT_FILE_MTIME,
  STMT_SELECT_TYPE_ID,
  STMT_SELECT_META_ID,
  STMT_SELECT_DATA_ID,
  STMT_SELECT_GROUP_ID,
  STMT_INSERT_FILE,
  STMT_INSERT_TYPE,
  STMT_INSERT_META,
  STMT_INSERT_DATA,
  STMT_INSERT_GROUP,
  STMT_INSERT_ASSOC_FILE_METADATA,
  STMT_UPDATE_FILE,
  STMT_DELETE_FILE,

  STMT_CLEANUP_ASSOC_FILE_METADATA,
  STMT_CLEANUP_META,
  STMT_CLEANUP_DATA,

  STMT_UPDATE_FILE_CHECKED_CLEAR,
  STMT_SELECT_FILE_CHECKED_CLEAR,
  STMT_BEGIN_TRANSACTION,
  STMT_END_TRANSACTION,
} database_stmt_t;

static const stmt_list_t g_stmts[] = {
  [STMT_SELECT_FILE_MTIME]           = { SELECT_FILE_MTIME,           NULL },
  [STMT_SELECT_TYPE_ID]              = { SELECT_TYPE_ID,              NULL },
  [STMT_SELECT_META_ID]              = { SELECT_META_ID,              NULL },
  [STMT_SELECT_DATA_ID]              = { SELECT_DATA_ID,              NULL },
  [STMT_SELECT_GROUP_ID]             = { SELECT_GROUP_ID,             NULL },
  [STMT_INSERT_FILE]                 = { INSERT_FILE,                 NULL },
  [STMT_INSERT_TYPE]                 = { INSERT_TYPE,                 NULL },
  [STMT_INSERT_META]                 = { INSERT_META,                 NULL },
  [STMT_INSERT_DATA]                 = { INSERT_DATA,                 NULL },
  [STMT_INSERT_GROUP]                = { INSERT_GROUP,                NULL },
  [STMT_INSERT_ASSOC_FILE_METADATA]  = { INSERT_ASSOC_FILE_METADATA,  NULL },
  [STMT_UPDATE_FILE]                 = { UPDATE_FILE,                 NULL },
  [STMT_DELETE_FILE]                 = { DELETE_FILE,                 NULL },

  [STMT_CLEANUP_ASSOC_FILE_METADATA] = { CLEANUP_ASSOC_FILE_METADATA, NULL },
  [STMT_CLEANUP_META]                = { CLEANUP_META,                NULL },
  [STMT_CLEANUP_DATA]                = { CLEANUP_DATA,                NULL },

  [STMT_UPDATE_FILE_CHECKED_CLEAR]   = { UPDATE_FILE_CHECKED_CLEAR,   NULL },
  [STMT_SELECT_FILE_CHECKED_CLEAR]   = { SELECT_FILE_CHECKED_CLEAR,   NULL },
  [STMT_BEGIN_TRANSACTION]           = { BEGIN_TRANSACTION,           NULL },
  [STMT_END_TRANSACTION]             = { END_TRANSACTION,             NULL },
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

static valhalla_meta_grp_t
database_group_get (database_t *database, int64_t id)
{
  int i;

  if (!database)
    return VALHALLA_META_GRP_NIL;

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_meta_group); i++)
    if (database->meta_group[i].id == id)
      return i;

  return VALHALLA_META_GRP_NIL;
}

static valhalla_file_type_t
database_file_type_get (database_t *database, int64_t id)
{
  int i;

  if (!database)
    return VALHALLA_FILE_TYPE_NULL;

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_file_type); i++)
    if (database->file_type[i].id == id)
      return i;

  return VALHALLA_FILE_TYPE_NULL;
}

static void
database_create_table (database_t *database)
{
  char *msg = NULL;

  /* Create tables */
  sqlite3_exec (database->db, CREATE_TABLE_FILE
                              CREATE_TABLE_TYPE
                              CREATE_TABLE_META
                              CREATE_TABLE_DATA
                              CREATE_TABLE_GROUP
                              CREATE_TABLE_ASSOC_FILE_METADATA,
                NULL, NULL, &msg);
  if (msg)
    goto err;

  /* Create indexes */
  sqlite3_exec (database->db, CREATE_INDEX_FILE_PATH
                              CREATE_INDEX_TYPE_NAME
                              CREATE_INDEX_META_NAME
                              CREATE_INDEX_DATA_VALUE
                              CREATE_INDEX_GROUP_NAME
                              CREATE_INDEX_ASSOC,
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
  sqlite3_reset (stmt);
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
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
  sqlite3_reset (stmt);
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
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
database_data_insert (database_t *database, const char *value)
{
  int64_t val;
  val = database_insert_name (database, STMT_GET (STMT_INSERT_DATA), value);

  /* retrieve ID if aborted */
  if (!val)
    return
      database_table_get_id (database, STMT_GET (STMT_SELECT_DATA_ID), value);

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

static void
database_assoc_insert (sqlite3_stmt *stmt,
                       int64_t file_id, int64_t meta_id,
                       int64_t data_id, int64_t group_id)
{
  int res, err = -1;

  res = sqlite3_bind_int64 (stmt, 1, file_id);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int64 (stmt, 2, meta_id);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int64 (stmt, 3, data_id);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int64 (stmt, 4, group_id);
  if (res != SQLITE_OK)
    goto out_clear;

  res = sqlite3_step (stmt);
  if (res == SQLITE_DONE)
    err = 0;

 out_clear:
  sqlite3_clear_bindings (stmt);
 out_reset:
  sqlite3_reset (stmt);
  if (err < 0 && res != SQLITE_CONSTRAINT) /* ignore constraint violation */
    valhalla_log (VALHALLA_MSG_ERROR,
                  "%s", sqlite3_errmsg (sqlite3_db_handle (stmt)));
}

static int64_t
database_file_insert (database_t *database, file_data_t *data, int64_t type_id)
{
  int res, err = -1;
  int64_t val = 0, val_tmp;

  res = sqlite3_bind_text (STMT_GET (STMT_INSERT_FILE), 1,
                           data->file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_reset;

  res = sqlite3_bind_int (STMT_GET (STMT_INSERT_FILE), 2, data->mtime);
  if (res != SQLITE_OK)
    goto out_clear;

  if (type_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_INSERT_FILE), 3, type_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  val_tmp = sqlite3_last_insert_rowid (database->db);
  res = sqlite3_step (STMT_GET (STMT_INSERT_FILE));
  if (res == SQLITE_DONE)
    err = 0;
  val = sqlite3_last_insert_rowid (database->db);

  if (val == val_tmp)
    val = 0;

 out_clear:
  sqlite3_clear_bindings (STMT_GET (STMT_INSERT_FILE));
 out_reset:
  sqlite3_reset (STMT_GET (STMT_INSERT_FILE));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static int64_t
database_file_update (database_t *database, file_data_t *data, int64_t type_id)
{
  int res, err = -1;
  int64_t val = 0, val_tmp;

  res = sqlite3_bind_int (STMT_GET (STMT_UPDATE_FILE), 1, data->mtime);
  if (res != SQLITE_OK)
    goto out_reset;

  if (type_id)
  {
    res = sqlite3_bind_int64 (STMT_GET (STMT_UPDATE_FILE), 2, type_id);
    if (res != SQLITE_OK)
      goto out_clear;
  }

  res = sqlite3_bind_text (STMT_GET (STMT_UPDATE_FILE), 3,
                           data->file, -1, SQLITE_STATIC);
  if (res != SQLITE_OK)
    goto out_clear;

  val_tmp = sqlite3_last_insert_rowid (database->db);
  res = sqlite3_step (STMT_GET (STMT_UPDATE_FILE));
  if (res == SQLITE_DONE)
    err = 0;
  val = sqlite3_last_insert_rowid (database->db);

  if (val == val_tmp)
    val = 0;

 out_clear:
  sqlite3_clear_bindings (STMT_GET (STMT_UPDATE_FILE));
 out_reset:
  sqlite3_reset (STMT_GET (STMT_UPDATE_FILE));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

static void
database_file_data (database_t *database, file_data_t *data, int insert)
{
  int64_t file_id, type_id, meta_id = 0, data_id = 0, group_id = 0;
  type_id = database->file_type[data->type].id;
  file_id = insert
            ? database_file_insert (database, data, type_id)
            : database_file_update (database, data, type_id);

  if (!file_id)
    return;

  if (data->meta)
  {
    int id = 0;
    const char *name, *value;
    valhalla_meta_grp_t group;

    while (!metadata_get (data->meta, id++, &name, &value, &group))
    {
      meta_id  = database_meta_insert (database, name);
      data_id  = database_data_insert (database, value);
      group_id = database->meta_group[group].id;

      database_assoc_insert (STMT_GET (STMT_INSERT_ASSOC_FILE_METADATA),
                             file_id, meta_id, data_id, group_id);
    }
  }
}

void
database_file_data_insert (database_t *database, file_data_t *data)
{
  database_file_data (database, data, 1);
}

void
database_file_data_update (database_t *database, file_data_t *data)
{
  database_file_data (database, data, 0);
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
  sqlite3_reset (STMT_GET (STMT_DELETE_FILE));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

void
database_file_checked_clear (database_t *database)
{
  int res;

  res = sqlite3_step (STMT_GET (STMT_UPDATE_FILE_CHECKED_CLEAR));

  sqlite3_reset (STMT_GET (STMT_UPDATE_FILE_CHECKED_CLEAR));
  if (res != SQLITE_DONE)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
}

const char *
database_file_get_checked_clear (database_t *database)
{
  int res;

  res = sqlite3_step (STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR));
  if (res == SQLITE_ROW)
    return (const char *) sqlite3_column_text (
                            STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR), 0);

  sqlite3_reset (STMT_GET (STMT_SELECT_FILE_CHECKED_CLEAR));
  if (res != SQLITE_DONE)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));

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
  sqlite3_reset (STMT_GET (STMT_SELECT_FILE_MTIME));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
  return val;
}

int
database_cleanup (database_t *database)
{
  int res, val, val_tmp, err = -1;

  val_tmp = sqlite3_total_changes (database->db);

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_ASSOC_FILE_METADATA));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_META));
  if (res != SQLITE_DONE)
    goto out;

  res = sqlite3_step (STMT_GET (STMT_CLEANUP_DATA));
  if (res == SQLITE_DONE)
    err = 0;

 out:
  val = sqlite3_total_changes (database->db);
  sqlite3_reset (STMT_GET (STMT_CLEANUP_ASSOC_FILE_METADATA));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_META));
  sqlite3_reset (STMT_GET (STMT_CLEANUP_DATA));
  if (err < 0)
    valhalla_log (VALHALLA_MSG_ERROR, "%s", sqlite3_errmsg (database->db));
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

  if (database->file_type)
    free (database->file_type);
  if (database->meta_group)
    free (database->meta_group);

  if (database->db)
    sqlite3_close (database->db);

  free (database);
}

database_t *
database_init (const char *path)
{
  int i, res;
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
    goto err;

  database->file_type = malloc (sizeof (g_file_type));
  database->meta_group = malloc (sizeof (g_meta_group));
  if (!database->file_type || !database->meta_group)
    goto err;

  memcpy (database->file_type, g_file_type, sizeof (g_file_type));
  memcpy (database->meta_group, g_meta_group, sizeof (g_meta_group));

  for (i = 0; i < ARRAY_NB_ELEMENTS (g_file_type); i++)
    database->file_type[i].id =
      database_type_insert (database, database->file_type[i].name);
  for (i = 0; i < ARRAY_NB_ELEMENTS (g_meta_group); i++)
    database->meta_group[i].id =
      database_group_insert (database, database->meta_group[i].name);

  return database;

 err:
  database_uninit (database);
  return NULL;
}

/******************************************************************************/
/*                                                                            */
/*                           For Public Selections                            */
/*                                                                            */
/******************************************************************************/

#define SQL_CONCAT(sql, str, args...)             \
  do                                              \
  {                                               \
    char buf[256];                                \
    snprintf (buf, sizeof (buf), str, ##args);    \
    if (strlen (sql) + strlen (buf) < SQL_BUFFER) \
      strcat (sql, buf);                          \
  }                                               \
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

#define DATABASE_RETURN_SQL_EXEC(db, sql, cb, data, msg) \
  sqlite3_exec (db, sql, cb, &data, &msg);               \
  if (msg)                                               \
  {                                                      \
    valhalla_log (VALHALLA_MSG_ERROR, "%s", msg);        \
    sqlite3_free (msg);                                  \
    return -1;                                           \
  }                                                      \
  return 0;


static void
database_list_get_restriction_sub (valhalla_db_restrict_t *restriction,
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
  SQL_CONCAT_TYPE (sql, restriction->meta, META);
  if (restriction->data.text || restriction->data.id)
  {
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT_TYPE (sql, restriction->data, DATA);
  }

  /* sub-end */
  SQL_CONCAT (sql, SELECT_LIST_WHERE_SUB_END);
}

static void
database_list_get_restriction_equal (valhalla_db_restrict_t *restriction,
                                     char *sql, int equal)
{
  if (equal)
    SQL_CONCAT (sql, SELECT_LIST_OR);

  SQL_CONCAT (sql, "( ");

  SQL_CONCAT_TYPE (sql, restriction->meta, META);
  if (restriction->data.text || restriction->data.id)
  {
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT_TYPE (sql, restriction->data, DATA);
  }

  SQL_CONCAT (sql, ") ");
}

static void
database_list_get_restriction (valhalla_db_restrict_t *restriction, char *sql)
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
      database_list_get_restriction_sub (restriction, sql);
      restr = 1;
      break;

    case VALHALLA_DB_OPERATOR_EQUAL:
      database_list_get_restriction_equal (restriction, sql_tmp, equal);
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

static int
database_select_metalist_cb (void *user_data,
                             int argc, char **argv, char **column)
{
  database_cb_t *data_cb = user_data;
  valhalla_db_metares_t res;

  if (argc != 5)
    return 0;

  res.meta_id    = (int64_t) strtoimax (argv[0], NULL, 10);
  res.meta_name  = argv[2];
  res.data_id    = (int64_t) strtoimax (argv[1], NULL, 10);
  res.data_value = argv[3];
  res.group      = database_group_get (data_cb->database,
                                       (int64_t) strtoimax (argv[4], NULL, 10));

  /* send to the frontend */
  return data_cb->cb_mr (data_cb->data, &res);
}

int
database_metalist_get (database_t *database,
                       valhalla_db_item_t *search,
                       valhalla_db_restrict_t *restriction,
                       int (*select_cb) (void *data,
                                         valhalla_db_metares_t *res),
                       void *data)
{
  database_cb_t data_cb;
  char *msg = NULL;
  char sql[SQL_BUFFER] = SELECT_LIST_METADATA_FROM;

  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  if (restriction)
  {
    database_list_get_restriction (restriction, sql);
    SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  SQL_CONCAT_TYPE (sql, *search, META);
  if (search->group)
  {
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT (sql, SELECT_LIST_WHERE_GROUP_ID,
                database->meta_group[search->group].id);
  }

  SQL_CONCAT (sql, SELECT_LIST_METADATA_END);

  valhalla_log (VALHALLA_MSG_VERBOSE, "query: %s", sql);

  data_cb.cb_mr    = select_cb;
  data_cb.data     = data;
  data_cb.database = database;

  DATABASE_RETURN_SQL_EXEC (database->db,
                            sql, database_select_metalist_cb, data_cb, msg)
}

static int
database_select_filelist_cb (void *user_data,
                             int argc, char **argv, char **column)
{
  database_cb_t *data_cb = user_data;
  valhalla_db_fileres_t res;

  if (argc != 3)
    return 0;

  res.id   = (int64_t) strtoimax (argv[0], NULL, 10);
  res.path = argv[1];
  res.type = database_file_type_get (data_cb->database,
                                     (int64_t) strtoimax (argv[2], NULL, 10));

  /* send to the frontend */
  return data_cb->cb_fr (data_cb->data, &res);
}

int
database_filelist_get (database_t *database,
                       valhalla_file_type_t filetype,
                       valhalla_db_restrict_t *restriction,
                       int (*select_cb) (void *data,
                                         valhalla_db_fileres_t *res),
                       void *data)
{
  database_cb_t data_cb;
  char *msg = NULL;
  char sql[SQL_BUFFER] = SELECT_LIST_FILE_FROM;

  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  if (restriction)
    database_list_get_restriction (restriction, sql);

  if (filetype)
  {
    SQL_CONCAT (sql, SELECT_LIST_AND);
    SQL_CONCAT (sql, SELECT_LIST_WHERE_TYPE_ID,
                database->file_type[filetype].id);
  }

  SQL_CONCAT (sql, SELECT_LIST_FILE_END);

  valhalla_log (VALHALLA_MSG_VERBOSE, "query: %s", sql);

  data_cb.cb_fr    = select_cb;
  data_cb.data     = data;
  data_cb.database = database;

  DATABASE_RETURN_SQL_EXEC (database->db,
                            sql, database_select_filelist_cb, data_cb, msg)
}

static int
database_select_file_cb (void *user_data,
                         int argc, char **argv, char **column)
{
  database_cb_t *data_cb = user_data;
  valhalla_db_filemeta_t **res, *new;

  if (argc != 6)
    return 0;

  res = data_cb->data;

  if (*res)
  {
    for (new = *res; new->next; new = new->next)
      ;
    new->next = calloc (1, sizeof (valhalla_db_filemeta_t));
    new = new->next;
  }
  else
  {
    *res = calloc (1, sizeof (valhalla_db_filemeta_t));
    new = *res;
  }

  if (!new)
    return 0;

  new->meta_id    = (int64_t) strtoimax (argv[2], NULL, 10);
  new->meta_name  = strdup (argv[4]);
  new->data_id    = (int64_t) strtoimax (argv[3], NULL, 10);
  new->data_value = strdup (argv[5]);

  new->group = database_group_get (data_cb->database,
                                   (int64_t) strtoimax (argv[1], NULL, 10));

  return 0;
}

int
database_file_get (database_t *database,
                   int64_t id, const char *path,
                   valhalla_db_restrict_t *restriction,
                   valhalla_db_filemeta_t **res)
{
  database_cb_t data_cb;
  char *msg = NULL;
  char sql[SQL_BUFFER] = SELECT_FILE_FROM;

  SQL_CONCAT (sql, SELECT_LIST_WHERE);

  if (restriction)
  {
    database_list_get_restriction (restriction, sql);
    SQL_CONCAT (sql, SELECT_LIST_AND);
  }

  if (id)
    SQL_CONCAT (sql, SELECT_FILE_WHERE_FILE_ID, id);
  else if (path)
    SQL_CONCAT (sql, SELECT_FILE_WHERE_FILE_PATH, path);
  else
    return -1;

  SQL_CONCAT (sql, SELECT_FILE_END);

  valhalla_log (VALHALLA_MSG_VERBOSE, "query: %s", sql);

  *res = NULL;
  data_cb.data     = res;
  data_cb.database = database;

  DATABASE_RETURN_SQL_EXEC (database->db,
                            sql, database_select_file_cb, data_cb, msg)
}
