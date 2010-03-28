/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009-2010 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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

#ifndef VALHALLA_DATABASE_H
#define VALHALLA_DATABASE_H

#include "utils.h"
#include "list.h"

typedef struct database_s database_t;

void vh_database_file_insert (database_t *database, file_data_t *data);
void vh_database_file_data_update (database_t *database, file_data_t *data);
void vh_database_file_delete (database_t *database, const char *file);
void vh_database_file_data_delete (database_t *database, const char *file);
void vh_database_file_grab_insert (database_t *database, file_data_t *data);
void vh_database_file_grab_update (database_t *database, file_data_t *data);
void vh_database_file_grab_delete (database_t *database, const char *file);
int64_t vh_database_file_get_mtime (database_t *db, const char *file);
void vh_database_file_get_grabber (database_t *database,
                                   const char *file, list_t *l);
void vh_database_file_insert_dlcontext (database_t *database,
                                        file_data_t *data);
void vh_database_file_get_dlcontext (database_t *database,
                                     const char *file, file_dl_t **dl);
void vh_database_delete_dlcontext (database_t *database);

void vh_database_file_interrupted_clear (database_t *database,
                                         const char *file);
void vh_database_file_interrupted_fix (database_t *database);
int vh_database_file_get_interrupted (database_t *database, const char *file);

void vh_database_file_checked_clear (database_t *database);
const char *vh_database_file_get_checked_clear (database_t *database, int rst);
const char *vh_database_file_get_outofpath_set (database_t *database, int rst);

void vh_database_begin_transaction (database_t *database);
void vh_database_end_transaction (database_t *database);
void vh_database_step_transaction (database_t *database,
                                   unsigned int interval, int value);

database_t *vh_database_init (const char *path);
void vh_database_uninit (database_t *database);
int vh_database_cleanup (database_t *database);


valhalla_db_stmt_t *
vh_database_metalist_get (database_t *database,
                          valhalla_db_item_t *search,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction);
const valhalla_db_metares_t *
vh_database_metalist_read (database_t *database, valhalla_db_stmt_t *vhstmt);

valhalla_db_stmt_t *
vh_database_filelist_get (database_t *database,
                          valhalla_file_type_t filetype,
                          valhalla_db_restrict_t *restriction);
const valhalla_db_fileres_t *
vh_database_filelist_read (database_t *database, valhalla_db_stmt_t *vhstmt);

valhalla_db_stmt_t *
vh_database_file_get (database_t *database, int64_t id, const char *path,
                      valhalla_db_restrict_t *restriction);
const valhalla_db_metares_t *
vh_database_file_read (database_t *database, valhalla_db_stmt_t *vhstmt);


int vh_database_metadata_insert (database_t *database, const char *path,
                                 const char *meta, const char *data,
                                 valhalla_meta_grp_t group);
int vh_database_metadata_update (database_t *database, const char *path,
                                 const char *meta, const char *data,
                                 const char *ndata);
int vh_database_metadata_delete (database_t *database, const char *path,
                                 const char *meta, const char *data);
int vh_database_metadata_priority (database_t *database, const char *path,
                                   const char *meta, const char *data,
                                   valhalla_metadata_pl_t p);

#endif /* VALHALLA_DATABASE_H */
