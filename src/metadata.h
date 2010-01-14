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

#ifndef VALHALLA_METADATA
#define VALHALLA_METADATA

#include "valhalla.h"

typedef struct metadata_s {
  struct metadata_s *next;
  char *name;
  char *value;
  valhalla_meta_grp_t group;
  int   priority;
} metadata_t;

typedef struct metadata_plist_s {
  const char *metadata;
  valhalla_grabber_pl_t priority;
} metadata_plist_t;

#define METADATA_IGNORE_SUFFIX (1 << 0)

extern const size_t vh_metadata_group_size;

const char *vh_metadata_group_str (valhalla_meta_grp_t group);
int vh_metadata_get (const metadata_t *meta,
                     const char *name, int flags, const metadata_t **tag);
void vh_metadata_free (metadata_t *meta);
void vh_metadata_add (metadata_t **meta, const char *name,
                      const char *value, valhalla_meta_grp_t group,
                      valhalla_grabber_pl_t priority);
void vh_metadata_add_auto (metadata_t **meta, const char *name,
                           const char *value, const metadata_plist_t *pl);
void vh_metadata_dup (metadata_t **dst, const metadata_t *src);

#endif /* VALHALLA_METADATA */
