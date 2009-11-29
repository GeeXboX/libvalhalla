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

#ifndef VALHALLA_UTILS_H
#define VALHALLA_UTILS_H

#include <time.h>
#include <semaphore.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "metadata.h"
#include "fifo_queue.h"
#include "list.h"

typedef enum od_type {
  OD_TYPE_DEF = 0,  /* created by "scanner" (default value) */
  OD_TYPE_NEW,      /* created by "ondemand" */
  OD_TYPE_UPD,      /* created by "scanner"; updated by "ondemand" */
} od_type_t;

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
  int                  outofpath;
  od_type_t            od;
  fifo_queue_prio_t    priority;
  metadata_t          *meta_parser;
  processing_step_t    step;

  /* grabbing attributes */
  int         wait;
  metadata_t *meta_grabber;
  const char *grabber_name;
  sem_t       sem_grabber;
  int         grabber_cnt;
  list_t     *grabber_list; /* grabbers already handled before interruption */

  /* downloading attribute */
  file_dl_t  *list_downloader;

  int         clean_f;
} file_data_t;


void vh_strtolower (char *str);
char *vh_strrcasestr (const char *buf, const char *str);
int vh_file_exists (const char *file);
int vh_file_copy (const char *src, const char *dst);
void vh_file_dl_add (file_dl_t **dl,
                     const char *url, const char *name, valhalla_dl_t dst);
void vh_file_data_free (file_data_t *data);
file_data_t *vh_file_data_new (const char *file, time_t mtime,
                               int outofpath, od_type_t od,
                               fifo_queue_prio_t prio, processing_step_t step);
void vh_file_data_step_increase (file_data_t *data, action_list_t *action);
void vh_file_data_step_continue (file_data_t *data, action_list_t *action);
void vh_queue_cleanup (fifo_queue_t *queue);
int vh_get_list_length (void *list);

#define VH_TIMERSUB(a, b, result)                     \
  do                                                  \
  {                                                   \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;     \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;  \
    if ((result)->tv_nsec < 0)                        \
    {                                                 \
      --(result)->tv_sec;                             \
      (result)->tv_nsec += 1000000000;                \
    }                                                 \
  }                                                   \
  while (0)

#define VH_TIMERADD(a, b, result)                     \
  do                                                  \
  {                                                   \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;     \
    (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec;  \
    if ((result)->tv_nsec >= 1000000000)              \
    {                                                 \
      ++(result)->tv_sec;                             \
      (result)->tv_nsec -= 1000000000;                \
    }                                                 \
  }                                                   \
  while (0)

#endif /* VALHALLA_UTILS_H */
