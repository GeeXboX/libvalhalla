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

#ifndef VALHALLA_GRABBER_H
#define VALHALLA_GRABBER_H

#include "fifo_queue.h"
#include "grabber_common.h"

typedef struct grabber_s grabber_t;

enum grabber_errno {
  GRABBER_ERROR_HANDLER = -2,
  GRABBER_ERROR_THREAD  = -1,
  GRABBER_SUCCESS       =  0,
};


int grabber_run (grabber_t *grabber, int priority);
fifo_queue_t *grabber_fifo_get (grabber_t *grabber);
void grabber_state_set (grabber_t *grabber, const char *id, int enable);
const char *grabber_list_get (grabber_t *grabber, const char *id);
void grabber_stop (grabber_t *grabber);
void grabber_uninit (grabber_t *grabber);
grabber_t *grabber_init (valhalla_t *handle);

void grabber_action_send (grabber_t *grabber, int action, void *data);

#endif /* VALHALLA_GRABBER_H */
