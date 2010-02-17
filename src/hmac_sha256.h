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

#ifndef VALHALLA_HMAC_SHA256_H
#define VALHALLA_HMAC_SHA256_H

typedef struct hmac_sha256_s hmac_sha256_t;

#define VH_HMAC_SHA256_SIZE 32


hmac_sha256_t *vh_hmac_sha256_new (const char *key);
void vh_hmac_sha256_free (hmac_sha256_t *hd);
void vh_hmac_sha256_reset (hmac_sha256_t *hd);
unsigned char *vh_hmac_sha256_compute (hmac_sha256_t *hd,
                                       const char *src, size_t size);

#endif /* VALHALLA_HMAC_SHA256_H */
