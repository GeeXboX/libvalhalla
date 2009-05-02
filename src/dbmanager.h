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

#ifndef VALHALLA_DBMANAGER_H
#define VALHALLA_DBMANAGER_H

typedef struct dbmanager_s dbmanager_t;

enum dbmanager_errno {
  DBMANAGER_ERROR_HANDLER = -2,
  DBMANAGER_ERROR_THREAD  = -1,
  DBMANAGER_SUCCESS       =  0,
};

int dbmanager_run (dbmanager_t *dbmanager, int priority);
void dbmanager_stop (dbmanager_t *dbmanager);
void dbmanager_uninit (dbmanager_t *dbmanager);
dbmanager_t *dbmanager_init (valhalla_t *handle,
                             const char *db, unsigned int commit_int);

void dbmanager_action_send (dbmanager_t *dbmanager, int action, void *data);

int dbmanager_db_author (dbmanager_t *dbmanager,
                         valhalla_db_author_t *author, int64_t album);
int dbmanager_db_album (dbmanager_t *dbmanager,
                        valhalla_db_album_t *album, int64_t where_id, int what);
int dbmanager_db_genre (dbmanager_t *dbmanager, valhalla_db_genre_t *genre);
int dbmanager_db_file (dbmanager_t *dbmanager, valhalla_db_file_t *file,
                       valhalla_db_file_where_t *where);

#endif /* VALHALLA_DBMANAGER_H */
