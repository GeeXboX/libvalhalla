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

#include "grabber_common.h"
#include "grabber_lastfm.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "logs.h"
#include "md5.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO

#define MAX_URL_SIZE        1024

#define LASTFM_HOSTNAME     "ws.audioscrobbler.com"
#define LASTFM_LICENSE_KEY  "402d3ca8e9bc9d3cf9b85e1202944ca5"
#define LASTFM_QUERY_SEARCH "http://%s/2.0/?method=album.getinfo&api_key=%s&artist=%s&album=%s"

typedef struct grabber_lastfm_s {
  url_t  *handler;
} grabber_lastfm_t;

static int
grabber_lastfm_get (url_t *handler, file_data_t *fdata,
                    const char *artist, const char *album)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  xmlChar *cv;

  xmlDocPtr doc;

  /* proceed with LyricWiki.org search request */
  memset (url, '\0', MAX_URL_SIZE);
  snprintf (url, MAX_URL_SIZE, LASTFM_QUERY_SEARCH,
            LASTFM_HOSTNAME, LASTFM_LICENSE_KEY, artist, album);

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = url_get_data (handler, url);
  if (udata.status != 0)
    return -1;

  valhalla_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = get_xml_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  cv = get_prop_value_from_xml_tree_by_attr (xmlDocGetRootElement (doc),
                                             "image", "size", "extralarge");
  if (cv)
  {
    char name[1024] = { 0 };
    char *cover;

    snprintf (name, sizeof (name), "%s-%s", artist, album);
    cover = md5sum (name);
    metadata_add (&fdata->meta_grabber, "cover",
                  cover, VALHALLA_META_GRP_MISCELLANEOUS);
    file_dl_add (&fdata->list_downloader,
                 (char *) cv, cover, VALHALLA_DL_COVER);
    free (cover);
    xmlFree (cv);
  }

  xmlFreeDoc (doc);

  return 0;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_lastfm_priv (void)
{
  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_lastfm_t));
}

static int
grabber_lastfm_init (void *priv)
{
  grabber_lastfm_t *lastfm = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lastfm)
    return -1;

  lastfm->handler = url_new ();
  return lastfm->handler ? 0 : -1;
}

static void
grabber_lastfm_uninit (void *priv)
{
  grabber_lastfm_t *lastfm = priv;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lastfm)
    return;

  url_free (lastfm->handler);
  free (lastfm);
}

static int
grabber_lastfm_grab (void *priv, file_data_t *data)
{
  grabber_lastfm_t *lyricwiki = priv;
  metadata_t *album = NULL, *author = NULL;
  char *artist, *alb;
  int res, err;

  valhalla_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = metadata_get (data->meta_parser, "album", 0, &album);
  if (err)
    return -1;

  err = metadata_get (data->meta_parser, "author", 0, &author);
  if (err)
  {
    err = metadata_get (data->meta_parser, "artist", 0, &author);
    if (err)
      return -2;
  }

  if (!album || !author)
    return -3;

  if (!album->value || !author->value)
    return -3;

  /* get HTTP compliant keywords */
  artist = url_escape_string (lyricwiki->handler, author->value);
  alb = url_escape_string (lyricwiki->handler, album->value);

  res = grabber_lastfm_get (lyricwiki->handler, data, artist, alb);

  free (artist);
  free (alb);

  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* grabber_lastfm_register () */
GRABBER_REGISTER (lastfm,
                  GRABBER_CAP_FLAGS,
                  grabber_lastfm_priv,
                  grabber_lastfm_init,
                  grabber_lastfm_uninit,
                  grabber_lastfm_grab,
                  NULL)
