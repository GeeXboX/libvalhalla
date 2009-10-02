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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "grabber_common.h"
#include "grabber_local.h"
#include "metadata.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO | \
  GRABBER_CAP_VIDEO

#define ARRAY_NB_ELEMENTS(array) (sizeof (array) / sizeof (array[0]))

typedef struct grabber_local_s {
  /* unused */
} grabber_local_t;

static char *
grabber_local_get (const char *filename)
{
  char *s, *dir = NULL, *file = NULL, *cv = NULL;
  unsigned int i, j;

  const char *known_filenames[] =
    { "cover", "COVER", "front", "FRONT" };

  const char *known_extensions[] =
    { "jpg", "JPG", "jpeg", "JPEG", "png", "PNG", "tbn", "TBN" };

  if (!filename)
    goto get_end;

  /* retrieve directory name */
  s = strrchr (filename, '/');
  if (!s)
    goto get_end;
  dir = strndup (filename, strlen (filename) - strlen (s));

  /* retrieve file base name */
  s = strrchr (filename, '.');
  if (!s)
    goto get_end;
  file = strndup (filename + strlen (dir) + 1,
                  strlen (filename) - strlen (dir) - strlen (s) - 1);

  /* try to find an exact file match */
  for (i = 0; i < ARRAY_NB_ELEMENTS (known_extensions); i++)
  {
    char cover[1024] = { 0 };

    snprintf (cover, sizeof (cover),
              "%s/%s.%s", dir, file, known_extensions[i]);
    if (vh_file_exists (cover))
    {
      cv = strdup (cover);
      goto get_end;
    }
  }

  /* try to find a generic cover file for the whole directory */
  for (i = 0; i < ARRAY_NB_ELEMENTS (known_extensions); i++)
    for (j = 0; j < ARRAY_NB_ELEMENTS (known_filenames); j++)
    {
      char cover[1024] = { 0 };

      snprintf (cover, sizeof (cover),
                "%s/%s.%s", dir, known_filenames[j], known_extensions[i]);
      if (vh_file_exists (cover))
      {
        cv = strdup (cover);
        goto get_end;
      }
    }

 get_end:
  if (dir)
    free (dir);
  if (file)
    free (file);

  return cv;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_local_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_local_t));
}

static int
grabber_local_init (vh_unused void *priv)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return 0;
}

static void
grabber_local_uninit (void *priv)
{
  grabber_local_t *local = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  free (local);
}

static int
grabber_local_grab (vh_unused void *priv, file_data_t *data)
{
  char *cover;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  cover = grabber_local_get (data->file);
  if (cover)
  {
    vh_metadata_add_auto (&data->meta_grabber, VALHALLA_METADATA_COVER, cover);
    free (cover);
  }

  return 0;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_local_register () */
GRABBER_REGISTER (local,
                  GRABBER_CAP_FLAGS,
                  grabber_local_priv,
                  grabber_local_init,
                  grabber_local_uninit,
                  grabber_local_grab,
                  NULL)
