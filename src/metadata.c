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
#include <string.h>

#include "valhalla_internals.h"

struct metadata_s {
  struct metadata_s *next;
  char *name;
  char *value;
};


const char *
metadata_get (metadata_t *meta, const char *name)
{
  for (; meta; meta = meta->next)
    if (!strcasecmp (meta->name, name))
      return meta->value;

  return NULL;
}

void
metadata_free (metadata_t *meta)
{
  metadata_t *tmp;

  while (meta)
  {
    free (meta->name);
    free (meta->value);
    tmp = meta->next;
    free (meta);
    meta = tmp;
  }
}

void
metadata_add (metadata_t **meta, const char *name, const char *value)
{
  metadata_t *it;

  if (!meta || !name || !value)
    return;

  if (!*meta)
  {
    *meta = calloc (1, sizeof (metadata_t));
    it = *meta;
  }
  else
  {
    for (it = *meta; it->next; it = it->next)
      ;
    it->next = calloc (1, sizeof (metadata_t));
    it = it->next;
  }

  if (!it)
    return;

  it->name  = strdup (name);
  it->value = strdup (value);
}
