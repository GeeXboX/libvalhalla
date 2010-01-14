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

typedef struct grabber_exif_s {
  const metadata_plist_t *pl;
  file_data_t *fdata;   /* exif_content_foreach_func */
} grabber_exif_t;

static const metadata_plist_t exif_pl[] = {
  { NULL,                             METADATA_PRIORITY_HIGH     }
};


static void
exif_content_foreach_func (ExifEntry *entry, void *data)
{
  grabber_exif_t *exif = data;
  char buf[BUF_SIZE] = { 0 };
  file_data_t *fdata;
  const metadata_plist_t *pl;

  if (!entry || !data)
    return;

  fdata = exif->fdata;

  exif_entry_get_value (entry, buf, BUF_SIZE);

  /* get default priority */
  for (pl = exif->pl; pl->metadata; pl++)
    ;

  vh_metadata_add (&fdata->meta_grabber,
                   exif_tag_get_name (entry->tag),
                   exif_entry_get_value (entry, buf, BUF_SIZE),
                   VALHALLA_META_GRP_TECHNICAL, pl->priority);
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

  return calloc (1, sizeof (grabber_exif_t));
}

static int
grabber_exif_init (void *priv, const metadata_plist_t *pl)
{
  grabber_exif_t *exif = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!exif)
    return -1;

  exif->pl = pl;
  return 0;
}

static void
grabber_exif_uninit (void *priv)
{
  grabber_exif_t *exif = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!exif)
    return;

  free (exif);
}

static int
grabber_exif_grab (void *priv, file_data_t *data)
{
  grabber_exif_t *exif = priv;
  ExifData *d;
  ExifEntry *e;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  d = exif_data_new_from_file (data->file.path);
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
                          VALHALLA_METADATA_PICTURE_ORIENTATION, val, exif->pl);
  }

  exif->fdata = data;
  exif_data_foreach_content (d, exif_data_foreach_func, exif);
  exif_data_unref (d);

  return 0;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_exif_register () */
GRABBER_REGISTER (exif,
                  GRABBER_CAP_FLAGS,
                  exif_pl,
                  grabber_exif_priv,
                  grabber_exif_init,
                  grabber_exif_uninit,
                  grabber_exif_grab,
                  NULL)
