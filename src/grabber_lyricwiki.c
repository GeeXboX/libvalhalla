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
#include "grabber_lyricwiki.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "utils.h"
#include "osdep.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_AUDIO

#define MAX_URL_SIZE      1024

#define LYRICWIKI_HOSTNAME     "lyrics.wikia.com"
#define LYRICWIKI_QUERY_SEARCH "http://%s/api.php?func=getSong&artist=%s&song=%s&fmt=xml"

#define LYRICWIKI_BOX_START         "<div class='lyricbox'>"
#define LYRICWIKI_BOX_END           "<p>"

static const char *lyricwiki_offset[][2] = {
  { "<div",     "</div>"    },
  { "<span",    "</span>"   },
};

typedef struct grabber_lyricwiki_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_lyricwiki_t;

static const metadata_plist_t lyricwiki_pl[] = {
  { NULL,                             VALHALLA_METADATA_PL_NORMAL   }
};


static int
grabber_lyricwiki_get (grabber_lyricwiki_t *lyricwiki, file_data_t *fdata,
                       const char *artist, const char *song)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  char *html = NULL;

  xmlDocPtr doc;

  /* proceed with LyricWiki.org search request */
  snprintf (url, MAX_URL_SIZE, LYRICWIKI_QUERY_SEARCH,
            LYRICWIKI_HOSTNAME, artist, song);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (lyricwiki->handler, url);
  if (udata.status != 0)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* fetch lyrics URL */
  vh_xml_search_str (xmlDocGetRootElement (doc), "url", &html);
  xmlFreeDoc (doc);

  if (html)
  {
    char *start, *offset, *end, *txt, *lyrics;
    const char *off = NULL;
    int len;
    unsigned int i, j;
    int cmp;

    udata = vh_url_get_data (lyricwiki->handler, html);
    free (html);
    if (udata.status != 0)
      return -1;

    /* parse HTML page */
    start = strstr (udata.buffer, LYRICWIKI_BOX_START);
    if (!start)
    {
      free (udata.buffer);
      return -1;
    }

    for (i = 0; i < ARRAY_NB_ELEMENTS (lyricwiki_offset) && !off; i++)
    {
      cmp = strncmp (start + strlen (LYRICWIKI_BOX_START),
                     lyricwiki_offset[i][0], strlen (lyricwiki_offset[i][0]));
      if (!cmp)
        off = lyricwiki_offset[i][1];
    }

    if (off)
    {
      offset = strstr (start, off);
      if (!offset)
      {
        free (udata.buffer);
        return -1;
      }
      offset += strlen (off);
    }
    else
      offset = start + strlen (LYRICWIKI_BOX_START);

    end = strstr (offset, LYRICWIKI_BOX_END);
    if (!end)
    {
      free (udata.buffer);
      return -1;
    }

    len = end - offset;
    txt = strndup (offset, len);

    lyrics = calloc (1, len + 1);
    for (i = 0, j = 0; i < (unsigned int) len; i++)
    {
      int n = 0;
      unsigned int c;

      if (txt[i] == '<') /* escape <br /> */
      {
        i += 5;
        lyrics[j] = '\n';
      }
      else if (txt[i] == '&' && sscanf (txt + i, "&#%u%n", &c, &n) >= 1 && n)
      {
        i += n;
        lyrics[j] = (char) c;
      }
      else
        lyrics[j] = txt[i];
      j++;
    }

    vh_metadata_add_auto (&fdata->meta_grabber, VALHALLA_METADATA_LYRICS,
                          lyrics, VALHALLA_LANG_EN, lyricwiki->pl);

    free (txt);
    free (lyrics);
    free (udata.buffer);
  }

  return 0;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_lyricwiki_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_lyricwiki_t));
}

static int
grabber_lyricwiki_init (void *priv, const grabber_param_t *param)
{
  grabber_lyricwiki_t *lyricwiki = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lyricwiki)
    return -1;

  lyricwiki->handler = vh_url_new (param->url_ctl);
  lyricwiki->pl      = param->pl;
  return lyricwiki->handler ? 0 : -1;
}

static void
grabber_lyricwiki_uninit (void *priv)
{
  grabber_lyricwiki_t *lyricwiki = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!lyricwiki)
    return;

  vh_url_free (lyricwiki->handler);
  free (lyricwiki);
}

static int
grabber_lyricwiki_grab (void *priv, file_data_t *data)
{
  grabber_lyricwiki_t *lyricwiki = priv;
  const metadata_t *title = NULL, *author = NULL;
  char *artist, *song;
  int res, err;

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
  artist = vh_url_escape_string (lyricwiki->handler, author->value);
  song = vh_url_escape_string (lyricwiki->handler, title->value);

  res = grabber_lyricwiki_get (lyricwiki, data, artist, song);

  free (artist);
  free (song);

  return res;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_lyricwiki_register () */
GRABBER_REGISTER (lyricwiki,
                  GRABBER_CAP_FLAGS,
                  lyricwiki_pl,
                  0,
                  grabber_lyricwiki_priv,
                  grabber_lyricwiki_init,
                  grabber_lyricwiki_uninit,
                  grabber_lyricwiki_grab,
                  NULL)
