/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009-2010 Mathieu Schroeter <mathieu@schroetersa.ch>
 *
 * The HMAC-SHA2 digest is freely inspired by FFmpeg/libavformat/rtmpproto.c.
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

#include "sha.h"
#include "hmac_sha256.h"

struct hmac_sha256_s {
  uint8_t  *key;
  size_t    keylen;

  vh_sha_t *sha;
  uint8_t   dst[VH_HMAC_SHA256_SIZE];
};


hmac_sha256_t *
vh_hmac_sha256_new (const char *key)
{
  hmac_sha256_t *hd;

  hd = calloc (1, sizeof (hmac_sha256_t));
  if (!hd)
    return NULL;

  hd->sha = calloc (1, vh_sha_size);
  if (!hd->sha)
    goto err;

  hd->key = (uint8_t *) strdup (key);
  if (!hd->key)
    goto err;

  hd->keylen = strlen (key);
  return hd;

 err:
  if (hd->sha)
    free (hd->sha);
  free (hd);
  return NULL;
}

void
vh_hmac_sha256_free (hmac_sha256_t *hd)
{
  if (!hd)
    return;

  free (hd->key);
  free (hd->sha);
  free (hd);
}

void
vh_hmac_sha256_reset (hmac_sha256_t *hd)
{
  if (!hd)
    return;

  memset (hd->sha, 0, vh_sha_size);
  memset (hd->dst, 0, VH_HMAC_SHA256_SIZE);
}

#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5C

uint8_t *
vh_hmac_sha256_compute (hmac_sha256_t *hd, const char *src, size_t size)
{
  int i;
  uint8_t hmac_buf[64 + 32] = { 0 };

  if (!hd || !src)
    return NULL;

  if (hd->keylen < 64)
    memcpy (hmac_buf, hd->key, hd->keylen);
  else
  {
    vh_sha_init   (hd->sha, 256);
    vh_sha_update (hd->sha, hd->key, hd->keylen);
    vh_sha_final  (hd->sha, hmac_buf);
  }

  for (i = 0; i < 64; i++)
    hmac_buf[i] ^= HMAC_IPAD;

  vh_sha_init   (hd->sha, 256);
  vh_sha_update (hd->sha, hmac_buf, 64);
  vh_sha_update (hd->sha, (uint8_t *) src, size);
  vh_sha_final  (hd->sha, hmac_buf + 64);

  for (i = 0; i < 64; i++)
    hmac_buf[i] ^= HMAC_IPAD ^ HMAC_OPAD;

  vh_sha_init   (hd->sha, 256);
  vh_sha_update (hd->sha, hmac_buf, 64 + 32);
  vh_sha_final  (hd->sha, hd->dst);

  return hd->dst;
}
