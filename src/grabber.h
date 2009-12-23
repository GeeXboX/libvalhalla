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

#define GRABBER_NUMBER_DEF 2


int vh_grabber_run (grabber_t *grabber, int priority);
void vh_grabber_pause (grabber_t *grabber);
fifo_queue_t *vh_grabber_fifo_get (grabber_t *grabber);
void vh_grabber_state_set (grabber_t *grabber, const char *id, int enable);
const char *vh_grabber_next (grabber_t *grabber, const char *id);
void vh_grabber_stop (grabber_t *grabber, int f);
void vh_grabber_uninit (grabber_t *grabber);
grabber_t *vh_grabber_init (valhalla_t *handle, unsigned int nb);

void vh_grabber_action_send (grabber_t *grabber,
                             fifo_queue_prio_t prio, int action, void *data);

#endif /* VALHALLA_GRABBER_H */
