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

#include <stdlib.h>
#include <ctype.h>

#include "valhalla.h"
#include "valhalla_internals.h"
#include "metadata.h"
#include "fifo_queue.h"
#include "utils.h"


void
my_strtolower (char *str)
{
  if (!str)
    return;

  for (; *str; str++)
    *str = (char) tolower ((int) (unsigned char) *str);
}

void
file_data_free (file_data_t *data)
{
  if (!data)
    return;

  if (data->file)
    free (data->file);
  if (data->meta)
    metadata_free (data->meta);
  free (data);
}

void
queue_cleanup (fifo_queue_t *queue)
{
  int e;
  void *data;

  fifo_queue_push (queue, ACTION_CLEANUP_END, NULL);

  do
  {
    e = ACTION_NO_OPERATION;
    data = NULL;
    fifo_queue_pop (queue, &e, &data);

    switch (e)
    {
    default:
      break;

    case ACTION_DB_INSERT:
    case ACTION_DB_UPDATE:
    case ACTION_DB_NEWFILE:
      if (data)
        file_data_free (data);
      break;
    }
  }
  while (e != ACTION_CLEANUP_END);
}
