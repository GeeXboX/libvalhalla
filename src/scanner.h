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

#ifndef VALHALLA_SCANNER_H
#define VALHALLA_SCANNER_H

#include "fifo_queue.h"

typedef struct scanner_s scanner_t;

enum scanner_errno {
  SCANNER_ERROR_PATH    = -3,
  SCANNER_ERROR_HANDLER = -2,
  SCANNER_ERROR_THREAD  = -1,
  SCANNER_SUCCESS       =  0,
};


void vh_scanner_wakeup (scanner_t *scanner);
int vh_scanner_run (scanner_t *scanner,
                    int loop, uint16_t timeout, uint16_t delay, int priority);
void vh_scanner_wait (scanner_t *scanner);
fifo_queue_t *vh_scanner_fifo_get (scanner_t *scanner);
void vh_scanner_stop (scanner_t *scanner, int f);
void vh_scanner_uninit (scanner_t *scanner);
scanner_t *vh_scanner_init (valhalla_t *handle);

int vh_scanner_path_cmp (scanner_t *scanner, const char *file);
void vh_scanner_path_add (scanner_t *scanner,
                          const char *location, int recursive);
int vh_scanner_suffix_cmp (scanner_t *scanner, const char *file);
void vh_scanner_suffix_add (scanner_t *scanner, const char *suffix);

void vh_scanner_action_send (scanner_t *scanner,
                             fifo_queue_prio_t prio, int action, void *data);

#endif /* VALHALLA_SCANNER_H */
