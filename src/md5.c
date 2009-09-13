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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <libavutil/md5.h>

#define MD5_SUM_SIZE 16
#define MD5_STR_SIZE (MD5_SUM_SIZE * 2 + 1)


char *
vh_md5sum (const char *str)
{
  unsigned char sum[MD5_SUM_SIZE];
  char md5[MD5_STR_SIZE];
  int i;

  if (!str)
    return NULL;

  av_md5_sum (sum, (const uint8_t *) str, strlen (str));
  memset (md5, '\0', MD5_STR_SIZE);

  for (i = 0; i < MD5_SUM_SIZE; i++)
  {
    char tmp[3];
    sprintf (tmp, "%02x", sum[i]);
    strcat (md5, tmp);
  }

  return strdup (md5);
}
