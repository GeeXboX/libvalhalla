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

#ifndef VALHALLA_UTILS_H
#define VALHALLA_UTILS_H

#include <time.h>
#include <semaphore.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "metadata.h"
#include "fifo_queue.h"

typedef enum processing_step {
  STEP_PARSING = 0,
#ifdef USE_GRABBER
  STEP_GRABBING,
  STEP_DOWNLOADING,
#endif /* USE_GRABBER */
  STEP_ENDING,
} processing_step_t;

typedef struct file_dl_s {
  struct file_dl_s *next;
  char         *url;
  valhalla_dl_t dst;
  char         *name;
} file_dl_t;

typedef struct file_data_s {
  char                *file;
  time_t               mtime;
  valhalla_file_type_t type;
  metadata_t          *meta_parser;
  processing_step_t    step;

  /* grabbing attributes */
  int         wait;
  metadata_t *meta_grabber;
  sem_t       sem_grabber;
  int         grabber_cnt;

  /* downloading attribute */
  file_dl_t  *list_downloader;

  int         clean_f;
} file_data_t;


void my_strtolower (char *str);
int file_exists (const char *file);
void file_dl_add (file_dl_t **dl,
                  const char *url, const char *name, valhalla_dl_t dst);
void file_data_free (file_data_t *data);
void file_data_step_increase (file_data_t *data, action_list_t *action);
void file_data_step_continue (file_data_t *data, action_list_t *action);
void queue_cleanup (fifo_queue_t *queue);

#endif /* VALHALLA_UTILS_H */
