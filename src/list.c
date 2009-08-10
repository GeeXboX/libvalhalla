/*
 * GeeXboX libplayer: a multimedia A/V abstraction layer API.
 * Copyright (C) 2008 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
 *
 * This file is part of libplayer.
 *
 * libplayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplayer; if not, write to the Free Software
 * Foundation, Inc, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>

#include "list.h"

struct list_s {
  struct list_s *next;
  void *data;
};


void
list_append (list_t **list, const void *data, size_t len)
{
  list_t *it;

  if (!list || !data)
    return;

  if (!*list)
  {
    *list = calloc (1, sizeof (list_t));
    it = *list;
  }
  else
  {
    for (it = *list; it->next; it = it->next)
      ;
    it->next = calloc (1, sizeof (list_t));
    it = it->next;
  }

  if (!it)
    return;

  it->data = calloc (1, len);
  if (it->data)
    memcpy (it->data, data, len);
}

void
list_free (list_t *list, void (*free_fct) (void *data))
{
  list_t *list_n;

  while (list)
  {
    list_n = list->next;
    if (list->data)
    {
      if (free_fct)
        free_fct (list->data);
      else
        free (list->data);
    }
    free (list);
    list = list_n;
  }
}

void *
list_search (list_t *list, const void *tocmp,
             int (*cmp_fct) (const void *tocmp, const void *data))
{
  if (!tocmp || !cmp_fct)
    return NULL;

  for (; list; list = list->next)
    if (!cmp_fct (tocmp, list->data))
      return list->data;

  return NULL;
}
