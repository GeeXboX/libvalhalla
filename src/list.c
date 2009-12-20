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

#include <stdlib.h>
#include <string.h>

#include "list.h"

typedef struct list_item_s {
  struct list_item_s *next;
  void *data;
} list_item_t;

struct list_s {
  list_item_t *item;
  list_item_t *item_last;
  void (*free_fct) (void *data);
  unsigned int depth; /* 0: infinite */
  unsigned int cnt;
};


static inline void
list_item_free (list_t *list, list_item_t *item)
{
  if (!list || !item)
    return;

  if (item->data)
  {
    if (list->free_fct)
      list->free_fct (item->data);
    else
      free (item->data);
  }

  free (item);
}

void
vh_list_append (list_t *list, const void *data, size_t len)
{
  list_item_t *item;

  if (!list || !data)
    return;

  if (!list->item)
  {
    list->item = calloc (1, sizeof (list_item_t));
    item = list->item;
  }
  else
  {
    list->item_last->next = calloc (1, sizeof (list_item_t));
    item = list->item_last->next;
  }

  if (!item)
    return;

  /* Remove the older item to preserve the depth. */
  if (list->depth)
  {
    if (list->cnt >= list->depth)
    {
      list_item_t *item_n = list->item->next;
      list_item_free (list, list->item);
      list->item = item_n;
    }
    else
      list->cnt++;
  }

  list->item_last = item;

  item->data = calloc (1, len);
  if (item->data)
    memcpy (item->data, data, len);
}

list_t *
vh_list_new (unsigned int depth, void (*free_fct) (void *data))
{
  list_t *list;

  list = calloc (1, sizeof (list_t));
  if (!list)
    return NULL;

  list->free_fct = free_fct;
  list->depth    = depth;
  return list;
}

void
vh_list_empty (list_t *list)
{
  list_item_t *item, *item_n;

  if (!list)
    return;

  item = list->item;
  while (item)
  {
    item_n = item->next;
    list_item_free (list, item);
    item = item_n;
  }

  list->item      = NULL;
  list->item_last = NULL;
  list->cnt       = 0;
}

void
vh_list_free (list_t *list)
{
  if (!list)
    return;

  vh_list_empty (list);
  free (list);
}

void *
vh_list_search (list_t *list, const void *tocmp,
                int (*cmp_fct) (const void *tocmp, const void *data))
{
  list_item_t *item;

  if (!list || !tocmp || !cmp_fct)
    return NULL;

  for (item = list->item; item; item = item->next)
    if (!cmp_fct (tocmp, item->data))
      return item->data;

  return NULL;
}
