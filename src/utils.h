/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

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
  valhalla_file_t      file;
  int                  outofpath;
  od_type_t            od;
  fifo_queue_prio_t    priority;
  metadata_t          *meta_parser;
  processing_step_t    step;

  /* grabbing attributes */
  unsigned int skip : 1; /* when all grabber threads are busy */
  unsigned int wait : 1;
  metadata_t  *meta_grabber;
  const char  *grabber_name;
  sem_t        sem_grabber;
  list_t      *grabber_list; /* grabbers already handled */

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
file_data_t *vh_file_data_new (const char *file, struct stat *st,
                               int outofpath, od_type_t od,
                               fifo_queue_prio_t prio, processing_step_t step);
void vh_file_data_step_increase (file_data_t *data, action_list_t *action);
void vh_file_data_step_continue (file_data_t *data, action_list_t *action);
int vh_get_list_length (void *list);

#define ARRAY_NB_ELEMENTS(array) (sizeof (array) / sizeof (array[0]))

#define VH_ISALNUM(c) isalnum ((int) (unsigned char) (c))
#define VH_ISGRAPH(c) isgraph ((int) (unsigned char) (c))
#define VH_ISSPACE(c) isspace ((int) (unsigned char) (c))
#define VH_TOLOWER(c) tolower ((int) (unsigned char) (c))

#define VH_TIMERNOW(t)                                \
  do                                                  \
  {                                                   \
    struct timespec tp;                               \
    *(t) = 0;                                         \
    if (!clock_gettime (CLOCK_REALTIME, &tp))         \
      *(t) = tp.tv_sec * 1000000000 + tp.tv_nsec;     \
  }                                                   \
  while (0)

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
