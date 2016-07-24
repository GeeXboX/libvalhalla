/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2016 Mathieu Schroeter <mathieu@schroetersa.ch>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "list.h"
#include "json_utils.h"


typedef struct item_s {
  char *item;
  unsigned int index;
  _Bool is_array;
} item_t;


static item_t *
item_array_new (const char *item, unsigned int index)
{
  if (!item)
    return NULL;

  item_t *it = calloc (1, sizeof (*it));
  if (!it)
    return NULL;

  it->item     = strdup (item);
  it->is_array = true;
  it->index    = index;
  return it;
}

static item_t *
item_new (const char *item)
{
  if (!item)
    return NULL;

  item_t *it = calloc (1, sizeof (*it));
  if (!it)
    return NULL;

  it->item = strdup (item);
  return it;
}

static void
item_free (item_t *it)
{
  if (!it)
    return;

  if (it->item)
    free (it->item);
  free (it);
}

static list_t *
tokenize (const char *path)
{
  if (!path)
    return NULL;

  char *p = strdup (path);
  char *token = strtok (p, ".");

  if (!token)
    return NULL;

  list_t *tokens = vh_list_new (0, (void *) item_free);

  while (token)
  {
    item_t *it = NULL;
    char item[strlen (token) + 1];
    unsigned int index = 0;

    int nb = sscanf (token, "%[^[][%u]", item, &index);
    if (nb < 2)
      it = item_new (item);
    else
      it = item_array_new (item, index);

    vh_list_append (tokens, it, sizeof (*it));
    token = strtok (NULL, ".");

    free (it);
  }

  return tokens;
}

static json_object *
foreach_item (json_object *json, const item_t *it)
{
  json_object *value = NULL;

  json_object_object_get_ex (json, it->item, &value);

  if (!it->is_array)
    return value;

  return json_object_array_get_idx (value, it->index);
}

json_object *
vh_json_get (json_object *json, const char *path)
{
  if (!json || !path)
    return NULL;

  list_t *items = tokenize (path);
  if (!items)
    return NULL;

  json_object *res = vh_list_foreach (items, json, (void *) foreach_item);
  vh_list_free (items);
  return res;
}

char *
vh_json_get_str (json_object *json, const char *path)
{

}

int
vh_json_get_int (json_object *json, const char *path)
{

}
