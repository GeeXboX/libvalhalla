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

#ifndef VALHALLA_EVENT_HANDLER_H
#define VALHALLA_EVENT_HANDLER_H

#include "fifo_queue.h"

typedef struct event_handler_s event_handler_t;

enum event_handler_errno {
  EVENT_HANDLER_ERROR_HANDLER = -2,
  EVENT_HANDLER_ERROR_THREAD  = -1,
  EVENT_HANDLER_SUCCESS       =  0,
};

typedef struct event_handler_data_s {
  char            *file;
  valhalla_event_t e;
  const char      *id;
} event_handler_data_t;


int vh_event_handler_run (event_handler_t *event_handler, int priority);
fifo_queue_t *vh_event_handler_fifo_get (event_handler_t *event_handler);
void vh_event_handler_stop (event_handler_t *event_handler, int f);
void vh_event_handler_uninit (event_handler_t *event_handler);
event_handler_t *vh_event_handler_init (valhalla_t *handle,
                                        void (*od_cb) (const char *file,
                                                    valhalla_event_t e,
                                                    const char *id, void *data),
                                        void *od_data,
                                        void (*gl_cb) (valhalla_event_gl_t e,
                                                       void *data),
                                        void *gl_data);

void vh_event_handler_send (event_handler_t *event_handler, const char *file,
                            valhalla_event_t e, const char *id);
void vh_event_handler_gl_send (event_handler_t *event_handler,
                               valhalla_event_gl_t e);

#endif /* VALHALLA_EVENT_HANDLER_H */
