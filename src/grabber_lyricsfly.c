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

#include "grabber_common.h"
#include "grabber_lyricsfly.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO

#define MAX_URL_SIZE      1024

#define LYRICSFLY_HOSTNAME     "lyricsfly.com"
#define LYRICSFLY_LICENSE_KEY  "9e8ca428841df74d2-temporary.API.access"
#define LYRICSFLY_QUERY_SEARCH "http://%s/api/api.php?i=%s&a=%s&t=%s"

typedef struct grabber_lyricsfly_s {
  url_t *handler;
} grabber_lyricsfly_t;

static int
grabber_lyricsfly_get (url_t *handler, file_data_t *fdata,
                       const char *artist, const char *song)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  char *lyrics = NULL;
  xmlDocPtr doc;

  /* proceed with LyricsFly.com search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, LYRICSFLY_QUERY_SEARCH,
            LYRICSFLY_HOSTNAME, LYRICSFLY_LICENSE_KEY, artist, song);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (handler, url);
  if (udata.status != 0)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* fetch lyrics URL */
  vh_xml_search_str (xmlDocGetRootElement (doc), "tx", &lyrics);
  xmlFreeDoc (doc);

  if (lyrics)
  {
    char *ly;
    int i, j = 0;

    ly = calloc (1, strlen (lyrics));
    for (i = 0, j = 0; i < strlen (lyrics); i++, j++)
    {
      if ((lyrics[i] == '[') && (lyrics[i+1] == 'b')
          && (lyrics[i+2] == 'r') && (lyrics[i+3] == ']'))
      {
        i += 3;
        ly[j] = ' ';
        continue;
      }
      ly[j] = lyrics[i];
    }
    vh_metadata_add (&fdata->meta_grabber, "lyrics",
                     ly, VALHALLA_META_GRP_MISCELLANEOUS);
    free (lyrics);
    free (ly);
  }

  return 0;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_lyricsfly_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_lyricsfly_t));
}

static int
grabber_lyricsfly_init (void *priv)
{
  grabber_lyricsfly_t *lyricsfly = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lyricsfly)
    return -1;

  lyricsfly->handler = vh_url_new ();
  return lyricsfly->handler ? 0 : -1;
}

static void
grabber_lyricsfly_uninit (void *priv)
{
  grabber_lyricsfly_t *lyricsfly = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lyricsfly)
    return;

  vh_url_free (lyricsfly->handler);
  free (lyricsfly);
}

static int
grabber_lyricsfly_grab (void *priv, file_data_t *data)
{
  grabber_lyricsfly_t *lyricsfly = priv;
  metadata_t *title = NULL, *author = NULL;
  char *artist, *song;
  int res, err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &title);
  if (err)
    return -1;

  err = vh_metadata_get (data->meta_parser, "author", 0, &author);
  if (err)
  {
    err = vh_metadata_get (data->meta_parser, "artist", 0, &author);
    if (err)
      return -2;
  }

  if (!title || !author)
    return -3;

  if (!title->value || !author->value)
    return -3;

  /* get HTTP compliant keywords */
  artist = vh_url_escape_string (lyricsfly->handler, author->value);
  song = vh_url_escape_string (lyricsfly->handler, title->value);

  res = grabber_lyricsfly_get (lyricsfly->handler, data, artist, song);

  free (artist);
  free (song);

  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_lyricsfly_register () */
GRABBER_REGISTER (lyricsfly,
                  GRABBER_CAP_FLAGS,
                  grabber_lyricsfly_priv,
                  grabber_lyricsfly_init,
                  grabber_lyricsfly_uninit,
                  grabber_lyricsfly_grab,
                  NULL)
