/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu.schroeter@gamesover.ch>
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

#ifndef VALHALLA_SHA_H
#define VALHALLA_SHA_H

#include <inttypes.h>

typedef struct vh_sha_s vh_sha_t;

extern const int vh_sha_size;

int vh_sha_init (vh_sha_t *context, int bits);
void vh_sha_update (vh_sha_t *context,
                    const uint8_t *data, unsigned int len);
void vh_sha_final (vh_sha_t *context, uint8_t *digest);

#endif /* VALHALLA_SHA_H */
