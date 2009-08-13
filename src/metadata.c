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

#include "valhalla.h"
#include "valhalla_internals.h"
#include "metadata.h"
#include "logs.h"


int
metadata_get (metadata_t *meta,
              const char *name, int flags, metadata_t **tag)
{
  if (!meta || !tag || !name)
    return -1;

  if (*tag)
    meta = (*tag)->next;

  for (; meta; meta = meta->next)
    if (flags & METADATA_IGNORE_SUFFIX
        ? !strncmp (name, meta->name, strlen (name))
        : !strcmp (name, meta->name))
      break;

  if (!meta)
    return -1;

  *tag = meta;
  return 0;
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
metadata_add (metadata_t **meta,
              const char *name, const char *value, valhalla_meta_grp_t group)
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
  it->group = group;

  valhalla_log (VALHALLA_MSG_VERBOSE,
                "Adding new metadata '%s' with value '%s'.",
                it->name, it->value);
}
