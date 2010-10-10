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

#ifndef VALHALLA_ONDEMAND_H
#define VALHALLA_ONDEMAND_H

#include "fifo_queue.h"

typedef struct ondemand_s ondemand_t;

enum ondemand_errno {
  ONDEMAND_ERROR_HANDLER = -2,
  ONDEMAND_ERROR_THREAD  = -1,
  ONDEMAND_SUCCESS       =  0,
};


int vh_ondemand_run (ondemand_t *ondemand, int priority);
fifo_queue_t *vh_ondemand_fifo_get (ondemand_t *ondemand);
void vh_ondemand_stop (ondemand_t *ondemand, int f);
void vh_ondemand_uninit (ondemand_t *ondemand);
ondemand_t *vh_ondemand_init (valhalla_t *handle);

void vh_ondemand_action_send (ondemand_t *ondemand,
                              fifo_queue_prio_t prio, int action, void *data);

#endif /* VALHALLA_ONDEMAND_H */
