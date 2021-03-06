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

#ifndef VALHALLA_DISPATCHER_H
#define VALHALLA_DISPATCHER_H

#include "fifo_queue.h"

typedef struct dispatcher_s dispatcher_t;

enum dispatcher_errno {
  DISPATCHER_ERROR_HANDLER = -2,
  DISPATCHER_ERROR_THREAD  = -1,
  DISPATCHER_SUCCESS       =  0,
};

int vh_dispatcher_run (dispatcher_t *dispatcher, int priority);
void vh_dispatcher_pause (dispatcher_t *dispatcher);
fifo_queue_t *vh_dispatcher_fifo_get (dispatcher_t *dispatcher);
void vh_dispatcher_stop (dispatcher_t *dispatcher, int f);
void vh_dispatcher_uninit (dispatcher_t *dispatcher);
dispatcher_t *vh_dispatcher_init (valhalla_t *handle);

void vh_dispatcher_action_send (dispatcher_t *dispatcher,
                                fifo_queue_prio_t prio, int action, void *data);

#endif /* VALHALLA_DISPATCHER_H */
