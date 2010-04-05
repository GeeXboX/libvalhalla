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

#ifndef VALHALLA_URL_UTILS_H
#define VALHALLA_URL_UTILS_H

typedef struct url_data_s {
  int status;
  char *buffer;
  size_t size;
} url_data_t;

typedef void url_t;
typedef struct url_ctl_s url_ctl_t;

void vh_url_global_init (void);
void vh_url_global_uninit (void);

url_t *vh_url_new (url_ctl_t *abort);
void vh_url_free (url_t *url);
url_data_t vh_url_get_data (url_t *handler, char *url);
char *vh_url_escape_string (url_t *handler, const char *buf);
int vh_url_save_to_disk (url_t *handler, char *src, char *dst);

url_ctl_t *vh_url_ctl_new (void);
void vh_url_ctl_free (url_ctl_t *url_ctl);
void vh_url_ctl_abort (url_ctl_t *url_ctl);

#endif /* VALHALLA_URL_UTILS_H */
