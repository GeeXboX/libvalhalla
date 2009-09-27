/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2008-2009 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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

#ifndef VALHALLA_FIFO_QUEUE_H
#define VALHALLA_FIFO_QUEUE_H

typedef struct fifo_queue_s fifo_queue_t;

enum fifo_queue_errno {
  FIFO_QUEUE_ERROR_QUEUE  = -3,
  FIFO_QUEUE_ERROR_EMPTY  = -2,
  FIFO_QUEUE_ERROR_MALLOC = -1,
  FIFO_QUEUE_SUCCESS      =  0,
};

typedef enum fifo_queue_prio {
  FIFO_QUEUE_PRIORITY_NORMAL,
  FIFO_QUEUE_PRIORITY_HIGH,
} fifo_queue_prio_t;


fifo_queue_t *vh_fifo_queue_new (void);
void vh_fifo_queue_free (fifo_queue_t *queue);

int vh_fifo_queue_push (fifo_queue_t *queue,
                        fifo_queue_prio_t p, int id, void *data);
int vh_fifo_queue_pop (fifo_queue_t *queue, int *id, void **data);

#endif /* VALHALLA_FIFO_QUEUE_H */
