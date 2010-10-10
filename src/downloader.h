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

#ifndef VALHALLA_DOWNLOADER_H
#define VALHALLA_DOWNLOADER_H

#include "valhalla.h"
#include "fifo_queue.h"

typedef struct downloader_s downloader_t;

enum downloader_errno {
  DOWNLOADER_ERROR_HANDLER = -2,
  DOWNLOADER_ERROR_THREAD  = -1,
  DOWNLOADER_SUCCESS       =  0,
};

int vh_downloader_run (downloader_t *downloader, int priority);
void vh_downloader_pause (downloader_t *downloader);
fifo_queue_t *vh_downloader_fifo_get (downloader_t *downloader);
void vh_downloader_stop (downloader_t *downloader, int f);
void vh_downloader_uninit (downloader_t *downloader);
downloader_t *vh_downloader_init (valhalla_t *handle);

void vh_downloader_destination_set (downloader_t *downloader,
                                    valhalla_dl_t dl, const char *dst);
const char *vh_downloader_destination_get (downloader_t *downloader,
                                           valhalla_dl_t dl);
void vh_downloader_action_send (downloader_t *downloader,
                                fifo_queue_prio_t prio, int action, void *data);

#endif /* VALHALLA_DOWNLOADER_H */
