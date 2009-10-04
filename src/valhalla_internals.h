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

#ifndef VALHALLA_INTERNALS_H
#define VALHALLA_INTERNALS_H

typedef enum action_list {
  ACTION_KILL_THREAD  = -1, /* auto-kill when all pending commands are ended */
  ACTION_NO_OPERATION =  0, /* wake-up for nothing */
  ACTION_PAUSE_THREAD,      /* send thread in waiting list */
  ACTION_DB_INSERT_P,       /* dispatcher: parser  metadata ok, insert in DB */
  ACTION_DB_INSERT_G,       /* dispatcher: grabber metadata ok, insert in DB */
  ACTION_DB_UPDATE_P,       /* dispatcher: parser  metadata ok, update in DB */
  ACTION_DB_UPDATE_G,       /* dispatcher: grabber metadata ok, update in DB */
  ACTION_DB_END,            /* dispatcher: end metadata */
  ACTION_DB_NEWFILE,        /* scanner: new file to handle */
  ACTION_DB_NEXT_LOOP,      /* scanner: stop db manage queue for next loop */
  ACTION_ACKNOWLEDGE,       /* dbmanager: ack scanner for each file handled */
  ACTION_CLEANUP_END,       /* special case for garbage collector */
} action_list_t;

typedef enum processing_step {
  STEP_PARSING = 0,
#ifdef USE_GRABBER
  STEP_GRABBING,
  STEP_DOWNLOADING,
#endif /* USE_GRABBER */
  STEP_ENDING,
} processing_step_t;

struct valhalla_s {
  struct scanner_s    *scanner;
  struct dispatcher_s *dispatcher;
  struct parser_s     *parser;
#ifdef USE_GRABBER
  struct grabber_s    *grabber;
  struct downloader_s *downloader;
#endif /* USE_GRABBER */
  struct dbmanager_s  *dbmanager;

  int run; /* prevent a bug if valhalla_run() is called two times */
};

#define ARRAY_NB_ELEMENTS(array) (sizeof (array) / sizeof (array[0]))

#ifndef vh_unused
#if defined(__GNUC__)
#    define vh_unused __attribute__((unused))
#else
#    define vh_unused
#endif
#endif


/****************************************************************************/
/* Thread 'pause' helpers                                                   */
/****************************************************************************/

#define VH_THREAD_PAUSE_ATTRS                                       \
  int   paused;                                                     \
  sem_t sem_pause;                                                  \
  sem_t sem_pausing;

#define VH_THREAD_PAUSE_INIT(handle)                                \
  handle->paused = 0;                                               \
  sem_init (&handle->sem_pause, 0, 0);                              \
  sem_init (&handle->sem_pausing, 0, 0);

#define VH_THREAD_PAUSE_UNINIT(handle)                              \
  sem_destroy (&handle->sem_pause);                                 \
  sem_destroy (&handle->sem_pausing);

#define VH_THREAD_PAUSE_FCT(handle, nb)                             \
  {                                                                 \
    unsigned int i;                                                 \
    if (handle->paused)                                             \
    {                                                               \
      handle->paused = 0;                                           \
      for (i = 0; i < nb; i++)                                      \
        sem_post (&handle->sem_pause);                              \
    }                                                               \
    else                                                            \
    {                                                               \
      for (i = 0; i < nb; i++)                                      \
      {                                                             \
        vh_fifo_queue_push (handle->fifo, FIFO_QUEUE_PRIORITY_HIGH, \
                            ACTION_PAUSE_THREAD, NULL);             \
        sem_wait (&handle->sem_pausing);                            \
      }                                                             \
      handle->paused = 1;                                           \
    }                                                               \
  }
#define VH_THREAD_PAUSE_ACTION(handle)                              \
  sem_post (&handle->sem_pausing);                                  \
  sem_wait (&handle->sem_pause);

#endif /* VALHALLA_INTERNALS_H */
