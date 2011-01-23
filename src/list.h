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

#ifndef VALHALLA_LIST_H
#define VALHALLA_LIST_H

typedef struct list_s list_t;


void vh_list_append (list_t *list, const void *data, size_t len);
list_t *vh_list_new (unsigned int depth, void (*free_fct) (void *data));
void vh_list_free (list_t *list);
void vh_list_empty (list_t *list);
void *vh_list_search (const list_t *list, const void *tocmp,
                      int (*cmp_fct) (const void *tocmp, const void *data));
void *vh_list_pos (const list_t *list, unsigned int pos);

#endif /* VALHALLA_LIST_H */
