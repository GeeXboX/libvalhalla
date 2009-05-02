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

#ifndef VALHALLA_DATABASE_H
#define VALHALLA_DATABASE_H

#include "utils.h"

typedef struct database_s database_t;

void database_file_data_insert (database_t *database, file_data_t *data);
void database_file_data_update (database_t *database, file_data_t *data);
void database_file_data_delete (database_t *database, const char *file);
int database_file_get_mtime (database_t *db, const char *file);

void database_file_checked_clear (database_t *database);
const char *database_file_get_checked_clear (database_t *database);

void database_begin_transaction (database_t *database);
void database_end_transaction (database_t *database);
void database_step_transaction (database_t *database,
                                unsigned int interval, int value);

database_t *database_init (const char *path);
void database_uninit (database_t *database);
int database_cleanup (database_t *database);


int database_select_author (database_t *database,
                            int64_t *id, const char **name, int64_t album);
int database_select_album (database_t *database, int64_t *id,
                           const char **name, int64_t where_id, int what);
int database_select_genre (database_t *database,
                           int64_t *id, const char **name);
int database_select_file (database_t *database, valhalla_db_file_t *file,
                          valhalla_db_file_where_t *where);

#endif /* VALHALLA_DATABASE_H */
