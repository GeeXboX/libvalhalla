/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2010 Mathieu Schroeter <mathieu@schroetersa.ch>
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
#include "grabber_chartlyrics.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO

/*
 * The documentation is available on:
 *  http://www.chartlyrics.com/api.aspx
 */

#define CHARTLYRICS_HOSTNAME     "api.chartlyrics.com"

#define CHARTLYRICS_QUERY_GET    "http://%s/apiv1.asmx/SearchLyricDirect?artist=%s&song=%s"

typedef struct grabber_chartlyrics_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_chartlyrics_t;

static const metadata_plist_t chartlyrics_pl[] = {
  { NULL,                             VALHALLA_METADATA_PL_ABOVE    }
};


static int
grabber_chartlyrics_get (grabber_chartlyrics_t *chartlyrics, file_data_t *fdata,
                         const char *artist, const char *song)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int res = -1;

  xmlDocPtr doc;
  xmlNode *n;

  /* proceed with ChartLyrics search and get request */
  snprintf (url, sizeof (url),
            CHARTLYRICS_QUERY_GET, CHARTLYRICS_HOSTNAME, artist, song);

  vh_log (VALHALLA_MSG_VERBOSE, "Search/Get Request: %s", url);

  udata = vh_url_get_data (chartlyrics->handler, url);
  if (udata.status)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search/Get Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    return -1;

  n = xmlDocGetRootElement (doc);

  /* fetch lyrics */
  vh_grabber_parse_str (fdata, n->children, "Lyric", VALHALLA_METADATA_LYRICS,
                        VALHALLA_LANG_EN, chartlyrics->pl);
  res = 0;

  xmlFreeDoc (doc);
  return res;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_chartlyrics_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_chartlyrics_t));
}

static int
grabber_chartlyrics_init (void *priv, const grabber_param_t *param)
{
  grabber_chartlyrics_t *chartlyrics = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!chartlyrics)
    return -1;

  chartlyrics->handler = vh_url_new (param->url_ctl);
  chartlyrics->pl      = param->pl;
  return chartlyrics->handler ? 0 : -1;
}

static void
grabber_chartlyrics_uninit (void *priv)
{
  grabber_chartlyrics_t *chartlyrics = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!chartlyrics)
    return;

  vh_url_free (chartlyrics->handler);
  free (chartlyrics);
}

static int
grabber_chartlyrics_grab (void *priv, file_data_t *data)
{
  grabber_chartlyrics_t *chartlyrics = priv;
  const metadata_t *title = NULL, *author = NULL;
  char *artist, *song;
  int err, res = -3;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

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
  artist = vh_url_escape_string (chartlyrics->handler, author->value);
  if (!artist)
    return -3;
  song = vh_url_escape_string (chartlyrics->handler, title->value);
  if (!song)
    goto out;

  res = grabber_chartlyrics_get (chartlyrics, data, artist, song);

 out:
  free (artist);
  if (song)
    free (song);
  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_chartlyrics_register () */
GRABBER_REGISTER (chartlyrics,
                  GRABBER_CAP_FLAGS,
                  chartlyrics_pl,
                  1000, /* 1 sec */
                  grabber_chartlyrics_priv,
                  grabber_chartlyrics_init,
                  grabber_chartlyrics_uninit,
                  grabber_chartlyrics_grab,
                  NULL)
