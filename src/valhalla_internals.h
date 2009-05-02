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

#ifndef VALHALLA_INTERNALS_H
#define VALHALLA_INTERNALS_H

#include <time.h>

#include "metadata.h"

struct fifo_queue_s;
struct scanner_s;
struct parser_s;
struct dbmanager_s;

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

typedef struct file_data_s {
  char       *file;
  time_t      mtime;
  metadata_t *meta;
} file_data_t;

struct valhalla_s {
  struct scanner_s *scanner;
  struct parser_s *parser;
  struct dbmanager_s *dbmanager;

  int run;      /* prevent a bug if valhalla_run() is called two times */
};

#define ARRAY_NB_ELEMENTS(array) (sizeof (array) / sizeof (array[0]))

void valhalla_queue_cleanup (struct fifo_queue_s *queue);
void file_data_free (file_data_t *data);

#endif /* VALHALLA_INTERNALS_H */
