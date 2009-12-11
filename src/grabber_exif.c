/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Benjamin Zores <ben@geexbox.org>
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
#include <stdio.h>
#include <string.h>

#include <libexif/exif-data.h>

#include "grabber_common.h"
#include "grabber_exif.h"
#include "metadata.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_IMAGE

#define BUF_SIZE 2048


static void
exif_content_foreach_func (ExifEntry *entry, void *data)
{
  char buf[BUF_SIZE] = { 0 };
  file_data_t *fdata = data;

  if (!entry || !fdata)
    return;

  exif_entry_get_value (entry, buf, BUF_SIZE);

  vh_metadata_add (&fdata->meta_grabber,
                   exif_tag_get_name (entry->tag),
                   exif_entry_get_value (entry, buf, BUF_SIZE),
                   VALHALLA_META_GRP_TECHNICAL);
}

static void
exif_data_foreach_func (ExifContent *content, void *data)
{
  exif_content_foreach_entry (content, exif_content_foreach_func, data);
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_exif_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return NULL;
}

static int
grabber_exif_init (vh_unused void *priv)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return 0;
}

static void
grabber_exif_uninit (vh_unused void *priv)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);
}

static int
grabber_exif_grab (vh_unused void *priv, file_data_t *data)
{
  ExifData *d;
  ExifEntry *e;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  d = exif_data_new_from_file (data->file);
  if (!d)
    return -1;

  e = exif_data_get_entry (d, EXIF_TAG_ORIENTATION);
  if (e)
  {
    ExifByteOrder bo;
    int orientation;
    char val[8] = { 0 };

    bo = exif_data_get_byte_order (d);
    orientation = exif_get_short (e->data, bo);

    snprintf (val, sizeof (val), "%d", orientation);
    vh_metadata_add_auto (&data->meta_grabber,
                          VALHALLA_METADATA_PICTURE_ORIENTATION, val);
  }

  exif_data_foreach_content (d, exif_data_foreach_func, data);
  exif_data_unref (d);

  return 0;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_exif_register () */
GRABBER_REGISTER (exif,
                  GRABBER_CAP_FLAGS,
                  grabber_exif_priv,
                  grabber_exif_init,
                  grabber_exif_uninit,
                  grabber_exif_grab,
                  NULL)
