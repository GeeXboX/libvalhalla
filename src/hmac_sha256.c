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

#include <gcrypt.h>

#include "hmac_sha256.h"


hmac_sha256_t *
vh_hmac_sha256_new (const char *key)
{
  gcry_md_hd_t hd;

  gcry_md_open (&hd, GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC);
  if (!hd)
    return NULL;

  gcry_md_setkey (hd, key, strlen (key));
  return hd;
}

void
vh_hmac_sha256_free (hmac_sha256_t *hd)
{
  if (!hd)
    return;

  gcry_md_close (hd);
}

void
vh_hmac_sha256_reset (hmac_sha256_t *hd)
{
  if (!hd)
    return;

  gcry_md_reset (hd);
}

unsigned char *
vh_hmac_sha256_compute (hmac_sha256_t *hd, const char *src, size_t size)
{
  if (!hd || !src)
    return NULL;

  gcry_md_write (hd, src, size);
  return gcry_md_read (hd, 0);
}
